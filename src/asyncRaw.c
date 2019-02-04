//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2018 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#include "internal_service.h"
#include <redfishRawAsync.h>

#include <string.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "debug.h"
#include "util.h"

static void safeFree(void* ptr);
static void freeHeaders(httpHeader* headers);
static void initAsyncThread(redfishService* service);
static void startAsyncThread(redfishService* service);
static size_t asyncHeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata);
static size_t curlWriteMemory(void *contents, size_t size, size_t nmemb, void *userp);
static size_t curlReadMemory(void *ptr, size_t size, size_t nmemb, void *userp);
static int curlSeekMemory(void *userp, curl_off_t offset, int origin);
static int addHeader(httpHeader** headersPtr, const char* name, const char* value);

asyncHttpRequest* createRequest(const char* url, httpMethod method, size_t bodysize, char* body)
{
    asyncHttpRequest* ret = malloc(sizeof(asyncHttpRequest));
    if(ret)
    {
        ret->url = safeStrdup(url);
        ret->method = method;
        ret->bodySize = bodysize;
        ret->body = body;
        ret->headers = NULL;
    }
    return ret;
}

void addRequestHeader(asyncHttpRequest* request, const char* name, const char* value)
{
    REDFISH_DEBUG_NOTICE_PRINT("%s: Adding %s => %s to %p\n", __FUNCTION__, name, value, request->headers);
    addHeader(&request->headers, name, value);
}

#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strtok_r   strtok_s
#endif

httpHeader* responseGetHeader(asyncHttpResponse* response, const char* name)
{
    httpHeader* current;

    if(response == NULL || response->headers == NULL)
    {
        return NULL;
    }
    current = response->headers;
    do
    {
        if(strcasecmp(current->name, name) == 0)
        {
            return current;
        }
        current = current->next;
    } while(current);
    return NULL;
}

/**
 * @brief A work item for the async queue.
 *
 * An item representing work for the async queue. This is usually a async HTTP request, but could also be a command for the thread.
 */
typedef struct
{
    /** This work item instructs the thread to terminate **/
    bool term;
    /** This is the request to process if term is false **/
    asyncHttpRequest* request;
    /** The callback for the request **/
    asyncRawCallback callback;
    /** The context for the request **/
    void* context;
} asyncWorkItem;

bool startRawAsyncRequest(redfishService* service, asyncHttpRequest* request, asyncRawCallback callback, void* context)
{
    asyncWorkItem* workItem;

    if(service == NULL || request == NULL)
    {
        return false;
    }
    if(service->queue == NULL)
    {
        initAsyncThread(service);
    }

    workItem = malloc(sizeof(asyncWorkItem));
    if(workItem == NULL)
    {
        return false;
    }
    workItem->term = false;
    workItem->request = request;
    workItem->callback = callback;
    workItem->context = context;
    queuePush(service->queue, workItem);
    return true;
}

thread getThreadId()
{
#ifndef _MSC_VER
    return pthread_self();
#else
    return GetCurrentThread();
#endif
}

void terminateAsyncThread(redfishService* service)
{
    asyncWorkItem* workItem;
#ifndef _MSC_VER
    int x;
#endif

    if(service == NULL || service->queue == NULL)
    {
        return;
    }
    workItem = malloc(sizeof(asyncWorkItem));
    if(workItem == NULL)
    {
        return;
    }
    workItem->term = true;
    queuePush(service->queue, workItem);
    if(service->asyncThread == getThreadId())
    {
        REDFISH_DEBUG_INFO_PRINT("%s: Async thread self cleanup...\n", __FUNCTION__);
#ifndef _MSC_VER
        //Need to set this thread detached and make it clean itself up
        pthread_detach(pthread_self());
#endif
        service->selfTerm = true;
    }
    else
    {
        REDFISH_DEBUG_INFO_PRINT("%s: Async thread other thread cleanup...\n", __FUNCTION__);
#ifdef _MSC_VER
        WaitForSingleObject(service->asyncThread, INFINITE);
#else
        x = pthread_join(service->asyncThread, NULL);
        if(x == 35)
        {
            //Workaround for valgrind
            sleep(10);
        }
#endif
        freeQueue(service->queue);
        service->queue = NULL;
    }
}

void freeAsyncRequest(asyncHttpRequest* request)
{
    if(request)
    {
        safeFree(request->url);
        safeFree(request->body);
        freeHeaders(request->headers);
        free(request);
    }
}

void freeAsyncResponse(asyncHttpResponse* response)
{
    if(response)
    {
        safeFree(response->body);
        freeHeaders(response->headers);
        free(response);
    }
}

/**
 * @brief A representation of memory for CURL callbacks.
 *
 * An item representing memory for use in CURL callbacks allowing for movement through the buffer as data is sent/receieved.
 */
struct MemoryStruct
{
  /**
   * @brief The memory pointer
   *
   * On data sent to the server this pointer will be incremented as data is sent and will always point to the next byte to be sent.
   * On data receieved from the server this pointer will always point to the first data byte receieved and be reallocated as needed.
   */
  char* memory;
  /** 
   * @brief The size of the memory region pointed to by memory.
   *
   * On data sent to the server this value will be reduced as the memory pointer is incremented.
   * On data received from the server this value will be increased to represent the total size of the memory pointer 
   */
  size_t size;
  /**
   * @brief The original memory pointer
   *
   * On data sent to the server this pointer will point to the first byte sent allowing the buffer to be freed when complete.
   * This pointer is not used on receive.
   */
  char* origin;
  /**
   * @brief The original size of the memory pointer
   *
   * On data sent to the server this will contain the original value of size. This is used when seeking back in the buffer to resent part of the payload.
   * This pointer is not used on receive.
   */
  size_t originalSize;
};

#ifdef _MSC_VER
threadRet __stdcall rawAsyncWorkThread(void* data)
#else
threadRet rawAsyncWorkThread(void* data)
#endif
{
    redfishService* service = (redfishService*)data;
    queue* q = service->queue;
    asyncWorkItem* workItem = NULL;
    CURL* curl;
    CURLcode res;
    asyncHttpResponse* response;
    struct MemoryStruct readChunk;
    struct MemoryStruct writeChunk;
    struct curl_slist* headers = NULL;
    char headerStr[1024];
    httpHeader* current;
    char* redirect;
    bool noReuse = false;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if(!curl)
    {
#ifdef _MSC_VER
        return 0;
#else
        pthread_exit(NULL);
        return NULL;
#endif
    }
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
    //curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteMemory);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, curlReadMemory);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, asyncHeaderCallback);
    curl_easy_setopt(curl, CURLOPT_SEEKFUNCTION, curlSeekMemory); 
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readChunk);
    curl_easy_setopt(curl, CURLOPT_READDATA, &writeChunk);

    while(queuePop(q, (void**)&workItem) == 0)
    {
        if(workItem->term)
        {
            break;
        }
        //Process workItem
        writeChunk.memory = workItem->request->body;
        writeChunk.size = workItem->request->bodySize;
        writeChunk.origin = writeChunk.memory;
        writeChunk.originalSize = writeChunk.size;
        curl_easy_setopt(curl, CURLOPT_INFILESIZE, writeChunk.size);
        if(workItem->callback)
        {
            response = malloc(sizeof(asyncHttpResponse));
            if(response == NULL)
            {
                workItem->callback(workItem->request, response, workItem->context);
                safeFree(workItem);
                continue;
            }
            response->headers = NULL;
            //If this fails then we just don't get headers returned...
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, response);
            readChunk.memory = (char*)malloc(1);
        }
        else
        {
            response = NULL;
            readChunk.memory = NULL;
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, NULL);
        }
        readChunk.size = 0;
        readChunk.origin = readChunk.memory;
        readChunk.originalSize = readChunk.size;
        current = workItem->request->headers;
        //Make sure it is always NULL terminated
        headerStr[sizeof(headerStr)-1] = 0;
        while(current)
        {
            snprintf(headerStr, sizeof(headerStr)-1, "%s: %s", current->name, current->value);
            headers = curl_slist_append(headers, headerStr);
            current = current->next;
        }
        switch(workItem->request->method)
        {
            case HTTP_GET:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
                curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
                break;
            case HTTP_HEAD:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "HEAD");
                curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
                break;
            case HTTP_POST:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
                curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
                break;
            case HTTP_PUT:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
                curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
                break;
            case HTTP_DELETE:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
                break;
            case HTTP_OPTIONS:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "OPTIONS");
                break;
            case HTTP_PATCH:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
                curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
                break;
        }
        if(noReuse)
        {
            curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, 1L);
            curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
        }
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, workItem->request->timeout);
        curl_easy_setopt(curl, CURLOPT_URL, workItem->request->url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); 
        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &redirect);
        if(redirect)
        {
            REDFISH_DEBUG_INFO_PRINT("%s: Redirect from %s to %s\n", __FUNCTION__, workItem->request->url, redirect);
            if(response)
            {
                safeFree(readChunk.memory);
                readChunk.memory = (char*)malloc(1);
                readChunk.size = 0;
                readChunk.origin = readChunk.memory;
                readChunk.originalSize = readChunk.size;
                freeHeaders(response->headers); 
                response->headers = NULL;
                curl_easy_setopt(curl, CURLOPT_URL, redirect);
                res = curl_easy_perform(curl);
            }
        }
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 0L);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);
        curl_slist_free_all(headers);
        headers = NULL;
        if(response)
        {
            if(res != CURLE_OK)
            {
                REDFISH_DEBUG_ERR_PRINT("%s: CURL returned %d\n", __FUNCTION__, res);
                response->connectError = 1;
                response->httpResponseCode = 0xFFFF;
                response->body = NULL;
                response->bodySize = 0;
                safeFree(readChunk.memory);
            }
            else
            {
                //This particular server version does not handle connection reuse correctly, so don't do it on that server
                current = responseGetHeader(response, "Server");
                if(current && (strcmp(current->value, "Appweb/4.5.4") == 0))
                {
                    noReuse = true;
                }

                response->connectError = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->httpResponseCode);
                REDFISH_DEBUG_NOTICE_PRINT("%s: Got response for url %s with code %ld\n", __FUNCTION__, workItem->request->url, response->httpResponseCode);
                response->body = readChunk.memory;
                response->bodySize = readChunk.size;
            }
            //It is the callback's responsibilty to free request, response, and context...
            workItem->callback(workItem->request, response, workItem->context);
        }
        else
        {
            freeAsyncRequest(workItem->request);
        }
        safeFree(workItem);
    }
    safeFree(workItem);
    curl_easy_cleanup(curl);
    curl_global_cleanup(); //Must be called the same number of times as init...
    if(service->selfTerm)
    {
        freeQueue(service->queue);
        service->queue = NULL;
        free(service);
    }
#ifdef _MSC_VER
    return 0;
#else
    pthread_exit(NULL);
    return NULL;
#endif
}

static void safeFree(void* ptr)
{
    if(ptr)
    {
        free(ptr);
    }
}

static void freeHeaders(httpHeader* headers)
{
    httpHeader* node;
    httpHeader* tmp;
    REDFISH_DEBUG_NOTICE_PRINT("%s: Freeing %p\n", __FUNCTION__, headers);
    if(headers)
    {
        node = headers;
        do
        {
            tmp = node->next;
            safeFree(node->name);
            safeFree(node->value);
            free(node);
            node = tmp;
        } while(tmp);
    }
}

static void initAsyncThread(redfishService* service)
{
    queue* q = newQueue();

    serviceIncRef(service);

    service->queue = q;
    startAsyncThread(service);

    serviceDecRef(service);
}

static void startAsyncThread(redfishService* service)
{
#ifdef _MSC_VER
    service->asyncThread = CreateThread(NULL, 0, rawAsyncWorkThread, service, 0, NULL);
#else
    pthread_create(&(service->asyncThread), NULL, rawAsyncWorkThread, service);
#endif
}

static size_t asyncHeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata)
{
    char* tmp;
    char* name;
    char* value;
    char* save;
    asyncHttpResponse* response = (asyncHttpResponse*)userdata;
    if(response == NULL)
    {
        //Just return that it has been processed...
        return nitems * size;
    }
    //Add an extra item for the null terminator...
    tmp = calloc(nitems+1, size);
    if(tmp == NULL)
    {
        return 0;
    }
    memcpy(tmp, buffer, nitems * size);
    name = strtok_r(tmp, ":", &save);
    if(name == NULL)
    {
        free(tmp);
        return nitems * size;
    }
    value = strtok_r(NULL, ":", &save);
    if(value == NULL)
    {
        free(tmp);
        return nitems * size;
    }
    if(value[0] == 0x20)
    {
        //Skip first character it's a space
        value++;
    }
    save = strchr(value, '\r');
    if(save)
    {
        //Replace \r\n with NULL terminator...
        save[0] = 0;
    }
    REDFISH_DEBUG_NOTICE_PRINT("%s: Adding %s => %s to %p\n", __FUNCTION__, name, value, response->headers);
    addHeader(&(response->headers), name, value);
    free(tmp);
    
    return nitems * size;
}

static size_t curlWriteMemory(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
  void* tmp;

  tmp = (char*)realloc(mem->memory, mem->size + realsize + 1);
  if(tmp == NULL)
  {
      free(mem->memory);
      mem->memory = NULL;
      mem->size = 0;
      return 0;
  }
  mem->memory = tmp;

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

static size_t curlReadMemory(void *ptr, size_t size, size_t nmemb, void *userp)
{
    size_t fullsize = size*nmemb;
    struct MemoryStruct* pooh = (struct MemoryStruct *)userp;

    if(fullsize < 1)
    {
        return 0;
    }

    if(pooh->size > fullsize)
    {
        memcpy(ptr, pooh->memory, fullsize);
        pooh->memory += fullsize;
        pooh->size -= fullsize;
        return fullsize;
    }
    else if(pooh->size)
    {
        memcpy(ptr, pooh->memory, pooh->size);
        pooh->memory += pooh->size;
        fullsize = pooh->size;
        pooh->size = 0;
        return fullsize;
    }

    return 0;                          /* no more data left to deliver */
}

static int curlSeekMemory(void *userp, curl_off_t offset, int origin)
{
    struct MemoryStruct* pooh = (struct MemoryStruct *)userp;
    (void)origin;

    if(pooh == NULL)
    {
        return CURL_SEEKFUNC_CANTSEEK;
    }

    pooh->memory = pooh->origin+offset;
    pooh->size = pooh->originalSize-offset;

    return CURL_SEEKFUNC_OK;
}

static int addHeader(httpHeader** headersPtr, const char* name, const char* value)
{
    httpHeader* headers = *headersPtr;
    httpHeader* current;
    if(headers == NULL)
    {
        *headersPtr = malloc(sizeof(httpHeader));
        current = *headersPtr;
        if(current == NULL)
        {
            return 1;
        }
    }
    else
    {
        current = headers;
        while(current->next)
        {
            current = current->next;
        }
        current->next = malloc(sizeof(httpHeader));
        if(current->next == NULL)
        {
            return 1;
        }
        current = current->next;
    }
    current->name = safeStrdup(name);
    current->value = safeStrdup(value);
    current->next = NULL;
    return 0;
}
