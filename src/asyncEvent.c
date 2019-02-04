//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2019 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#include <string.h>

#include "internal_service.h"
#include "asyncEvent.h"
#include "util.h"
#include "debug.h"

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

static threadRet eventActorTask(void* args);
static threadRet sseThread(void* args);
static threadRet tcpThread(void* args);
static bool addTerminationToQueue(redfishService* service);
static bool addRegistrationToQueue(redfishService* service, bool unregister, redfishEventCallback callback, unsigned int eventTypes, const char* context);
static bool addEventToQueue(redfishService* service, EventInfo* event, bool copy);
static bool processNewRegistrations(EventCallbackRegister* newReg, queueNode** registrationsPtr);
static size_t gotSSEData(void *contents, size_t size, size_t nmemb, void *userp);
static size_t getRedfishEventInfoFromRawHttp(const char* buffer, redfishService* service, EventInfo** events);
static unsigned int getEventTypeFromEventPayload(redfishPayload* payload);
static void freeWorkItem(EventWorkItem* wi);
#ifdef HAVE_OPENSSL
static void initOpenssl();
static void cleanupOpenssl();
static SSL_CTX* createSSLContext();
static int mkSSLCert(X509** x509Ptr, EVP_PKEY** publicKeyPtr);
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
        REDFISH_DEBUG_INFO_PRINT("%s: Event thread self cleanup...\n", __FUNCTION__);
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
    int socket;
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

bool startTCPListener(redfishService* service, int socket)
{
    struct TCPThreadData* data = malloc(sizeof(struct TCPThreadData));
    if(data == NULL)
    {
        return false;
    }
    data->service = service;
    data->socket = socket;
#ifdef _MSC_VER
    service->tcpThread = CreateThread(NULL, 0, tcpThread, data, 0, NULL);
#else
    pthread_create(&(service->tcpThread), NULL, tcpThread, data);
#endif
    service->tcpSocket = socket;
    return true;
}

static threadRet eventActorTask(void* args)
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
                REDFISH_DEBUG_ERR_PRINT("%s: Unknown work item type %d!\n", __FUNCTION__, wi->type);
            case WorkItemTermination:
                term = true;
                break;
            case WorkItemRegistration:
                processNewRegistrations(wi->registration, &registrations);
                wi->registration = NULL;//This is cleaned up, or not as appropirate in the prior call...
                break;
            case WorkItemEvent:
                REDFISH_DEBUG_ERR_PRINT("%s: Got new event %p (registrations = %p)\n", __FUNCTION__, wi->event, registrations);
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
    REDFISH_DEBUG_WARNING_PRINT("%s: Exiting...\n", __FUNCTION__);
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

static threadRet sseThread(void* args)
{
    struct SSEThreadData* data = (struct SSEThreadData*)args;
    CURL* curl;
    CURLcode res;
    struct SSEStruct readChunk;
    struct curl_slist* headers = NULL;
    char headerStr[1024];
    long httpCode;
    char* uri = data->sseUri;

    //Need to set this thread detached and make it clean itself up
    pthread_detach(pthread_self());

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
    REDFISH_DEBUG_ERR_PRINT("%s: Thread is done %d\n", __FUNCTION__, res);
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

#include <sys/socket.h>
#include <unistd.h>
#include <sys/poll.h>

static threadRet tcpThread(void* args)
{
    struct TCPThreadData* data = (struct TCPThreadData*)args;
    int tmpSock;
    char buffer[2048];
    size_t eventCount, i;
    EventInfo* events;
    struct pollfd ufds[1];
    int rv;
#ifdef HAVE_OPENSSL
    SSL_CTX* ctx;
    SSL* ssl;
    unsigned long err;
    int readCount;
    int buffPos;

    initOpenssl();

    ctx = createSSLContext();
#endif

    while(1)
    {
        ufds[0].fd = data->socket;
        ufds[0].events = POLLIN | POLLNVAL;
        // wait for events on the sockets, 0.5 second timeout
        // This let's the thread error out when the socket is closed and no events are received... 
        rv = poll(ufds, 1, 500);
        if(rv < 0)
        {
#ifdef HAVE_OPENSSL
            SSL_CTX_free(ctx);
            cleanupOpenssl();
#endif
            free(args);
#ifdef _MSC_VER
            return 0;
#else
            pthread_exit(NULL);
            return NULL;
#endif
        }
        else if(rv == 0)
        {
            //Timeout...
            continue;
        }
        if(ufds[0].revents & POLLNVAL)
        {
            REDFISH_DEBUG_ERR_PRINT("Poll found an error with the socket. Exit thread. %x\n", ufds[0].revents);
#ifdef HAVE_OPENSSL
            SSL_CTX_free(ctx);
            cleanupOpenssl();
#endif
            free(args);
#ifdef _MSC_VER
            return 0;
#else
            pthread_exit(NULL);
            return NULL;
#endif
        }
        tmpSock = accept(data->socket, NULL, NULL);
        if(tmpSock < 0)
        {
#ifdef HAVE_OPENSSL
            SSL_CTX_free(ctx);
            cleanupOpenssl();
#endif
            free(args);
#ifdef _MSC_VER
            return 0;
#else
            pthread_exit(NULL);
            return NULL;
#endif
        }
#ifdef HAVE_OPENSSL
        ssl = SSL_new(ctx);
        SSL_set_fd(ssl, tmpSock);
        if(SSL_accept(ssl) <= 0)
        {
            err = ERR_get_error();
            REDFISH_DEBUG_ERR_PRINT("%s: Unable to complete TLS handshake %u %s\n", __FUNCTION__, err, ERR_error_string(err, NULL));
            SSL_free(ssl);
            close(tmpSock);
            continue;
        }
#endif
        bzero(buffer, 2048);
#ifdef HAVE_OPENSSL
        buffPos = 0;
        while((unsigned int)buffPos < sizeof(buffer)-1)
        {
            readCount = SSL_read(ssl, buffer+buffPos, 2047-buffPos);
            //REDFISH_DEBUG_INFO_PRINT("%s: SSL_read returned %d bytes\n", __FUNCTION__, readCount);
            //REDFISH_DEBUG_INFO_PRINT("%s: Current Buffer is %s\n", __FUNCTION__, buffer);
            if(readCount < 0)
            {
                REDFISH_DEBUG_ERR_PRINT("%s: Unable to complete SSL read %u %s\n", __FUNCTION__, err, ERR_error_string(err, NULL));
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
#else
        read(tmpSock, buffer, 2047);
#endif
        eventCount = getRedfishEventInfoFromRawHttp(buffer, data->service, &events);
        if(eventCount)
        {
#ifdef HAVE_OPENSSL
            SSL_write(ssl,"HTTP/1.1 200 OK\nConnection: Closed\n\n",36);
#else
            write(tmpSock,"HTTP/1.1 200 OK\nConnection: Closed\n\n",36);
#endif
        }
        else
        {
#ifdef HAVE_OPENSSL
            SSL_write(ssl,"HTTP/1.1 400 Bad Request\nConnection: Closed\n\n",45);
#else
            write(tmpSock,"HTTP/1.1 400 Bad Request\nConnection: Closed\n\n",45);
#endif
        }
#ifdef HAVE_OPENSSL
        SSL_free(ssl);
#endif
        close(tmpSock);
        for(i = 0; i < eventCount; i++)
        {
            addEventToQueue(data->service, &(events[i]), true);
        }
        free(events);
    }
    free(args);
#ifdef HAVE_OPENSSL
    SSL_CTX_free(ctx);
    cleanupOpenssl();
#endif
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

static size_t getRedfishEventInfoFromRawHttp(const char* buffer, redfishService* service, EventInfo** events)
{
    EventInfo* ret;
    const char* bodyStart;
    redfishPayload* payload;
    redfishPayload* contextPayload;
    redfishPayload* eventArray;
    char* contextStr;
    size_t count, i;

    //REDFISH_DEBUG_DEBUG_PRINT("%s: Got Payload: %s\n", __FUNCTION__, buffer);

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
        //TODO Auth...
        ret[i].auth = NULL;
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
        REDFISH_DEBUG_ERR_PRINT("%s: Unable to create SSL context\n", __FUNCTION__);
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
    BIGNUM e;

    *publicKeyPtr = EVP_PKEY_new();
    cert = X509_new();
    rsa = RSA_new();

    BN_init(&e);
    BN_set_word(&e, 17);

    RSA_generate_key_ex(rsa, 2048, &e, NULL);
    EVP_PKEY_assign_RSA((*publicKeyPtr), rsa);
    BN_free(&e);
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
/* vim: set tabstop=4 shiftwidth=4 ff=unix expandtab: */
