//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2018 Distributed Management Task Force, Inc. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#include "internal_service.h"
#include <redfishRawAsync.h>

#include <string.h>

#include "debug.h"

static void safeFree(void* ptr);
static void freeHeaders(httpHeader* headers);
static void initAsyncThread(redfishService* service);
static thread startAsyncThread(queue* q);
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
        ret->url = strdup(url);
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

typedef struct
{
    bool term;
    asyncHttpRequest* request;
    asyncRawCallback callback;
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

void terminateAsyncThread(redfishService* service)
{
    asyncWorkItem* workItem;

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
#ifdef _MSC_VER
    WaitForSingleObject(service->asyncThread, INFINITE);
#else
    pthread_join(service->asyncThread, NULL);
#endif
    freeQueue(service->queue);
    service->queue = NULL;
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

struct MemoryStruct
{
  char* memory;
  size_t size;
  char* origin;
  size_t originalSize;
};

threadRet rawAsyncWorkThread(void* data)
{
    queue* q = (queue*)data;
    asyncWorkItem* workItem = NULL;
    CURL* curl;
    CURLcode res;
    asyncHttpResponse* response;
    struct MemoryStruct readChunk;
    struct MemoryStruct writeChunk;
    struct curl_slist* headers = NULL;
    char headerStr[1024];
    httpHeader* current;

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
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteMemory);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, curlReadMemory);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, asyncHeaderCallback);
    curl_easy_setopt(curl, CURLOPT_SEEKFUNCTION, curlSeekMemory);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
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
            case GET:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
                curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
                break;
            case HEAD:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "HEAD");
                curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
                break;
            case POST:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
                curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
                break;
            case PUT:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
                curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
                break;
            case DELETE:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
                break;
            case OPTIONS:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "OPTIONS");
                break;
            case PATCH:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
                break;
        }
        curl_easy_setopt(curl, CURLOPT_URL, workItem->request->url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); 
        res = curl_easy_perform(curl);
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
            }
            else
            {
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
    thread threadId = startAsyncThread(q);

    serviceIncRef(service);

    service->queue = q;
    service->asyncThread = threadId;

    serviceDecRef(service);
}

static thread startAsyncThread(queue* q)
{
    thread ret;

#ifdef _MSC_VER
    ret = CreateThread(NULL, 0, rawAsyncWorkThread, q, 0, NULL);
#else
    pthread_create(&ret, NULL, rawAsyncWorkThread, q);
#endif
    return ret;
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
    current->name = strdup(name);
    current->value = strdup(value);
    current->next = NULL;
    return 0;
}
