//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2019 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#include <string.h>
#include <stdbool.h>

#include "internal_service.h"
#include "redfishService.h"
#include "asyncEvent.h"
#include "util.h"
#include "debug.h"
#include "redfishEvent.h"

#ifdef _MSC_VER
#define poll WSAPoll
#else
#include <sys/socket.h>
#include <unistd.h>
#include <sys/poll.h>
#endif

#ifdef HAVE_OPENSSL
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#endif

typedef struct {
    /** The redfishPayload from the event **/
    redfishPayload* event;
    /** The authentication data provided by the caller or null if none **/
    enumeratorAuthentication* auth;
    /** The type of event this is **/
    unsigned int eventType;
    /** user specified value **/
    char* context;
} EventInfo;

/**
 * @brief A representation of event registrations.
 *
 * A structure representing a redfish event registration
 */
typedef struct {
    /** If true, the event actor thread will remove this registration the next time it processes **/
    bool unregister;
    /** The function to call each time an event meeting the requirements is received **/
    redfishEventCallback callback;
    /** The event types for this registration **/
    unsigned int eventTypes;
    /** The context to pass to the callback **/
    char* context;
    /** The service associated with this registration **/
    redfishService* service;
} EventCallbackRegister;

typedef enum {
    WorkItemTermination,
    WorkItemRegistration,
    WorkItemEvent
} EventWorkItemType;

typedef struct {
    EventWorkItemType type;
    union {
        EventCallbackRegister* registration;
        EventInfo* event;
    };
} EventWorkItem;

#ifndef _MSC_VER
//Windows needs this for the thread call syntax to be right
#define WINAPI
#endif

static threadRet WINAPI eventActorTask(void* args);
static threadRet WINAPI sseThread(void* args);
static threadRet WINAPI tcpThread(void* args);
static bool addTerminationToQueue(redfishService* service);
static bool addRegistrationToQueue(redfishService* service, bool unregister, redfishEventCallback callback, unsigned int eventTypes, const char* context);
static bool addEventToQueue(redfishService* service, EventInfo* event, bool copy);
static bool processNewRegistrations(EventCallbackRegister* newReg, queueNode** registrationsPtr);
static size_t gotSSEData(void *contents, size_t size, size_t nmemb, void *userp);
static size_t getRedfishEventInfoFromRawHttp(const char* buffer, redfishService* service, EventInfo** events);
static unsigned int getEventTypeFromEventPayload(redfishPayload* payload);
static void freeWorkItem(EventWorkItem* wi);
static bool doSSERegAsync(redfishService* service, redfishEventRegistration* registration, redfishEventFrontEnd* frontend, redfishEventCallback callback);
static bool doEventPostRegAsync(redfishService* service, redfishEventRegistration* registration, redfishEventFrontEnd* frontend, redfishEventCallback callback);
#ifdef HAVE_OPENSSL
static void initOpenssl();
static void cleanupOpenssl();
static SSL_CTX* createSSLContext();
static int mkSSLCert(X509** x509Ptr, EVP_PKEY** publicKeyPtr);
#endif
#ifndef NO_CZMQ
static void zeroMQActor(zsock_t* pipe, void* args);
#endif

bool registerCallback(redfishService* service, redfishEventCallback callback, unsigned int eventTypes, const char* context)
{
    return addRegistrationToQueue(service, false, callback, eventTypes, context);
}

bool unregisterCallback(redfishService* service, redfishEventCallback callback, unsigned int eventTypes, const char* context)
{
    return addRegistrationToQueue(service, true, callback, eventTypes, context);
}

void startEventThread(redfishService* service)
{
#ifdef _MSC_VER
    service->eventThread = CreateThread(NULL, 0, eventActorTask, service, 0, NULL);
#else
    pthread_create(&(service->eventThread), NULL, eventActorTask, service);
#endif
}

void terminateAsyncEventThread(redfishService* service)
{
    addTerminationToQueue(service);
    if(service->eventThread == getThreadId())
    {
        REDFISH_DEBUG_INFO_PRINT("%s: Event thread self cleanup...\n", __func__);
#ifndef _MSC_VER
        //Need to set this thread detached and make it clean itself up
        pthread_detach(pthread_self());
#endif
        service->eventTerm = true;
        return;
    }
#ifdef _MSC_VER
    WaitForSingleObject(service->eventThread, INFINITE);
#else
    pthread_join(service->eventThread, NULL);
#endif
    freeQueue(service->eventThreadQueue);
    service->eventThreadQueue = NULL;
}

#define SSE_THREAD_PENDING 0
#define SSE_THREAD_ERROR   -1

struct SSEThreadData {
    redfishService* service;
    char* sseUri;
    /** A lock to control access to the condition variable **/
    mutex spinLock;
    /** The condition variable to be signalled on the async call completion **/
    condition waitForIt;
    int threadStatus;
};

struct TCPThreadData {
    redfishService* service;
	SOCKET socket;
    int type;
};

bool startSSEListener(redfishService* service, const char* sseUri)
{
    struct SSEThreadData* data = malloc(sizeof(struct SSEThreadData));
    bool ret = true;
    if(data == NULL)
    {
        return false;
    }
    data->service = service;
    data->sseUri = safeStrdup(sseUri);
    data->threadStatus = SSE_THREAD_PENDING;
    mutex_init(&data->spinLock);
    cond_init(&data->waitForIt);
    //We start out locked...
    mutex_lock(&data->spinLock);
#ifdef _MSC_VER
    service->sseThread = CreateThread(NULL, 0, sseThread, data, 0, NULL);
#else
    pthread_create(&(service->sseThread), NULL, sseThread, data);
#endif
    //Wait for the condition
    cond_wait(&data->waitForIt, &data->spinLock);
    if(data->threadStatus == SSE_THREAD_ERROR)
    {
        ret = false;
    }
    //Figure out if listening actually works...
    mutex_destroy(&data->spinLock);
    cond_destroy(&data->waitForIt);
    free(data);
    return ret;
}

bool startTCPListener(redfishService* service, SOCKET socket, int type)
{
    struct TCPThreadData* data = malloc(sizeof(struct TCPThreadData));
    if(data == NULL)
    {
        return false;
    }
    data->service = service;
    data->socket = socket;
    data->type = type;
#ifdef _MSC_VER
    service->tcpThread = CreateThread(NULL, 0, tcpThread, data, 0, NULL);
#else
    pthread_create(&(service->tcpThread), NULL, tcpThread, data);
#endif
    service->tcpSocket = socket;
    return true;
}

bool startZeroMQListener(redfishService* service)
{
#ifdef NO_CZMQ
    (void)service;
    return false;
#else
    if(!service->zeroMQListener || !zactor_is(service->zeroMQListener))
    {
        service->zeroMQListener = zactor_new(zeroMQActor, service);
        if(!service->zeroMQListener)
        {
            return false;
        }
    }
    //Either already running or just started
    return true;
#endif
}

bool registerForEventsAsync(redfishService* service, redfishEventRegistration* registration, redfishEventFrontEnd* frontend, redfishEventCallback callback)
{
    if(service == NULL)
    {
        return false;
    }

    if(service->eventThreadQueue == NULL)
    {
        service->eventThreadQueue = newQueue();
        if(service->eventThreadQueue == NULL)
        {
            REDFISH_DEBUG_ERR_PRINT("%s: Unable to allocate event queue!\n", __func__);
            return false;
        }
        startEventThread(service);
    }
    registerCallback(service, callback, REDFISH_EVENT_TYPE_ALL, NULL);

    if(registration != NULL)
    {
        registration->context           = safeStrdup(registration->context);
        registration->postBackURI       = safeStrdup(registration->postBackURI);
        registration->postBackInterface = safeStrdup(registration->postBackInterface);
    }
    if(frontend != NULL)
    {
        frontend->socketInterface       = safeStrdup(frontend->socketInterface);
        frontend->socketName            = safeStrdup(frontend->socketName);
    }

    if(registration == NULL || registration->regTypes & REDFISH_REG_TYPE_SSE)
    {
        return doSSERegAsync(service, registration, frontend, callback);
    }
    if(registration->regTypes & REDFISH_REG_TYPE_POST)
    {
        return doEventPostRegAsync(service, registration, frontend, callback);
    }
    return false;
}

static threadRet WINAPI eventActorTask(void* args)
{
    redfishService* service = (redfishService*)args;
    queueNode* registrations = NULL;
    queueNode* current;
    queueNode* prev;
    EventWorkItem* wi;
    bool term = false;
    EventCallbackRegister* reg;
    queue* q = service->eventThreadQueue;

    while(queuePop(q, (void**)&wi) == 0)
    { 
        switch(wi->type)
        {
            default:
                REDFISH_DEBUG_ERR_PRINT("%s: Unknown work item type %d!\n", __func__, wi->type);
#if __GNUC__ >= 7
                //Yes, we want this to fall through...
                __attribute__ ((fallthrough));
#endif
            case WorkItemTermination:
                term = true;
                break;
            case WorkItemRegistration:
                processNewRegistrations(wi->registration, &registrations);
                wi->registration = NULL;//This is cleaned up, or not as appropirate in the prior call...
                break;
            case WorkItemEvent:
                REDFISH_DEBUG_WARNING_PRINT("%s: Got new event %p (registrations = %p)\n", __func__, wi->event, registrations);
                current = registrations;
                while(current)
                {
                    reg = (EventCallbackRegister*)current->value;
                    if(wi->event->eventType & reg->eventTypes)
                    {
                        reg->callback(wi->event->event, wi->event->auth, wi->event->context);
                    }
                    current = current->next;
                }
                break;
        }
        freeWorkItem(wi);
        if(term)
        {
            break;
        }
    }
    REDFISH_DEBUG_WARNING_PRINT("%s: Exiting...\n", __func__);
    current = registrations;
    while(current)
    {
        prev = current;
        reg = (EventCallbackRegister*)current->value;
        if(reg->context)
        {
            free(reg->context);
        }
        free(reg);
        current = current->next;
        free(prev);
    }
    if(service->eventTerm == true)
    {
        freeQueue(service->eventThreadQueue);
        service->eventThreadQueue = NULL;
        free(service);
    }
#ifdef _MSC_VER
    return 0;
#else
    pthread_exit(NULL);
    return NULL;
#endif
}

struct SSEStruct {
  char* memory;
  size_t size;
  char* origin;
  size_t originalSize;
  redfishService* service;
};

static threadRet WINAPI sseThread(void* args)
{
    struct SSEThreadData* data = (struct SSEThreadData*)args;
    CURL* curl;
    CURLcode res;
    struct SSEStruct readChunk;
    struct curl_slist* headers = NULL;
    char headerStr[1024];
    long httpCode;
    char* uri = data->sseUri;

#ifndef _MSC_VER
    //Need to set this thread detached and make it clean itself up
    pthread_detach(pthread_self());
#endif

    curl = curl_easy_init();
    if(!curl)
    {
        free(uri);
#ifdef _MSC_VER
        return 0;
#else
        pthread_exit(NULL);
        return NULL;
#endif
    }
    readChunk.memory = malloc(1);
    readChunk.size = 0;
    readChunk.origin = readChunk.memory;
    readChunk.originalSize = 0;
    readChunk.service = data->service;
    if(data->service->sessionToken)
    {
        snprintf(headerStr, sizeof(headerStr)-1, "X-Auth-Token: %s", data->service->sessionToken);
        headers = curl_slist_append(headers, headerStr);
    }
    else if(data->service->bearerToken)
    {
        snprintf(headerStr, sizeof(headerStr)-1, "Authorization: Bearer %s", data->service->bearerToken);
        headers = curl_slist_append(headers, headerStr);
    }
    else if(data->service->otherAuth)
    {
        snprintf(headerStr, sizeof(headerStr)-1, "Authorization: %s", data->service->otherAuth);
        headers = curl_slist_append(headers, headerStr);
    }
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, gotSSEData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readChunk);
    curl_easy_setopt(curl, CURLOPT_URL, uri);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    res = curl_easy_perform(curl);
    if(res != CURLE_OK)
    {
        REDFISH_DEBUG_ERR_PRINT("%s: CURL returned %d\n", __func__, res);
    }
    REDFISH_DEBUG_ERR_PRINT("%s: Thread is done\n", __func__);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_slist_free_all(headers);
    if(httpCode > 400)
    {
        data->threadStatus = SSE_THREAD_ERROR;
        free(readChunk.memory);
    }
    cond_broadcast(&data->waitForIt);
    curl_easy_cleanup(curl);
    free(uri);
#ifdef _MSC_VER
    return 0;
#else
    pthread_exit(NULL);
    return NULL;
#endif
}

#define EVENT_BUFFER_SIZE 12288

static void listenOpenSSL(struct TCPThreadData* data)
{
#ifdef HAVE_OPENSSL
    SOCKET tmpSock;
    char buffer[EVENT_BUFFER_SIZE];
    size_t eventCount, i;
    EventInfo* events;
    struct pollfd ufds[1];
    int rv;
    int readCount;
#ifdef HAVE_OPENSSL
    int buffPos;
    SSL_CTX* ctx;
    SSL* ssl;
#ifdef _DEBUG
    unsigned long err;
#endif

    initOpenssl();

    ctx = createSSLContext();

    while(1)
    {
        ufds[0].fd = data->socket;
#ifdef _MSC_VER
		ufds[0].events = POLLIN;
#else
        ufds[0].events = POLLIN | POLLNVAL;
#endif
        ufds[0].revents = 0;
        // wait for events on the sockets, 0.5 second timeout
        // This let's the thread error out when the socket is closed and no events are received... 
        rv = poll(ufds, 1, 500);
        if(rv < 0)
        {
            SSL_CTX_free(ctx);
            cleanupOpenssl();
#ifdef _MSC_VER
			rv = WSAGetLastError();
			REDFISH_DEBUG_CRIT_PRINT("%s: WSAPoll returned %d\n", __FUNCTION__, rv);
#endif
            return;
        }
        else if(rv == 0)
        {
            //Timeout...
            continue;
        }
        if(ufds[0].revents & POLLNVAL)
        {
            REDFISH_DEBUG_ERR_PRINT("Poll found an error with the socket. Exit thread. %x\n", ufds[0].revents);
            SSL_CTX_free(ctx);
            cleanupOpenssl();
            return;
        }
        tmpSock = accept(data->socket, NULL, NULL);
        if(tmpSock < 0)
        {
            REDFISH_DEBUG_ERR_PRINT("%s: Unable to open client socket!\n", __func__);
            SSL_CTX_free(ctx);
            cleanupOpenssl();
            return;
        }
        ssl = SSL_new(ctx);
        SSL_set_fd(ssl, tmpSock);
        if(SSL_accept(ssl) <= 0)
        {
#ifdef _DEBUG
            err = ERR_get_error();
            REDFISH_DEBUG_ERR_PRINT("%s: Unable to complete TLS handshake %u %s\n", __func__, err, ERR_error_string(err, NULL));
#endif
            SSL_free(ssl);
            close(tmpSock);
            continue;
        }
#endif
        memset(buffer, 0, sizeof(buffer));
        buffPos = 0;
        while((unsigned int)buffPos < sizeof(buffer)-1)
        {
            readCount = SSL_read(ssl, buffer+buffPos, (EVENT_BUFFER_SIZE-1)-buffPos);
            //REDFISH_DEBUG_INFO_PRINT("%s: SSL_read returned %d bytes\n", __func__, readCount);
            //REDFISH_DEBUG_INFO_PRINT("%s: Current Buffer is %s\n", __func__, buffer);
            if(readCount < 0)
            {
#ifdef _DEBUG
                err = ERR_get_error();
                REDFISH_DEBUG_ERR_PRINT("%s: Unable to complete SSL read %u %s\n", __func__, err, ERR_error_string(err, NULL));
#endif
                SSL_free(ssl);
                close(tmpSock);
                tmpSock = -1;
                break;
            }
            else if(readCount == 0)
            {
                break;
            }
            buffPos += readCount;
        }
        if(tmpSock == -1)
        {
            continue;
        }
        eventCount = getRedfishEventInfoFromRawHttp(buffer, data->service, &events);
        if(eventCount)
        {
            SSL_write(ssl,"HTTP/1.1 200 OK\nConnection: Closed\n\n",36);
        }
        else
        {
            REDFISH_DEBUG_ERR_PRINT("%s: Unrecognized payload!\n", __func__);
            SSL_write(ssl,"HTTP/1.1 400 Bad Request\nConnection: Closed\n\n",45);
        }
        SSL_free(ssl);
        socketClose(tmpSock);
        for(i = 0; i < eventCount; i++)
        {
            addEventToQueue(data->service, &(events[i]), true);
        }
        free(events);
    }
    SSL_CTX_free(ctx);
    cleanupOpenssl();
#else
    //Can't do it without OpenSSL compiled in
    return;
#endif
}

static void listenTCP(struct TCPThreadData* data)
{
    SOCKET tmpSock;
    char buffer[EVENT_BUFFER_SIZE];
    size_t eventCount, i;
    EventInfo* events;
    struct pollfd ufds[1];
    int rv;
    int readCount;

    while(1)
    {
        ufds[0].fd = data->socket;
#ifdef _MSC_VER
		ufds[0].events = POLLIN;
#else
        ufds[0].events = POLLIN | POLLNVAL;
#endif
        ufds[0].revents = 0;
        // wait for events on the sockets, 0.5 second timeout
        // This let's the thread error out when the socket is closed and no events are received... 
        rv = poll(ufds, 1, 500);
        if(rv < 0)
        {
#ifdef _MSC_VER
			rv = WSAGetLastError();
			REDFISH_DEBUG_CRIT_PRINT("%s: WSAPoll returned %d\n", __FUNCTION__, rv);
#endif
            return;
        }
        else if(rv == 0)
        {
            //Timeout...
            continue;
        }
        if(ufds[0].revents & POLLNVAL)
        {
            REDFISH_DEBUG_ERR_PRINT("Poll found an error with the socket. Exit thread. %x\n", ufds[0].revents);
            return;
        }
        tmpSock = accept(data->socket, NULL, NULL);
        if(tmpSock < 0)
        {
            REDFISH_DEBUG_ERR_PRINT("%s: Unable to open client socket!\n", __func__);
            return;
        }
        memset(buffer, 0, sizeof(buffer));
        readCount = recv(tmpSock, buffer, (EVENT_BUFFER_SIZE-1), 0);
        //REDFISH_DEBUG_INFO_PRINT("%s: recv returned %d bytes\n", __func__, readCount);
        //REDFISH_DEBUG_INFO_PRINT("%s: Current Buffer is %s\n", __func__, buffer);
        if(readCount >= (EVENT_BUFFER_SIZE-1))
        {
            //Too large!
            REDFISH_DEBUG_ERR_PRINT("%s: Event payload is too large for buffer!\n", __func__);
            send(tmpSock,"HTTP/1.1 413 Request Entity Too Large\nConnection: Closed\n\n",58, 0);
            socketClose(tmpSock);
            tmpSock = -1;
        }
        if(tmpSock == -1)
        {
            continue;
        }
        eventCount = getRedfishEventInfoFromRawHttp(buffer, data->service, &events);
        if(eventCount)
        {
            send(tmpSock,"HTTP/1.1 200 OK\nConnection: Closed\n\n",36, 0);
        }
        else
        {
            REDFISH_DEBUG_ERR_PRINT("%s: Unrecognized payload!\n", __func__);
			send(tmpSock,"HTTP/1.1 400 Bad Request\nConnection: Closed\n\n",45, 0);
        }
        socketClose(tmpSock);
        for(i = 0; i < eventCount; i++)
        {
            addEventToQueue(data->service, &(events[i]), true);
        }
        free(events);
    }
}

static threadRet WINAPI tcpThread(void* args)
{
    struct TCPThreadData* data = (struct TCPThreadData*)args;

    if(data->type == CONNECT_TYPE_ANY)
    {
#ifdef HAVE_OPENSSL
        listenOpenSSL(data);
#else
        listenTCP(data);
#endif
    }
    else if(data->type == CONNECT_TYPE_SSL)
    {
#ifdef HAVE_OPENSSL
        REDFISH_DEBUG_CRIT_PRINT("%s: OpenSSL socket requested without OpenSSL compiled in\n", __func__);
#else
        listenOpenSSL(data);
#endif
    }
    else if(data->type == CONNECT_TYPE_TCP)
    {
        listenTCP(data);
    }
    else
    {
        REDFISH_DEBUG_CRIT_PRINT("%s: Unknown socket type %d requested\n", __func__, data->type);
    }
    free(args);
#ifdef _MSC_VER
    return 0;
#else
    pthread_exit(NULL);
    return NULL;
#endif
}

static bool addWorkItemToQueue(redfishService* service, EventWorkItem* wi)
{
    if(queuePush(service->eventThreadQueue, wi) != 0)
    {
        return false;
    }
    return true;
}

static bool addTerminationToQueue(redfishService* service)
{
    bool ret;
    EventWorkItem* wi = malloc(sizeof(EventWorkItem));
    if(wi == NULL)
    {
        return false;
    }
    wi->type = WorkItemTermination;
    ret = addWorkItemToQueue(service, wi);
    if(ret == false)
    {
        free(wi);
    }
    return ret;
}

static bool addRegistrationToQueue(redfishService* service, bool unregister, redfishEventCallback callback, unsigned int eventTypes, const char* context)
{
    bool ret;
    EventWorkItem* wi = malloc(sizeof(EventWorkItem));

    if(wi == NULL)
    {
        return false;
    }
    wi->type = WorkItemRegistration;
    wi->registration = malloc(sizeof(EventCallbackRegister));
    if(!wi->registration)
    {
        free(wi);
        return false;
    }
    wi->registration->unregister = unregister;
    wi->registration->callback = callback;
    wi->registration->eventTypes = eventTypes;
    wi->registration->context = safeStrdup(context);
    wi->registration->service = service;

    ret = addWorkItemToQueue(service, wi);
    if(ret == false)
    {
        if(wi->registration->context)
        {
            free(wi->registration->context);
        }
        free(wi->registration);
        free(wi);
    }
    return ret;
}

static bool addEventToQueue(redfishService* service, EventInfo* event, bool copy)
{
    bool ret;
    EventWorkItem* wi = malloc(sizeof(EventWorkItem));

    if(wi == NULL)
    {
        return false;
    }
    wi->type = WorkItemEvent;
    if(copy == false)
    {
        wi->event = event;
    }
    else
    {
        wi->event = malloc(sizeof(EventInfo));
        if(wi->event == NULL)
        {
            free(wi);
            return false;
        }
        memcpy(wi->event, event, sizeof(EventInfo));
    }

    ret = addWorkItemToQueue(service, wi);
    if(ret == false)
    {
        if(wi->event->context)
        {
            free(wi->event->context);
        }
        cleanupPayload(wi->event->event);
        free(wi->event);
        free(wi);
    }
    return ret;
}

static bool processNewRegistrations(EventCallbackRegister* newReg, queueNode** registrationsPtr)
{
    EventCallbackRegister* oldReg;
    queueNode* current;
    queueNode* prev = NULL;

    if(newReg->unregister == true)
    {
        //Remove the registration from the list...
        current = *registrationsPtr;
        while(current)
        {
            oldReg = (EventCallbackRegister*)current->value;
            if(oldReg->callback == newReg->callback)
            {
                if(newReg->context == NULL || oldReg->context == NULL || strcmp(newReg->context, oldReg->context) == 0)
                {
                    if(current == *registrationsPtr)
                    {
                        *registrationsPtr = current->next;
                    }
                    else
                    {
                        prev->next = current->next;
                    }
                    if(oldReg->context)
                    {
                        free(oldReg->context);
                    }
                    free(current->value);
                    free(current);
                    break;
                }
            }
            prev = current;
            current = current->next;
        }
        free(newReg);
    }
    else
    {
        //Add the registration to the list...
        if(*registrationsPtr == NULL)
        {
            *registrationsPtr = calloc(1, sizeof(queueNode));
            if(*registrationsPtr)
            {
                (*registrationsPtr)->value = newReg;
            }
        }
        else
        {
            current = *registrationsPtr;
            while(current->next)
            {
                current = current->next;
            }
            current->next = calloc(1, sizeof(queueNode));
            if(current->next)
            {
                current->next->value = newReg;
            }
        } 
    }
    return true;
}

static size_t gotSSEData(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct SSEStruct *mem = (struct SSEStruct *)userp;
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

  REDFISH_DEBUG_ERR_PRINT("SSE data: %s\n", mem->memory);

  return realsize;
}

static size_t getRedfishEventInfoFromPayload(redfishPayload* payload, enumeratorAuthentication* auth, EventInfo** events)
{
    EventInfo* ret;
    redfishPayload* contextPayload;
    redfishPayload* eventArray;
    char* contextStr;
    size_t count, i;

    contextPayload = getPayloadByNodeName(payload, "Context");
    if(contextPayload)
    {
        contextStr = getPayloadStringValue(contextPayload);
        cleanupPayload(contextPayload);
    }
    else
    {
        contextStr = NULL;
    }
    eventArray = getPayloadByNodeName(payload, "Events");
    cleanupPayload(payload);
    if(eventArray == NULL)
    {
        if(contextStr)
        {
            free(contextStr);
        }
        *events = NULL;
        return 0;
    }
    count = json_array_size(eventArray->json);
    ret = calloc(count, sizeof(EventInfo));
    if(ret == NULL)
    {
        if(contextStr)
        {
            free(contextStr);
        }
        cleanupPayload(eventArray);
        *events = NULL;
        return 0;
    }
    *events = ret;
    for(i = 0; i < count; i++)
    {
        ret[i].event = getPayloadByIndex(eventArray, i);
        ret[i].auth = auth;
        ret[i].context = safeStrdup(contextStr);
        ret[i].eventType = getEventTypeFromEventPayload(ret[i].event);
    }
    cleanupPayload(eventArray);
    if(contextStr)
    {
        free(contextStr);
    }
    return count;
}

static size_t getRedfishEventInfoFromRawHttp(const char* buffer, redfishService* service, EventInfo** events)
{
    const char* bodyStart;
    redfishPayload* payload;

    //REDFISH_DEBUG_DEBUG_PRINT("%s: Got Payload: %s\n", __func__, buffer);

    if(strncmp(buffer, "POST", 4) != 0)
    {
        REDFISH_DEBUG_WARNING_PRINT("Recieved non-POST to event URI: %s\n", buffer);
        *events = NULL;
        return 0;
    }
    bodyStart = strstr(buffer, "\r\n\r\n");
    if(bodyStart == NULL)
    {
        REDFISH_DEBUG_WARNING_PRINT("Recieved POST with no body: %s\n", buffer);
        *events = NULL;
        return 0;
    }
    bodyStart+=4;
    payload = createRedfishPayloadFromString(bodyStart, service);
    //TODO Auth...
    return getRedfishEventInfoFromPayload(payload, NULL, events);
}

static unsigned int getEventTypeFromEventPayload(redfishPayload* payload)
{
    redfishPayload* eventTypePayload = getPayloadByNodeName(payload, "EventType");
    char* str;
    unsigned int ret = 0;
    if(eventTypePayload == NULL)
    {
        return 0;
    }
    str = getPayloadStringValue(eventTypePayload);
    cleanupPayload(eventTypePayload);
    if(str == NULL)
    {
        return 0;
    }
    if(strcmp(str, "StatusChange") == 0)
    {
        ret = REDFISH_EVENT_TYPE_STATUSCHANGE;
    }
    else if(strcmp(str, "ResourceUpdated") == 0)
    {
        ret = REDFISH_EVENT_TYPE_RESOURCEUPDATED;
    }
    else if(strcmp(str, "ResourceAdded") == 0)
    {
        ret = REDFISH_EVENT_TYPE_RESOURCEADDED;
    }
    else if(strcmp(str, "ResourceRemoved") == 0)
    {
        ret = REDFISH_EVENT_TYPE_RESOURCEREMOVED;
    }
    else if(strcmp(str, "Alert") == 0)
    {
        ret = REDFISH_EVENT_TYPE_ALERT;
    }
    free(str);
    return ret;
}

static void freeWorkItem(EventWorkItem* wi)
{
    if(wi)
    {
        if(wi->type == WorkItemRegistration)
        {
            if(wi->registration)
            {
                if(wi->registration->context)
                {
                    free(wi->registration->context);
                }
                free(wi->registration);
            }
        }
        else if(wi->type == WorkItemEvent)
        {
            if(wi->event)
            {
                cleanupPayload(wi->event->event);
                if(wi->event->auth)
                {
                    free(wi->event->auth);
                }
                if(wi->event->context)
                {
                    free(wi->event->context);
                }
                free(wi->event);
            }
        }
        free(wi);
    }
}

typedef struct
{
    redfishService* service;
    redfishEventRegistration* registration;
    redfishEventFrontEnd* frontend;
    redfishEventCallback callback;
} regStruct;

static void safeFree(void* ptr)
{
    if(ptr)
    {
        free(ptr);
    }
}

static void gotSSEUri(bool success, unsigned short httpCode, redfishPayload* payload, void* context)
{
    bool tmp;
    regStruct* regContext = (regStruct*)context;
    char* uri;

    (void)httpCode;

    if(success == false)
    {
        if(payload)
        {
            cleanupPayload(payload);
        }
        if(regContext->registration && regContext->registration->regTypes & REDFISH_REG_TYPE_POST)
        {
            tmp = doEventPostRegAsync(regContext->service, regContext->registration, regContext->frontend, regContext->callback);
            if(tmp == false)
            {
                //Tell the caller that we didn't register...
                regContext->callback(NULL, NULL, NULL);
            }
        }
        else
        {
            //Tell the caller that we didn't register...
            regContext->callback(NULL, NULL, NULL);
        }
        free(regContext);
        return;
    }
    uri = getPayloadStringValue(payload);
    startSSEListener(regContext->service, uri);
    free(uri);
    cleanupPayload(payload);
    if(regContext->registration)
    {
        safeFree(regContext->registration->context);
        safeFree(regContext->registration->postBackURI);
        safeFree(regContext->registration->postBackInterface);
    }
    if(regContext->frontend)
    {
        safeFree(regContext->frontend->socketInterface);
        safeFree(regContext->frontend->socketName);
    }
    free(regContext);
}

static char* getIP(int ipType, const char* interfaceName)
{
    if(ipType == REDFISH_REG_IP_TYPE_4)
    {
        return getIpv4Address(interfaceName);
    }
    else if(ipType == REDFISH_REG_IP_TYPE_6)
    {
        return getIpv6Address(interfaceName);
    }
    else
    {
        return NULL;
    }
}
#ifndef _MSC_VER
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
static char* getDestStringForReg(redfishEventRegistration* registration)
{
    char* ip = getIP(registration->postBackInterfaceIPType, registration->postBackInterface);
    char ip6tmp[100];
    char destUri[512];

    if(registration->postBackInterfaceIPType == REDFISH_REG_IP_TYPE_4)
    {
        snprintf(destUri, sizeof(destUri)-1, registration->postBackURI, ip);
    }
    else if(registration->postBackInterfaceIPType == REDFISH_REG_IP_TYPE_6)
    {
        snprintf(ip6tmp, sizeof(ip6tmp)-1, "[%s]", ip);
        snprintf(destUri, sizeof(destUri)-1, registration->postBackURI, ip6tmp);
    }
    else
    {
        REDFISH_DEBUG_ERR_PRINT("%s: Unknown IP type %d\n", __func__, registration->postBackInterfaceIPType);
        return NULL;
    }
    free(ip);
    return safeStrdup(destUri);
}
#ifndef _MSC_VER
#pragma GCC diagnostic warning "-Wformat-nonliteral"
#endif

static redfishPayload* getPayloadForSubscription(redfishService* service, redfishEventRegistration* registration)
{
    redfishPayload* subPayload;
    char* destStr;
    json_t* typeArray;

    subPayload = createEmptyRedfishPayload(service);
    if(subPayload)
    {
        if(strstr(registration->postBackURI, "%s"))
        {
            destStr = getDestStringForReg(registration);
            if(destStr == NULL)
            {
                cleanupPayload(subPayload);
                return NULL;
            }
            setPayloadStringByName(subPayload, "Destination", destStr);
            free(destStr);
        }
        else
        {
            setPayloadStringByName(subPayload, "Destination", registration->postBackURI);
        }
        if(registration->context)
        {
            setPayloadStringByName(subPayload, "Context", registration->context);
        }
        setPayloadStringByName(subPayload, "Protocol", "Redfish");
        typeArray = json_array();
        addStringToJsonArray(typeArray, "StatusChange");
        addStringToJsonArray(typeArray, "ResourceUpdated");
        addStringToJsonArray(typeArray, "ResourceAdded");
        addStringToJsonArray(typeArray, "ResourceRemoved");
        addStringToJsonArray(typeArray, "Alert");
        setPayloadElementByName(subPayload, "EventTypes", typeArray);
        json_decref(typeArray);
    } 
    return subPayload;
}

static void postSubDone(bool success, unsigned short httpCode, redfishPayload* payload, void* context)
{
    regStruct* regContext = (regStruct*)context;

    (void)httpCode;

    regContext->service->eventRegistrationUri = getPayloadUri(payload);
    cleanupPayload(payload);
    if(success == false)
    {
        //Tell the caller that we didn't register...
        regContext->callback(NULL, NULL, NULL);
        free(regContext);
        return;
    }
    free(context);
}

static void gotPostSubUri(bool success, unsigned short httpCode, redfishPayload* payload, void* context)
{
    bool tmp;
    regStruct* regContext = (regStruct*)context;
    redfishPayload* subPayload;
    SOCKET socket;
    char* ip;

    (void)httpCode;

    if(success == false)
    {
        //Tell the caller that we didn't register...
        regContext->callback(NULL, NULL, NULL);
        cleanupPayload(payload);
        if(regContext->registration)
        {
            safeFree(regContext->registration->context);
            safeFree(regContext->registration->postBackURI);
            safeFree(regContext->registration->postBackInterface);
        }
        if(regContext->frontend)
        {
            safeFree(regContext->frontend->socketInterface);
            safeFree(regContext->frontend->socketName);
        }
        free(regContext);
        return;
    }
    subPayload = getPayloadForSubscription(regContext->service, regContext->registration);
    if(subPayload == NULL)
    {
        //Tell the caller that we didn't register...
        regContext->callback(NULL, NULL, NULL);
        cleanupPayload(payload);
        if(regContext->registration)
        {
            safeFree(regContext->registration->context);
            safeFree(regContext->registration->postBackURI);
            safeFree(regContext->registration->postBackInterface);
        }
        if(regContext->frontend)
        {
            safeFree(regContext->frontend->socketInterface);
            safeFree(regContext->frontend->socketName);
        }
        free(regContext);
        return;
    }

    if(regContext->frontend == NULL)
    {
        REDFISH_DEBUG_ERR_PRINT("%s: No frontend provided\n", __func__);
        //Tell the caller that we didn't register...
        regContext->callback(NULL, NULL, NULL);
        cleanupPayload(subPayload);
        cleanupPayload(payload);
        if(regContext->registration)
        {
            safeFree(regContext->registration->context);
            safeFree(regContext->registration->postBackURI);
            safeFree(regContext->registration->postBackInterface);
        }
        free(regContext);
        return;
    }

    tmp = postPayloadAsync(payload, subPayload, NULL, postSubDone, context);
    if(regContext->registration)
    {
        safeFree(regContext->registration->context);
        safeFree(regContext->registration->postBackURI);
        safeFree(regContext->registration->postBackInterface);
    }
    cleanupPayload(subPayload);
    cleanupPayload(payload);
    if(tmp)
    {
        switch(regContext->frontend->frontEndType)
        {
            case REDFISH_EVENT_FRONT_END_OPEN_SOCKET:
                startTCPListener(regContext->service, regContext->frontend->socket, CONNECT_TYPE_ANY);
                break;
            case REDFISH_EVENT_FRONT_END_TCP_SOCKET:
                ip = getIP(regContext->frontend->socketIPType, regContext->frontend->socketInterface);
                socket = getSocket(ip, &regContext->frontend->socketPort);
                startTCPListener(regContext->service, socket, CONNECT_TYPE_TCP);
                break;
            case REDFISH_EVENT_FRONT_END_SSL_SOCKET:
                ip = getIP(regContext->frontend->socketIPType, regContext->frontend->socketInterface);
                socket = getSocket(ip, &regContext->frontend->socketPort);
                startTCPListener(regContext->service, socket, CONNECT_TYPE_SSL);
                break;
            case REDFISH_EVENT_FRONT_END_DOMAIN_SOCKET:
                socket = getDomainSocket(regContext->frontend->socketName);
                startTCPListener(regContext->service, socket, CONNECT_TYPE_TCP);
                break;
            default:
                REDFISH_DEBUG_ERR_PRINT("%s: Unknown frontend type %d\n", __func__, regContext->frontend->frontEndType);
                break;
        }
    }
    safeFree(regContext->frontend->socketInterface);
    safeFree(regContext->frontend->socketName);
}

static bool doSSERegAsync(redfishService* service, redfishEventRegistration* registration, redfishEventFrontEnd* frontend, redfishEventCallback callback)
{
    bool tmp;
    regStruct* context = malloc(sizeof(regStruct));

    if(context == NULL)
    {
        return false;
    }
    context->service = service;
    context->registration = registration;
    context->frontend = frontend;
    context->callback = callback;

    tmp = getPayloadByPathAsync(service, "/EventService/ServerSentEventUri", NULL, gotSSEUri, context);
    if(tmp == false)
    {
        tmp = doEventPostRegAsync(service, registration, frontend, callback);
    }
    
    return tmp;
}

static bool doEventPostRegAsync(redfishService* service, redfishEventRegistration* registration, redfishEventFrontEnd* frontend, redfishEventCallback callback)
{
    bool tmp;
    regStruct* context = malloc(sizeof(regStruct));

    if(context == NULL)
    {
        return false;
    }
    context->service = service;
    context->registration = registration;
    context->frontend = frontend;
    context->callback = callback;

    tmp = getPayloadByPathAsync(service, "/EventService/Subscriptions", NULL, gotPostSubUri, context);
    if(tmp == false)
    {
        if(registration)
        {
            safeFree(registration->context);
            safeFree(registration->postBackURI);
            safeFree(registration->postBackInterface);
        }
        if(frontend)
        {
            safeFree(frontend->socketInterface);
            safeFree(frontend->socketName);
        }
        free(context);
    }

    return tmp;
}

#ifdef HAVE_OPENSSL
static bool openSSLInited = false;

static void initOpenssl()
{ 
    if(openSSLInited == false)
    {
        SSL_load_error_strings();	
        OpenSSL_add_ssl_algorithms();
        openSSLInited = true;
    }
}

static void cleanupOpenssl()
{
    EVP_cleanup();
    openSSLInited = false;
}

static SSL_CTX* createSSLContext()
{
    const SSL_METHOD* method;
    SSL_CTX* ctx;
    X509* cert;
    EVP_PKEY* key;

    method = SSLv23_server_method();

    ctx = SSL_CTX_new(method);
    if(!ctx)
    {
        REDFISH_DEBUG_ERR_PRINT("%s: Unable to create SSL context\n", __func__);
        return NULL;
    }
    SSL_CTX_set_ecdh_auto(ctx, 1);

    mkSSLCert(&cert, &key);

    /* Set the key and cert */
    if(SSL_CTX_use_certificate(ctx, cert) <= 0)
    {
        ERR_print_errors_fp(stderr);
        return NULL;
    }
    X509_free(cert);

    if (SSL_CTX_use_PrivateKey(ctx, key) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        return NULL;
    }
    EVP_PKEY_free(key);
    return ctx;
}

static int mkSSLCert(X509** x509Ptr, EVP_PKEY** publicKeyPtr)
{
    X509* cert;
    RSA* rsa;
    X509_NAME* name=NULL;
    BIGNUM* e;

    *publicKeyPtr = EVP_PKEY_new();
    cert = X509_new();
    rsa = RSA_new();

#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
    e = BN_new();
#else
    e = malloc(sizeof(BIGNUM));
    BN_init(e);
#endif
    BN_set_word(e, 17);

    RSA_generate_key_ex(rsa, 2048, e, NULL);
    EVP_PKEY_assign_RSA((*publicKeyPtr), rsa);
    BN_free(e);
#if OPENSSL_VERSION_NUMBER < 0x1010000fL
    free(e);
#endif
    //RSA_free(rsa);

    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 0);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), (long)315360000); //10 years...
    X509_set_pubkey(cert, (*publicKeyPtr));

    name=X509_get_subject_name(cert);

    X509_NAME_add_entry_by_txt(name,"C", MBSTRING_ASC, (unsigned char*)"US", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name,"CN", MBSTRING_ASC, (unsigned char*)"DMTF", -1, -1, 0);

    /* Its self signed so set the issuer name to be the same as the
     * subject.
     */
    X509_set_issuer_name(cert, name);

    X509_sign(cert, (*publicKeyPtr), EVP_md5());

    *x509Ptr = cert;
    return 0;
}
#endif

#ifndef NO_CZMQ
static int zeroMQEventReceivedCallback(zloop_t* loop, zsock_t* reader, void* arg)
{
    redfishService* service = (redfishService*)arg;
    char* msg;
    char* body;
    enumeratorAuthentication* auth = NULL;
    size_t eventCount, i;
    EventInfo* events;
    redfishPayload* payload;

    (void)loop;

    if(!service || !service->eventThreadQueue)
    {
        return -1;
    }

    msg = zstr_recv(reader);
    if(!msg)
    {
        return 0;
    }

    body = strstr(msg, "\n\n");
    if(!body)
    {
        free(msg);
        return 0;
    }
    payload = createRedfishPayloadFromString(body, service);
    eventCount = getRedfishEventInfoFromPayload(payload, NULL, &events);
    for(i = 0; i < eventCount; i++)
    {
        addEventToQueue(service, &(events[i]), true);
    }
    free(events); 
    if(auth)
    {
        free(auth);
    }
    free(msg);
    return 0;
}

static int zeroMQThreadSocketListener(zloop_t* loop, zsock_t* reader, void* arg)
{
    zmsg_t* msg = zmsg_recv(reader);
    char* command;
    int rc = 0;

    (void)loop;
    (void)arg;

    if(msg == NULL)
    {
        //No message on socket something is wrong, terminate...
        return -1;
    }
    command = zmsg_popstr(msg);
    if(strcmp(command, "$TERM") == 0)
    {
        //Told to terminate
        rc = -1;
    }
    free(command);
    zmsg_destroy(&msg);

    return rc;
}

static void zeroMQActor(zsock_t* pipe, void* args)
{
    redfishService* service = (redfishService*)args;
    zloop_t* loop;
    zsock_t* remote;

    //Unblock caller
    zsock_signal(pipe, 0);

    loop = zloop_new();
    if(!loop)
    { 
        return;
    }
    //zloop_set_verbose(loop, true);
    zloop_reader(loop, pipe, zeroMQThreadSocketListener, NULL);

    //Open 0mq socket for redfish events
    remote = REDFISH_EVENT_0MQ_LIBRARY_NEW_SOCK;
    if(!remote)
    {
        zloop_destroy(&loop);
        return;
    }

    zloop_reader(loop, remote, zeroMQEventReceivedCallback, service);

    zloop_start(loop);

    zloop_destroy(&loop);
    zsock_destroy(&remote);
}
#endif
/* vim: set tabstop=4 shiftwidth=4 ff=unix expandtab: */
