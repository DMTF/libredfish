//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017-2019 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#include <string.h>
#include <stdlib.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "internal_service.h"
#include "asyncEvent.h"
#include <redfishService.h>
#include <redfishPayload.h>
#include <redpath.h>
#include <redfishEvent.h>
#include <redfishRawAsync.h>

#include "debug.h"
#include "util.h"

/**
 * @brief A representation of event actor state.
 *
 * A structure representing state of the event actor
 */
struct EventActorState
{
    /** If true, terminate the thread **/
    bool terminate;
    /** A linked list of struct EventCallbackRegister for each registration **/
    queueNode* registrations;
};

/** Default asynchronous options for Redfish calls **/
redfishAsyncOptions gDefaultOptions = {
    .accept = REDFISH_ACCEPT_JSON,
    .timeout = 20
};

static redfishService* createServiceEnumeratorNoAuth(const char* host, const char* rootUri, bool enumerate, unsigned int flags);
static redfishService* createServiceEnumeratorBasicAuth(const char* host, const char* rootUri, const char* username, const char* password, unsigned int flags);
static redfishService* createServiceEnumeratorSessionAuth(const char* host, const char* rootUri, const char* username, const char* password, unsigned int flags);
static redfishService* createServiceEnumeratorToken(const char* host, const char* rootUri, const char* token, unsigned int flags);
static char* makeUrlForService(redfishService* service, const char* uri);
static json_t* getVersions(redfishService* service, const char* rootUri);
static char* getSSEUri(redfishService* service);
static char* getEventSubscriptionUri(redfishService* service);
static void addStringToJsonArray(json_t* array, const char* value);
static void addStringToJsonObject(json_t* object, const char* key, const char* value);
static redfishPayload* getPayloadFromAsyncResponse(asyncHttpResponse* response, redfishService* service);
static unsigned char* base64_encode(const unsigned char* src, size_t len, size_t* out_len);
static bool createServiceEnumeratorNoAuthAsync(const char* host, const char* rootUri, unsigned int flags, redfishCreateAsyncCallback callback, void* context);
static bool createServiceEnumeratorBasicAuthAsync(const char* host, const char* rootUri, const char* username, const char* password, unsigned int flags, redfishCreateAsyncCallback callback, void* context);
static bool createServiceEnumeratorSessionAuthAsync(const char* host, const char* rootUri, const char* username, const char* password, unsigned int flags, redfishCreateAsyncCallback callback, void* context);
static bool createServiceEnumeratorTokenAsync(const char* host, const char* rootUri, const char* token, unsigned int flags, redfishCreateAsyncCallback callback, void* context);
static bool getVersionsAsync(redfishService* service, const char* rootUri, redfishCreateAsyncCallback callback, void* context);
static char* getDestinationAddress(const char* addressInfo, SOCKET* socket);

redfishService* createServiceEnumerator(const char* host, const char* rootUri, enumeratorAuthentication* auth, unsigned int flags)
{
    REDFISH_DEBUG_DEBUG_PRINT("%s: Entered. host = %s, rootUri = %s, auth = %p, flags = %x\n", __func__, host, rootUri, auth, flags);
    if(auth == NULL)
    {
        return createServiceEnumeratorNoAuth(host, rootUri, true, flags);
    }
    if(auth->authType == REDFISH_AUTH_BASIC)
    {
        return createServiceEnumeratorBasicAuth(host, rootUri, auth->authCodes.userPass.username, auth->authCodes.userPass.password, flags);
    }
    else if(auth->authType == REDFISH_AUTH_BEARER_TOKEN)
    {
        return createServiceEnumeratorToken(host, rootUri, auth->authCodes.authToken.token, flags);
    }
    else if(auth->authType == REDFISH_AUTH_SESSION)
    {
        return createServiceEnumeratorSessionAuth(host, rootUri, auth->authCodes.userPass.username, auth->authCodes.userPass.password, flags);
    }
    else
    {
        return NULL;
    }
}

/**
 * @brief An internal structure used to convert a call from async to sync.
 *
 * An internal structure used to convert a call from the new asynchronous to the old synchronous calls
 */
typedef struct
{
    /** A lock to control access to the condition variable **/
    mutex spinLock;
    /** The condition variable to be signalled on the async call completion **/
    condition waitForIt;
    /** The redfishPayload that was returned **/
    redfishPayload* data;
    /** True means the callback returned success, otherwise false **/
    bool success;
} asyncToSyncContext;

static asyncToSyncContext* makeAsyncToSyncContext()
{
    asyncToSyncContext* context;

    context = calloc(sizeof(asyncToSyncContext), 1);
    if(context)
    {
        mutex_init(&context->spinLock);
        cond_init(&context->waitForIt);
        //We start out locked...
        mutex_lock(&context->spinLock);
    }
    return context;
}

static void cleanupAsyncToSyncContext(asyncToSyncContext* context)
{
    mutex_destroy(&context->spinLock);
    cond_destroy(&context->waitForIt);
    free(context);
}

static bool isOnAsyncThread(redfishService* service)
{
    if(service == NULL)
    {
        return false;
    }
#ifdef _MSC_VER
    return (GetThreadId(service->asyncThread) == GetCurrentThreadId());
#else
    return (service->asyncThread == pthread_self());
#endif
}

void asyncToSyncConverter(bool success, unsigned short httpCode, redfishPayload* payload, void* context)
{
    asyncToSyncContext* myContext = (asyncToSyncContext*)context;
    char* content;

#ifndef DEBUG
    (void)httpCode;
#endif
    REDFISH_DEBUG_DEBUG_PRINT("%s: Entered. success = %d, httpCode = %u, payload = %p, context = %p\n", __func__, success, httpCode, payload, context);
    myContext->success = success;
    myContext->data = payload;
    if(payload != NULL && payload->content != NULL)
    {
        content = malloc(payload->contentLength+1);
        if(content)
        {
            memcpy(content, payload->content, payload->contentLength);
            content[payload->contentLength] = 0; //HTTP payloads aren't null terminated...
            REDFISH_DEBUG_DEBUG_PRINT("%s: Got non-json response to old sync operation %s\n", __func__, content);
            free(content);
        }
    }
    cond_broadcast(&myContext->waitForIt);
}

json_t* getUriFromService(redfishService* service, const char* uri)
{
    json_t* json;
    asyncToSyncContext* context;
    bool tmp;

    REDFISH_DEBUG_DEBUG_PRINT("%s: Entered. service = %p, uri = %s\n", __func__, service, uri);

    if(isOnAsyncThread(service))
    {
#ifdef _DEBUG
        //Abort a debug build so there is a core file pointing to this function and it's caller
        abort();
#endif
        return NULL;
    }

    context = makeAsyncToSyncContext();
    if(context == NULL)
    {
        REDFISH_DEBUG_CRIT_PRINT("%s: Failed to allocate context!\n", __func__);
        return NULL;
    }
    tmp = getUriFromServiceAsync(service,uri, NULL, asyncToSyncConverter, context);
    if(tmp == false)
    {
        REDFISH_DEBUG_ERR_PRINT("%s: Async call failed immediately...\n", __func__);
        cleanupAsyncToSyncContext(context);
        return NULL;
    }
    //Wait for the condition
    cond_wait(&context->waitForIt, &context->spinLock);
    if(context->data)
    {
        json = json_incref(context->data->json);
        cleanupPayload(context->data);
    }
    else
    {
        json = NULL;
    }
    cleanupAsyncToSyncContext(context);
    REDFISH_DEBUG_DEBUG_PRINT("%s: Exit. json = %p\n", __func__, json);
    return json;
}

json_t* patchUriFromService(redfishService* service, const char* uri, const char* content)
{
    json_t* json;
    asyncToSyncContext* context;
    bool tmp;
    redfishPayload* payload;

    REDFISH_DEBUG_DEBUG_PRINT("%s: Entered. service = %p, uri = %s, content = %s\n", __func__, service, uri, content);

    if(isOnAsyncThread(service))
    {
#ifdef _DEBUG
        //Abort a debug build so there is a core file pointing to this function and it's caller
        abort();
#endif
        return NULL;
    }

    context = makeAsyncToSyncContext();
    if(context == NULL)
    {
        REDFISH_DEBUG_CRIT_PRINT("%s: Failed to allocate context!\n", __func__);
        return NULL;
    }
    payload = createRedfishPayloadFromString(content, service);
    tmp = patchUriFromServiceAsync(service, uri, payload, NULL, asyncToSyncConverter, context);
    cleanupPayload(payload);
    if(tmp == false)
    {
        REDFISH_DEBUG_ERR_PRINT("%s: Async call failed immediately...\n", __func__);
        cleanupAsyncToSyncContext(context);
        return NULL;
    }
    //Wait for the condition
    cond_wait(&context->waitForIt, &context->spinLock);
    if(context->data)
    {
        json = json_incref(context->data->json);
        cleanupPayload(context->data);
    }
    else
    {
        json = NULL;
    }
    cleanupAsyncToSyncContext(context);
    return json;
}

json_t* postUriFromService(redfishService* service, const char* uri, const char* content, size_t contentLength, const char* contentType)
{
    json_t* json;
    asyncToSyncContext* context;
    bool tmp;
    redfishPayload* payload;
    char* tmpStr;

    REDFISH_DEBUG_DEBUG_PRINT("%s: Entered. service = %p, uri = %s, content = %s\n", __func__, service, uri, content);

    if(isOnAsyncThread(service))
    {
#ifdef _DEBUG
        //Abort a debug build so there is a core file pointing to this function and it's caller
        abort();
#endif
        return NULL;
    }

    context = makeAsyncToSyncContext();
    if(context == NULL)
    {
        REDFISH_DEBUG_CRIT_PRINT("%s: Failed to allocate context!\n", __func__);
        return NULL;
    }
    payload = createRedfishPayloadFromContent(content, contentLength, contentType, service);
    tmp = postUriFromServiceAsync(service, uri, payload, NULL, asyncToSyncConverter, context);
    cleanupPayload(payload);
    if(tmp == false)
    {
        REDFISH_DEBUG_ERR_PRINT("%s: Async call failed immediately...\n", __func__);
        cleanupAsyncToSyncContext(context);
        return NULL;
    }
    //Wait for the condition
    cond_wait(&context->waitForIt, &context->spinLock);
    if(context->data)
    {
        json = json_incref(context->data->json);
        cleanupPayload(context->data);
    }
    else
    {
        json = NULL;
    }
    if(context->success == false)
    {
        REDFISH_DEBUG_ERR_PRINT("%s: Old style call got an error, but returned payload!\n", __func__);
        tmpStr = json_dumps(json, 0);
        REDFISH_DEBUG_NOTICE_PRINT("%s: Response payload is %s\n", __func__, tmpStr);
        free(tmpStr);
        json_decref(json);
        json = NULL;
    }
    cleanupAsyncToSyncContext(context);
    REDFISH_DEBUG_DEBUG_PRINT("%s: Exit.\n", __func__);
    return json;
}

bool deleteUriFromService(redfishService* service, const char* uri)
{
    asyncToSyncContext* context;
    bool tmp;

    REDFISH_DEBUG_DEBUG_PRINT("%s: Entered. service = %p, uri = %s\n", __func__, service, uri);

    if(isOnAsyncThread(service))
    {
#ifdef _DEBUG
        //Abort a debug build so there is a core file pointing to this function and it's caller
        abort();
#endif
        return false;
    }

    context = makeAsyncToSyncContext();
    if(context == NULL)
    {
        REDFISH_DEBUG_CRIT_PRINT("%s: Failed to allocate context!\n", __func__);
        return false;
    }
    tmp = deleteUriFromServiceAsync(service, uri, NULL, asyncToSyncConverter, context);
    if(tmp == false)
    {
        REDFISH_DEBUG_ERR_PRINT("%s: Async call failed immediately...\n", __func__);
        cleanupAsyncToSyncContext(context);
        return tmp;
    }
    //Wait for the condition
    cond_wait(&context->waitForIt, &context->spinLock);
    tmp = context->success;
    cleanupAsyncToSyncContext(context);
    return tmp;
}

/**
 * @brief An internal structure used to convert Redfish calls to raw async HTTP(s) calls.
 *
 * An internal structure used to convert a call from the Redfish call interface to the raw HTTP(s) interface
 */
typedef struct
{
    /** The redfish style callback to call when the async HTTP(s) call is complete **/
    redfishAsyncCallback callback;
    /** The original caller provided context to pass to the callback **/
    void*                originalContext;
    /** The original options passed to the call so that child calls can use the same options **/
    redfishAsyncOptions* originalOptions;
    /** The redfish service the call was made on **/
    redfishService*      service;
} rawAsyncCallbackContextWrapper;

static void rawCallbackWrapper(asyncHttpRequest* request, asyncHttpResponse* response, void* context)
{
    bool success = false;
    rawAsyncCallbackContextWrapper* myContext = (rawAsyncCallbackContextWrapper*)context;
    redfishPayload* payload;
    httpHeader* header;

    REDFISH_DEBUG_DEBUG_PRINT("%s: Entered. request = %p, response = %p, context = %p\n", __func__, request, response, context);

    header = responseGetHeader(response, "X-Auth-Token");
    if(header)
    {
        if(myContext->service->sessionToken)
        {
            free(myContext->service->sessionToken);
        }
        myContext->service->sessionToken = safeStrdup(header->value);
    }
    if(response->httpResponseCode == 201)
    {
        //This is a created response, go get the actual payload...
        header = responseGetHeader(response, "Location");
        getUriFromServiceAsync(myContext->service, header->value, myContext->originalOptions, myContext->callback, myContext->originalContext);
        freeAsyncRequest(request);
        freeAsyncResponse(response);
        serviceDecRef(myContext->service);
        free(context);
        REDFISH_DEBUG_DEBUG_PRINT("%s: Exit. Location Redirect...\n", __func__);
        return;
    }
    if(myContext->callback)
    {
        if(response->connectError == 0 && response->httpResponseCode >= 200 && response->httpResponseCode < 300)
        {
            success = true;
        }
        payload = getPayloadFromAsyncResponse(response, myContext->service);
        myContext->callback(success, (unsigned short)response->httpResponseCode, payload, myContext->originalContext);
    }
    freeAsyncRequest(request);
    freeAsyncResponse(response);
    serviceDecRef(myContext->service);
    free(context);
    REDFISH_DEBUG_DEBUG_PRINT("%s: Exit.\n", __func__);
}

static void setupRequestFromOptions(asyncHttpRequest* request, redfishService* service, redfishAsyncOptions* options)
{
    char tmp[1024];

    if(options == NULL)
    {
        options = &gDefaultOptions;
    }
    switch(options->accept)
    {
        default:
        case REDFISH_ACCEPT_ALL:
            addRequestHeader(request, "Accept", "*/*");
            break;
        case REDFISH_ACCEPT_JSON:
            addRequestHeader(request, "Accept", "application/json");
            break;
        case REDFISH_ACCEPT_XML:
            addRequestHeader(request, "Accept", "application/xml");
            break;
    }
    addRequestHeader(request, "OData-Version", "4.0");
    addRequestHeader(request, "User-Agent", "libredfish");

    if(service->sessionToken)
    {
        addRequestHeader(request, "X-Auth-Token", service->sessionToken);
    }
    else if(service->bearerToken)
    {
        tmp[sizeof(tmp)-1] = 0;
        snprintf(tmp, sizeof(tmp)-1, "Bearer %s", service->bearerToken);
        addRequestHeader(request, "Authorization", tmp);
    }
    else if(service->otherAuth)
    {
        addRequestHeader(request, "Authorization", service->otherAuth);
    }
    request->timeout = options->timeout;
}

bool createServiceEnumeratorAsync(const char* host, const char* rootUri, enumeratorAuthentication* auth, unsigned int flags, redfishCreateAsyncCallback callback, void* context)
{
    REDFISH_DEBUG_DEBUG_PRINT("%s: Entered. host = %s, rootUri = %s, auth = %p, callback = %p, context = %p\n", __func__, host, rootUri, auth, callback, context);
    if(auth == NULL)
    {
        return createServiceEnumeratorNoAuthAsync(host, rootUri, flags, callback, context);
    }
    if(auth->authType == REDFISH_AUTH_BASIC)
    {
        return createServiceEnumeratorBasicAuthAsync(host, rootUri, auth->authCodes.userPass.username, auth->authCodes.userPass.password, flags, callback, context);
    }
    else if(auth->authType == REDFISH_AUTH_BEARER_TOKEN)
    {
        return createServiceEnumeratorTokenAsync(host, rootUri, auth->authCodes.authToken.token, flags, callback, context);
    }
    else if(auth->authType == REDFISH_AUTH_SESSION)
    {
        return createServiceEnumeratorSessionAuthAsync(host, rootUri, auth->authCodes.userPass.username, auth->authCodes.userPass.password, flags, callback, context);
    }
    else
    {
        return false;
    }
}

bool getUriFromServiceAsync(redfishService* service, const char* uri, redfishAsyncOptions* options, redfishAsyncCallback callback, void* context)
{
    char* url;
    asyncHttpRequest* request;
    rawAsyncCallbackContextWrapper* myContext;
    bool ret;

    REDFISH_DEBUG_DEBUG_PRINT("%s: Entered. service = %p, uri = %s, options = %p, callback = %p, context = %p\n", __func__, service, uri, options, callback, context);

    serviceIncRef(service);

    url = makeUrlForService(service, uri);
    if(!url)
    {
        REDFISH_DEBUG_ERR_PRINT("%s: Error. Could not make url for uri %s\n", __func__, uri);
        serviceDecRef(service);
        return false;
    }

    request = createRequest(url, HTTP_GET, 0, NULL);
    free(url);
    setupRequestFromOptions(request, service, options);

    myContext = malloc(sizeof(rawAsyncCallbackContextWrapper));
    myContext->callback = callback;
    myContext->originalContext = context;
    myContext->originalOptions = options;
    myContext->service = service;
    ret = startRawAsyncRequest(service, request, rawCallbackWrapper, myContext);
    if(ret == false)
    {
        free(myContext);
    }
    REDFISH_DEBUG_DEBUG_PRINT("%s: Exit. ret = %u\n", __func__, ret);
    return ret;
}

bool patchUriFromServiceAsync(redfishService* service, const char* uri, redfishPayload* payload, redfishAsyncOptions* options, redfishAsyncCallback callback, void* context)
{
    char* url;
    asyncHttpRequest* request;
    rawAsyncCallbackContextWrapper* myContext;
    bool ret;

    REDFISH_DEBUG_DEBUG_PRINT("%s: Entered. service = %p, uri = %s, payload = %p\n", __func__, service, uri, payload);

    serviceIncRef(service);

    url = makeUrlForService(service, uri);
    if(!url)
    {
        serviceDecRef(service);
        return false;
    }

    request = createRequest(url, HTTP_PATCH, getPayloadSize(payload), getPayloadBody(payload));
    free(url);
    setupRequestFromOptions(request, service, options);
    addRequestHeader(request, "Content-Type", getPayloadContentType(payload));

    myContext = malloc(sizeof(rawAsyncCallbackContextWrapper));
    myContext->callback = callback;
    myContext->originalContext = context;
    myContext->originalOptions = options;
    myContext->service = service;
    ret = startRawAsyncRequest(service, request, rawCallbackWrapper, myContext);
    if(ret == false)
    {
        free(myContext);
    }
    return ret;
}

bool postUriFromServiceAsync(redfishService* service, const char* uri, redfishPayload* payload, redfishAsyncOptions* options, redfishAsyncCallback callback, void* context)
{
    char* url;
    asyncHttpRequest* request;
    rawAsyncCallbackContextWrapper* myContext;
    bool ret;

    serviceIncRef(service);

    url = makeUrlForService(service, uri);
    if(!url)
    {
        serviceDecRef(service);
        return false;
    }

    request = createRequest(url, HTTP_POST, getPayloadSize(payload), getPayloadBody(payload));
    free(url);
    setupRequestFromOptions(request, service, options);
    addRequestHeader(request, "Content-Type", getPayloadContentType(payload));

    myContext = malloc(sizeof(rawAsyncCallbackContextWrapper));
    myContext->callback = callback;
    myContext->originalContext = context;
    myContext->originalOptions = options;
    myContext->service = service;
    ret = startRawAsyncRequest(service, request, rawCallbackWrapper, myContext);
    if(ret == false)
    {
        free(myContext);
    }
    return ret;
}

bool deleteUriFromServiceAsync(redfishService* service, const char* uri, redfishAsyncOptions* options, redfishAsyncCallback callback, void* context)
{
    char* url;
    asyncHttpRequest* request;
    rawAsyncCallbackContextWrapper* myContext;
    bool ret;

    serviceIncRef(service);

    url = makeUrlForService(service, uri);
    if(!url)
    {
        serviceDecRef(service);
        return false;
    }

    request = createRequest(url, HTTP_DELETE, 0, NULL);
    free(url);
    setupRequestFromOptions(request, service, options);

    myContext = malloc(sizeof(rawAsyncCallbackContextWrapper));
    myContext->callback = callback;
    myContext->originalContext = context;
    myContext->originalOptions = options;
    myContext->service = service;
    ret = startRawAsyncRequest(service, request, rawCallbackWrapper, myContext);
    if(ret == false)
    {
        free(myContext);
    }
    return ret;
}

bool getRedfishServiceRootAsync(redfishService* service, const char* version, redfishAsyncOptions* options, redfishAsyncCallback callback, void* context)
{
    json_t* versionNode;
    const char* verUrl;
    bool ret;

    if(version == NULL)
    {
        versionNode = json_object_get(service->versions, "v1");
    }
    else
    {
        versionNode = json_object_get(service->versions, version);
    }
    if(versionNode == NULL)
    {
        return false;
    }
    verUrl = json_string_value(versionNode);
    if(verUrl == NULL)
    {
        return false;
    }
    ret = getUriFromServiceAsync(service, verUrl, options, callback, context);
    return ret;
}

typedef struct
{
    redfishAsyncCallback callback;
    void* originalContext;
    redPathNode* redpath;
    redfishAsyncOptions* options;
} redpathAsyncContext;

void gotServiceRootAsync(bool success, unsigned short httpCode, redfishPayload* payload, void* context)
{
    redpathAsyncContext* myContext = (redpathAsyncContext*)context;
    redfishPayload* root = payload;
    bool ret;

    REDFISH_DEBUG_DEBUG_PRINT("%s: Entered. success = %u, httpCode = %p, payload = %p, context = %p\n", __func__, success, httpCode, payload, context);

    if(success == false || httpCode >= 400 || myContext->redpath->next == NULL)
    {
        myContext->callback(success, httpCode, payload, myContext->originalContext);
        cleanupRedPath(myContext->redpath);
        free(context);
        return;
    }
    ret = getPayloadForPathAsync(root, myContext->redpath->next, myContext->options, myContext->callback, myContext->originalContext);
    cleanupPayload(root);
    if(ret == false)
    {
        REDFISH_DEBUG_ERR_PRINT("%s: Failed to get next path section immediately...", __func__);
        myContext->callback(ret, 0xFFFF, NULL, myContext->originalContext);
        cleanupRedPath(myContext->redpath);
    }
    else
    {
        //Free just this redpath node...
        myContext->redpath->next = NULL;
        cleanupRedPath(myContext->redpath);
    }
    free(context);
}

bool getPayloadByPathAsync(redfishService* service, const char* path, redfishAsyncOptions* options, redfishAsyncCallback callback, void* context)
{
    redPathNode* redpath;
    bool ret;
    redpathAsyncContext* myContext;

    if(!service || !path)
    {
        return false;
    }

    redpath = parseRedPath(path);
    if(!redpath)
    {
        return false;
    }
    if(!redpath->isRoot)
    {
        cleanupRedPath(redpath);
        return false;
    }
    myContext = malloc(sizeof(redpathAsyncContext));
    if(!myContext)
    {
        cleanupRedPath(redpath);
        return false;
    }
    myContext->callback = callback;
    myContext->originalContext = context;
    myContext->redpath = redpath;
    myContext->options = options;
    ret = getRedfishServiceRootAsync(service, redpath->version, options, gotServiceRootAsync, myContext);
    if(ret == false)
    {
        free(myContext);
        cleanupRedPath(redpath);
    }
    return ret;
}

bool registerForEvents(redfishService* service, const char* postbackUri, unsigned int eventTypes, redfishEventCallback callback, const char* context)
{
    json_t* eventSubscriptionPayload;
    json_t* typeArray;
    char* sseUri;
    char* eventSubscriptionUri;
    char* destination = (char*)postbackUri;
    bool ret;
	SOCKET socket;
    redfishPayload* postPayload;
    asyncToSyncContext* asyncContext;

    if(service == NULL || postbackUri == NULL || callback == NULL)
    {
        return false;
    }

    //start event thread
    if(service->eventThreadQueue == NULL)
    {
        service->eventThreadQueue = newQueue();
        startEventThread(service);
    }
    registerCallback(service, callback, eventTypes, context);

    //Try to use SSE first...
    sseUri = getSSEUri(service);
    if(sseUri)
    {
        ret = startSSEListener(service, sseUri);
        if(ret == true)
        {
            free(sseUri);
            return true;
        }
        else
        {
            REDFISH_DEBUG_ERR_PRINT("Service supports SSE, but listening to SSE @ %s failed. Falling back to old style events...\n", sseUri);
            free(sseUri);
        }
    }

    //User wants libredfish to listen for events directly...
    if(strncmp(postbackUri, "libredfish", 10) == 0)
    {
        destination = getDestinationAddress(postbackUri+11, &socket);
        if(destination == NULL)
        {
            REDFISH_DEBUG_CRIT_PRINT("Unable to obtain destination address from string \"%s\"\n", postbackUri);
            return false;
        }
        ret = startTCPListener(service, socket);
    }

    eventSubscriptionUri = getEventSubscriptionUri(service);
    if(eventSubscriptionUri == NULL)
    {
        //This service does not support Eventing
        if(destination != (char*)postbackUri)
        {
            free(destination);
        }
        return false;
    }

    eventSubscriptionPayload = json_object();
    if(eventSubscriptionPayload == NULL)
    {
        //Out of memory
        free(eventSubscriptionUri);
        if(destination != (char*)postbackUri)
        {
            free(destination);
        }
        return false;
    }
    addStringToJsonObject(eventSubscriptionPayload, "Destination", destination);
    if(destination != (char*)postbackUri)
    {
        free(destination);
    }
    else
    {
        //Start external event listeners...
#ifndef NO_CZMQ
        startZeroMQListener(service);
#endif
    }
    addStringToJsonObject(eventSubscriptionPayload, "Context", context);
    addStringToJsonObject(eventSubscriptionPayload, "Protocol", "Redfish");
    if(eventTypes != 0)
    {
        typeArray = json_array();
        if(typeArray == NULL)
        {
            json_decref(eventSubscriptionPayload);
            free(eventSubscriptionUri);
            return false;
        }
        if(eventTypes & REDFISH_EVENT_TYPE_STATUSCHANGE)
        {
            addStringToJsonArray(typeArray, "StatusChange");
        }
        if(eventTypes & REDFISH_EVENT_TYPE_RESOURCEUPDATED)
        {
            addStringToJsonArray(typeArray, "ResourceUpdated");
        }
        if(eventTypes & REDFISH_EVENT_TYPE_RESOURCEADDED)
        {
            addStringToJsonArray(typeArray, "ResourceAdded");
        }
        if(eventTypes & REDFISH_EVENT_TYPE_RESOURCEREMOVED)
        {
            addStringToJsonArray(typeArray, "ResourceRemoved");
        }
        if(eventTypes & REDFISH_EVENT_TYPE_ALERT)
        {
            addStringToJsonArray(typeArray, "Alert");
        }
        json_object_set(eventSubscriptionPayload, "EventTypes", typeArray);
        json_decref(typeArray);
    }
    postPayload = createRedfishPayload(eventSubscriptionPayload, service);
    if(postPayload == NULL)
    {
        free(eventSubscriptionUri);
        return false;
    }

    asyncContext = makeAsyncToSyncContext();
    if(asyncContext == NULL)
    {
        REDFISH_DEBUG_CRIT_PRINT("%s: Failed to allocate asyncContext!\n", __func__);
        return false;
    }
    ret = postUriFromServiceAsync(service, eventSubscriptionUri, postPayload, NULL, asyncToSyncConverter, asyncContext);
    free(eventSubscriptionUri);
    cleanupPayload(postPayload);
    if(ret == false)
    {
        REDFISH_DEBUG_ERR_PRINT("%s: Async call failed immediately...\n", __func__);
        cleanupAsyncToSyncContext(asyncContext);
        return false;
    }
    //Wait for the condition
    cond_wait(&asyncContext->waitForIt, &asyncContext->spinLock);
    if(asyncContext->data)
    {
        service->eventRegistrationUri = getPayloadUri(asyncContext->data);
        cleanupPayload(asyncContext->data);
    }
    if(asyncContext->success == false)
    {
        unregisterCallback(service, callback, eventTypes, context);
        cleanupAsyncToSyncContext(asyncContext);
        return false;
    }
    cleanupAsyncToSyncContext(asyncContext);
    return true;
}

redfishPayload* getRedfishServiceRoot(redfishService* service, const char* version)
{
    json_t* value;
    json_t* versionNode;
    const char* verUrl;

    if(version == NULL)
    {
        versionNode = json_object_get(service->versions, "v1");
    }
    else
    {
        versionNode = json_object_get(service->versions, version);
    }
    if(versionNode == NULL)
    {
        return NULL;
    }
    verUrl = json_string_value(versionNode);
    if(verUrl == NULL)
    {
        return NULL;
    }
    value = getUriFromService(service, verUrl);
    if(value == NULL)
    {
        if((service->flags & REDFISH_FLAG_SERVICE_NO_VERSION_DOC) == 0)
        {
            json_decref(versionNode);
        }
        return NULL;
    }
    return createRedfishPayload(value, service);
}

redfishPayload* getPayloadByPath(redfishService* service, const char* path)
{
    redPathNode* redpath;
    redfishPayload* root;
    redfishPayload* ret;

    if(!service || !path)
    {
        return NULL;
    }

    redpath = parseRedPath(path);
    if(!redpath)
    {
        return NULL;
    }
    if(!redpath->isRoot)
    {
        cleanupRedPath(redpath);
        return NULL;
    }
    root = getRedfishServiceRoot(service, redpath->version);
    if(redpath->next == NULL)
    {
        cleanupRedPath(redpath);
        return root;
    }
    ret = getPayloadForPath(root, redpath->next);
    cleanupPayload(root);
    cleanupRedPath(redpath);
    return ret;
}

void cleanupServiceEnumerator(redfishService* service)
{
    if(!service)
    {
        return;
    }
    serviceDecRef(service);
}

void serviceIncRef(redfishService* service)
{
    if(service->freeing)
    {
        //service is currntly being free'd. Don't do anything...
        return;
    }
#ifdef _MSC_VER
#if _M_AMD64
    InterlockedIncrement64(&(service->refCount));
#else
    InterlockedIncrement(&(service->refCount));
#endif
#else
    __sync_fetch_and_add(&(service->refCount), 1);
#endif
    REDFISH_DEBUG_DEBUG_PRINT("%s: New count = %u\n", __func__, service->refCount);
}

void terminateAsyncThread(redfishService* service);

static void freeServicePtr(redfishService* service)
{
    if(service->tcpSocket != -1)
    {
#ifdef _MSC_VER
		closesocket(service->tcpSocket);
		service->tcpSocket = INVALID_SOCKET;
#else
		close(service->tcpSocket);
		service->tcpSocket = -1;
#endif
#ifdef _MSC_VER
        WaitForSingleObject(service->tcpThread, INFINITE);
#else
        pthread_join(service->tcpThread, NULL);
#endif
    }
    service->freeing = true;
    if(service->eventRegistrationUri)
    {
        deleteUriFromServiceAsync(service, service->eventRegistrationUri, NULL, NULL, NULL);
        free(service->eventRegistrationUri);
    }
    REDFISH_DEBUG_INFO_PRINT("%s: Service session URI = %s\n", __func__, service->sessionUri);
    if(service->sessionUri)
    {
        deleteUriFromServiceAsync(service, service->sessionUri, NULL, NULL, NULL);
        free(service->sessionUri);
    }
    if(service->eventThreadQueue != NULL)
    {
        terminateAsyncEventThread(service);
    }
    terminateAsyncThread(service);
    free(service->host);
    service->host = NULL;
    json_decref(service->versions);
    service->versions = NULL;
    if(service->sessionToken != NULL)
    {
        free(service->sessionToken);
        service->sessionToken = NULL;
    }
    if(service->bearerToken != NULL)
    {
        free(service->bearerToken);
        service->bearerToken = NULL;
    }
    if(service->otherAuth != NULL)
    {
        free(service->otherAuth);
        service->otherAuth = NULL;
    }
    if(service->selfTerm == false && service->eventTerm == false)
    {
        free(service);
    }
}

void serviceDecRef(redfishService* service)
{
    size_t newCount;

    if(service == NULL)
    {
        return;
    }
    if(service->freeing)
    {
        //service is currntly being free'd. Don't do anything...
        return;
    }
#ifdef _MSC_VER
#if _M_AMD64
    newCount = InterlockedDecrement64(&(service->refCount));
#else
    newCount = InterlockedDecrement(&(service->refCount));
#endif
#else
    newCount = __sync_sub_and_fetch(&(service->refCount), 1);
#endif
    REDFISH_DEBUG_DEBUG_PRINT("%s: New count = %u\n", __func__, newCount);
    if(newCount == 0)
    {
        freeServicePtr(service);
    }
}

void serviceDecRefAndWait(redfishService* service)
{
    size_t newCount;

    if(service == NULL)
    {
        return;
    }
#ifdef _MSC_VER
#if _M_AMD64
    newCount = InterlockedDecrement64(&(service->refCount));
#else
    newCount = InterlockedDecrement(&(service->refCount));
#endif
#else
    newCount = __sync_sub_and_fetch(&(service->refCount), 1);
#endif
    REDFISH_DEBUG_DEBUG_PRINT("%s: New count = %u\n", __func__, newCount);
    if(newCount == 0)
    {
        freeServicePtr(service);
    }
    else
    {
        while(service->refCount != 0)
        {
#ifdef _MSC_VER
            SwitchToThread();
#else
            sched_yield();
#endif
        }
    }
}

static redfishService* createServiceEnumeratorNoAuth(const char* host, const char* rootUri, bool enumerate, unsigned int flags)
{
    redfishService* ret;

    ret = (redfishService*)calloc(1, sizeof(redfishService));
    serviceIncRef(ret);
#ifdef _MSC_VER
	ret->host = _strdup(host);
#else
    ret->host = strdup(host);
#endif
    ret->flags = flags;
    ret->tcpSocket = -1;
    if(enumerate)
    {
        ret->versions = getVersions(ret, rootUri);
    }

    return ret;
}

static bool createServiceEnumeratorNoAuthAsync(const char* host, const char* rootUri, unsigned int flags, redfishCreateAsyncCallback callback, void* context)
{
    redfishService* ret;
    bool rc;

    ret = (redfishService*)calloc(1, sizeof(redfishService));
    if(ret == NULL)
    {
        return false;
    }
    serviceIncRef(ret);
#ifdef _MSC_VER
    ret->host = _strdup(host);
#else
    ret->host = strdup(host);
#endif
    ret->flags = flags;
    ret->tcpSocket = -1;
    rc = getVersionsAsync(ret, rootUri, callback, context);
    if(rc == false)
    {
        serviceDecRef(ret);
    }
    return rc;
}

static redfishService* createServiceEnumeratorBasicAuth(const char* host, const char* rootUri, const char* username, const char* password, unsigned int flags)
{
    redfishService* ret;
    char userPass[1024] = {0};
    char* base64;
    size_t original;
    size_t newSize;

    original = snprintf(userPass, sizeof(userPass), "%s:%s", username, password);
    base64 = (char*)base64_encode((unsigned char*)userPass, original, &newSize);
    if(base64 == NULL)
    {
        return NULL;
    }
    snprintf(userPass, sizeof(userPass), "Basic %s", base64);
    free(base64);

    ret = createServiceEnumeratorNoAuth(host, rootUri, false, flags);
    ret->otherAuth = safeStrdup(userPass);
    ret->versions = getVersions(ret, rootUri);
    return ret;
}

static bool createServiceEnumeratorBasicAuthAsync(const char* host, const char* rootUri, const char* username, const char* password, unsigned int flags, redfishCreateAsyncCallback callback, void* context)
{
    redfishService* ret;
    char userPass[1024] = {0};
    char* base64;
    size_t original;
    size_t newSize;
    bool rc;

    original = snprintf(userPass, sizeof(userPass), "%s:%s", username, password);
    base64 = (char*)base64_encode((unsigned char*)userPass, original, &newSize);
    if(base64 == NULL)
    {
        return false;
    }
    snprintf(userPass, sizeof(userPass), "Basic %s", base64);
    free(base64);

    //This does no network interactions when enumerate is false... use it because it's easier
    ret = createServiceEnumeratorNoAuth(host, rootUri, false, flags);
    if(ret == NULL)
    {
        return false;
    }
    ret->otherAuth = safeStrdup(userPass);
    rc = getVersionsAsync(ret, rootUri, callback, context);
    if(rc == false)
    {
        serviceDecRef(ret);
    }
    return rc;
}

static redfishService* createServiceEnumeratorSessionAuth(const char* host, const char* rootUri, const char* username, const char* password, unsigned int flags)
{
    redfishService* ret;
    redfishPayload* payload;
    redfishPayload* links;
    json_t* sessionPayload;
    json_t* session;
    json_t* odataId;
    const char* uri;
    json_t* post;
    char* content;

    ret = createServiceEnumeratorNoAuth(host, rootUri, true, flags);
    if(ret == NULL)
    {
        return NULL;
    }
    payload = getRedfishServiceRoot(ret, NULL);
    if(payload == NULL)
    {
        cleanupServiceEnumerator(ret);
        return NULL;
    }
    links = getPayloadByNodeName(payload, "Links");
    cleanupPayload(payload);
    if(links == NULL)
    {
        cleanupServiceEnumerator(ret);
        return NULL;
    }
    session = json_object_get(links->json, "Sessions");
    if(session == NULL)
    {
        cleanupPayload(links);
        cleanupServiceEnumerator(ret);
        return NULL;
    }
    odataId = json_object_get(session, "@odata.id");
    if(odataId == NULL)
    {
        cleanupPayload(links);
        cleanupServiceEnumerator(ret);
        return NULL;
    }
    uri = json_string_value(odataId);
    post = json_object();
    addStringToJsonObject(post, "UserName", username);
    addStringToJsonObject(post, "Password", password);
    content = json_dumps(post, 0);
    json_decref(post);
    sessionPayload = postUriFromService(ret, uri, content, 0, NULL);
    if(sessionPayload == NULL)
    {
        //Failed to create session!
        free(content);
        cleanupPayload(links);
        cleanupServiceEnumerator(ret);
        return NULL;
    }
    post = json_object_get(sessionPayload, "@odata.id");
    if(post)
    {
        ret->sessionUri = safeStrdup(json_string_value(post));
        REDFISH_DEBUG_INFO_PRINT("%s: Got Session URI %s\n", __func__, ret->sessionUri);
    }
    json_decref(sessionPayload);
    cleanupPayload(links);
    free(content);
    return ret;
}

typedef struct {
    char* username;
    char* password;
    redfishCreateAsyncCallback originalCallback;
    void* originalContext;
    redfishService* service;
} createServiceSessionAuthAsyncContext;

static void didSessionAuthPost(bool success, unsigned short httpCode, redfishPayload* payload, void* context)
{
    createServiceSessionAuthAsyncContext* myContext = (createServiceSessionAuthAsyncContext*)context;

    (void)httpCode;

    if(payload)
    {
        myContext->service->sessionUri = getPayloadUri(payload);
        REDFISH_DEBUG_INFO_PRINT("%s: Got Session URI %s\n", __func__, myContext->service->sessionUri);
        cleanupPayload(payload);
    }

    if(success == false)
    {
        myContext->originalCallback(NULL, myContext->originalContext);
        free(myContext->username);
        free(myContext->password);
        serviceDecRef(myContext->service);
        free(myContext);
        return;
    }

    //The session token should already be set if the redfish service works according to spec...
    if(myContext->service->sessionToken == NULL)
    {
        REDFISH_DEBUG_ERR_PRINT("Session returned success (%u) but did not set X-Auth-Token header...\n", httpCode);
        myContext->originalCallback(NULL, myContext->originalContext);
        free(myContext->username);
        free(myContext->password);
        serviceDecRef(myContext->service);
        free(myContext);
        return;
    } 
    myContext->originalCallback(myContext->service, myContext->originalContext);
    free(myContext->username);
    free(myContext->password);
    free(myContext);
}

static redfishPayload* createAuthPayload(const char* username, const char* password, redfishService* service)
{
    json_t* post = json_object();
    addStringToJsonObject(post, "UserName", username);
    addStringToJsonObject(post, "Password", password);

    return createRedfishPayload(post, service);
}

static void gotServiceRootServiceAuth(bool success, unsigned short httpCode, redfishPayload* payload, void* context)
{
    createServiceSessionAuthAsyncContext* myContext = (createServiceSessionAuthAsyncContext*)context;
    redfishPayload* links;
    redfishPayload* authPayload;
    json_t* session;
    json_t* odataId;
    const char* uri;
    bool rc;

    (void)httpCode;

    if(success == false)
    {
        myContext->originalCallback(NULL, myContext->originalContext);
        free(myContext->username);
        free(myContext->password);
        serviceDecRef(myContext->service);
        free(myContext);
        return;
    }

    links = getPayloadByNodeNameNoNetwork(payload, "Links");
    cleanupPayload(payload);
    if(links == NULL)
    {
        myContext->originalCallback(NULL, myContext->originalContext);
        free(myContext->username);
        free(myContext->password);
        serviceDecRef(myContext->service);
        free(myContext);
        return;
    }
    session = json_object_get(links->json, "Sessions");
    if(session == NULL)
    {
        cleanupPayload(links);
        myContext->originalCallback(NULL, myContext->originalContext);
        free(myContext->username);
        free(myContext->password);
        serviceDecRef(myContext->service);
        free(myContext);
        return;
    }
    odataId = json_object_get(session, "@odata.id");
    if(odataId == NULL)
    {
        cleanupPayload(links);
        myContext->originalCallback(NULL, myContext->originalContext);
        free(myContext->username);
        free(myContext->password);
        serviceDecRef(myContext->service);
        free(myContext);
        return;
    }
    uri = json_string_value(odataId);

    authPayload = createAuthPayload(myContext->username, myContext->password, myContext->service);
    if(authPayload == NULL)
    {
        cleanupPayload(links);
        myContext->originalCallback(NULL, myContext->originalContext);
        free(myContext->username);
        free(myContext->password);
        serviceDecRef(myContext->service);
        free(myContext);
        return;
    }

    rc = postUriFromServiceAsync(myContext->service, uri, authPayload, NULL, didSessionAuthPost, myContext);
    cleanupPayload(links);
    cleanupPayload(authPayload);
    if(rc == false)
    {
        myContext->originalCallback(NULL, myContext->originalContext);
        free(myContext->username);
        free(myContext->password);
        serviceDecRef(myContext->service);
        free(myContext);
    }
}

static void finishedRedfishCreate(redfishService* service, void* context)
{
    bool rc;
    createServiceSessionAuthAsyncContext* myContext = (createServiceSessionAuthAsyncContext*)context;

    myContext->service = service;

    rc = getRedfishServiceRootAsync(service, NULL, NULL, gotServiceRootServiceAuth, myContext);
    if(rc == false)
    {
        serviceDecRef(myContext->service);
        free(myContext->username);
        free(myContext->password);
        free(myContext);
    }
}

static bool createServiceEnumeratorSessionAuthAsync(const char* host, const char* rootUri, const char* username, const char* password, unsigned int flags, redfishCreateAsyncCallback callback, void* context)
{
    bool rc;
    createServiceSessionAuthAsyncContext* myContext;

    myContext = malloc(sizeof(createServiceSessionAuthAsyncContext));
    myContext->username = safeStrdup(username);
    myContext->password = safeStrdup(password);
    myContext->originalCallback = callback;
    myContext->originalContext = context;
    myContext->service = NULL;

    rc = createServiceEnumeratorNoAuthAsync(host, rootUri, flags, finishedRedfishCreate, myContext);
    if(rc == false)
    {
        free(myContext->username);
        free(myContext->password);
        free(myContext);
    }
    return rc;
}

static redfishService* createServiceEnumeratorToken(const char* host, const char* rootUri, const char* token, unsigned int flags)
{
    redfishService* ret;

    ret = createServiceEnumeratorNoAuth(host, rootUri, false, flags);
    if(ret == NULL)
    {
        return ret;
    }
    ret->bearerToken = safeStrdup(token);
    ret->versions = getVersions(ret, rootUri);
    return ret;
}

static bool createServiceEnumeratorTokenAsync(const char* host, const char* rootUri, const char* token, unsigned int flags, redfishCreateAsyncCallback callback, void* context)
{
    redfishService* ret;
    bool rc;

    //This does no network interactions when enumerate is false... use it because it's easier
    ret = createServiceEnumeratorNoAuth(host, rootUri, false, flags);
    if(ret == NULL)
    {
        return false;
    }
    ret->bearerToken = safeStrdup(token);
    rc = getVersionsAsync(ret, rootUri, callback, context);
    if(rc == false)
    {
        serviceDecRef(ret);
    }
    return rc;
}

static char* makeUrlForService(redfishService* service, const char* uri)
{
    char* url;
    if(service->host == NULL)
    {
        return NULL;
    }
    url = (char*)malloc(strlen(service->host)+strlen(uri)+1);
    strcpy(url, service->host);
    strcat(url, uri);
    return url;
}

static json_t* getVersions(redfishService* service, const char* rootUri)
{
    json_t* data;

    REDFISH_DEBUG_DEBUG_PRINT("%s: Entered. service = %p, rootUri = %s\n", __func__, service, rootUri);

    if(service->flags & REDFISH_FLAG_SERVICE_NO_VERSION_DOC)
    {
        service->versions = json_object();
        if(service->versions == NULL)
        {
            REDFISH_DEBUG_ERR_PRINT("%s: Error. Unable to allocate simple json object!\n", __func__);
            return NULL;
        }
        addStringToJsonObject(service->versions, "v1", "/redfish/v1");
        data = service->versions;
    }
    else if(rootUri != NULL)
    {
        data = getUriFromService(service, rootUri);
    }
    else
    {
        data = getUriFromService(service, "/redfish");
        if(data == NULL)
        {
            //Some redfish services don't respond here, but do respond at /redfish/
            data = getUriFromService(service, "/redfish/");
        }
    }
    REDFISH_DEBUG_DEBUG_PRINT("%s: Exited. data = %p\n", __func__, data);
    return data;
}

typedef struct {
    redfishService* service;
    redfishCreateAsyncCallback callback;
    void* context;
    bool rootUriProvided;
} getVersionsContext;

#ifdef _MSC_VER
threadRet __stdcall doCallbackInSeperateThread(void* data)
#else
threadRet doCallbackInSeperateThread(void* data)
#endif
{
    getVersionsContext* myContext = (getVersionsContext*)data;

    myContext->callback(myContext->service, myContext->context);
    free(myContext);
#ifdef _MSC_VER
	return 0;
#else
    pthread_exit(NULL);
    return NULL;
#endif
}

static void gotVersions(bool success, unsigned short httpCode, redfishPayload* payload, void* context)
{
    getVersionsContext* myContext = (getVersionsContext*)context;
    bool rc;
#ifndef _MSC_VER
    pthread_attr_t attr;
    pthread_t thread;
#endif

    (void)httpCode;

    if(success == false && myContext->rootUriProvided == true)
    {
        serviceDecRef(myContext->service);
        myContext->callback(NULL, myContext->context);
        free(myContext);
        return;
    }
    if(success == false)
    {
        //Some redfish services don't handle the URI /redfish correctly and need /redfish/
        myContext->rootUriProvided = true;
        rc = getUriFromServiceAsync(myContext->service, "/redfish/", NULL, gotVersions, myContext);
        if(rc == false)
        {
            serviceDecRef(myContext->service);
            myContext->callback(NULL, myContext->context);
            free(myContext);
        }
        return;
    }

    myContext->service->versions = payload->json;
    //Get rid of the payload's service reference...
    serviceDecRef(myContext->service);
    free(payload);
#ifndef _MSC_VER
    //In order to be more useful and let callers actually cleanup things in their callback we're doing this on a seperate thread...
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&thread, &attr, doCallbackInSeperateThread, myContext);
#else
	CreateThread(NULL, 0, doCallbackInSeperateThread, myContext, 0, NULL);
#endif

}

static bool getVersionsAsync(redfishService* service, const char* rootUri, redfishCreateAsyncCallback callback, void* context)
{
    bool rc;
    getVersionsContext* myContext;

    REDFISH_DEBUG_DEBUG_PRINT("%s: Entered. service = %p, rootUri = %s\n", __func__, service, rootUri);

    if(service->flags & REDFISH_FLAG_SERVICE_NO_VERSION_DOC)
    {
        service->versions = json_object();
        if(service->versions == NULL)
        {
            REDFISH_DEBUG_ERR_PRINT("%s: Error. Unable to allocate simple json object!\n", __func__);
            return false;
        }
        addStringToJsonObject(service->versions, "v1", "/redfish/v1");
        callback(service, context);
        return true;
    }

    myContext = malloc(sizeof(getVersionsContext));
    if(myContext == NULL)
    {
        serviceDecRef(service);
        return false;
    }
    myContext->service = service;
    myContext->callback = callback;
    myContext->context = context;
    myContext->rootUriProvided = (rootUri != NULL);

    if(rootUri != NULL)
    {
        rc = getUriFromServiceAsync(service, rootUri, NULL, gotVersions, myContext);
    }
    else
    {
        rc = getUriFromServiceAsync(service, "/redfish", NULL, gotVersions, myContext);
    }
    if(rc == false)
    {
        free(myContext);
    }
    return rc;
}

static char* getSSEUri(redfishService* service)
{
    redfishPayload* payload;
    char* tmp;
    char* ret = NULL;

    payload = getPayloadByPath(service, "/EventService/ServerSentEventUri");
    if(payload)
    {
        tmp = getPayloadStringValue(payload);
        cleanupPayload(payload);
        if(tmp)
        {
            ret = makeUrlForService(service, tmp);
            free(tmp);
        }
    }
    return ret;
}

static char* getEventSubscriptionUri(redfishService* service)
{
    redPathNode* redpath;
    redfishPayload* root;
    redfishPayload* eventSub;
    json_t*         odataId;
    char*           ret;

    redpath = parseRedPath("EventService/Subscriptions");
    if(!redpath)
    {
        return NULL;
    }
    root = getRedfishServiceRoot(service, NULL);
    if(root == NULL)
    {
        cleanupRedPath(redpath);
        return NULL;
    }
    eventSub = getPayloadForPath(root, redpath);
    cleanupPayload(root);
    cleanupRedPath(redpath);
    if(eventSub == NULL)
    {
        return NULL;
    }
    odataId = json_object_get(eventSub->json, "@odata.id");
    if(odataId == NULL)
    {
        cleanupPayload(eventSub);
        return NULL;
    }
    ret = safeStrdup(json_string_value(odataId));
    cleanupPayload(eventSub);
    return ret;
}

static void addStringToJsonArray(json_t* array, const char* value)
{
    json_t* jValue = json_string(value);

    json_array_append(array, jValue);

    json_decref(jValue);
}

static void addStringToJsonObject(json_t* object, const char* key, const char* value)
{
    json_t* jValue = json_string(value);

    json_object_set(object, key, jValue);

    json_decref(jValue);
}

static redfishPayload* getPayloadFromAsyncResponse(asyncHttpResponse* response, redfishService* service)
{
    httpHeader* header;
    size_t length = 0;
    char* type = NULL;

    if(response == NULL || response->bodySize == 0 || response->body == NULL)
    {
        REDFISH_DEBUG_ERR_PRINT("%s: Error, called without response data...\n", __func__);
        return NULL;
    }
    header = responseGetHeader(response, "Content-Length");
    if(header)
    {
        length = (size_t)strtoull(header->value, NULL, 10);
    }
    header = responseGetHeader(response, "Content-Type");
    if(header)
    {
        type = header->value;
    }
    return createRedfishPayloadFromContent(response->body, length, type, service);
}

static const unsigned char base64_table[65] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static unsigned char* base64_encode(const unsigned char* src, size_t len, size_t* out_len)
{
	unsigned char* out;
    unsigned char* pos;
	const unsigned char* end;
    const unsigned char* in;
	size_t olen;
	int line_len;

	olen = len * 4 / 3 + 4; /* 3-byte blocks to 4-byte */
	olen += olen / 72; /* line feeds */
	olen++; /* null termination */
	if (olen < len)
    {
        //Overflow
		return NULL;
    }
	out = malloc(olen);
	if (out == NULL)
    {
		return NULL;
    }

	end = src + len;
	in = src;
	pos = out;
	line_len = 0;
	while (end - in >= 3)
    {
		*pos++ = base64_table[in[0] >> 2];
		*pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
		*pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
		*pos++ = base64_table[in[2] & 0x3f];
		in += 3;
		line_len += 4;
	}

	if (end - in)
    {
		*pos++ = base64_table[in[0] >> 2];
		if (end - in == 1)
        {
			*pos++ = base64_table[(in[0] & 0x03) << 4];
			*pos++ = '=';
		}
        else
        {
			*pos++ = base64_table[((in[0] & 0x03) << 4) |
					      (in[1] >> 4)];
			*pos++ = base64_table[(in[1] & 0x0f) << 2];
		}
		*pos++ = '=';
		line_len += 4;
	}

	*pos = '\0';
	if (out_len)
    {
		*out_len = pos - out;
    }
	return out;
}

static char* getDestinationAddress(const char* addressInfo, SOCKET* socket)
{
    char* addressType = NULL;
    char* networkInterface = getStringTill(addressInfo, ":", &addressType);
    char* portStr = NULL;
    char* ret = NULL;
    char dest[1024];
    unsigned int port = 0;
    bool freeAddressType = false;
    if(addressType == NULL)
    {
        addressType = "ipv4";
    }
    else
    {
        addressType++;
        addressType = getStringTill(addressType, ":", &portStr);
        freeAddressType = true;
        if(portStr == NULL)
        {
            port = 0;
        }
        else
        {
            portStr++;
            port = strtoul(portStr, NULL, 10);
        }
    }
    if(strcmp(addressType, "ipv4") == 0)
    {
        ret = getIpv4Address(networkInterface);
    }
    else
    {
        ret = getIpv6Address(networkInterface);
    }
    free(networkInterface);
    if(freeAddressType)
    {
        free(addressType);
    }
    *socket = getSocket(ret, &port);
    if(strcmp(addressType, "ipv4") == 0)
    {
#ifdef HAVE_OPENSSL
        snprintf(dest, sizeof(dest)-1, "https://%s:%u", ret, port);
#else
        snprintf(dest, sizeof(dest)-1, "http://%s:%u", ret, port);
#endif
    }
    else
    {
        //Per RFC3986 IPv6 addresses in URIs need to be enclosed in []
#ifdef HAVE_OPENSSL
        snprintf(dest, sizeof(dest)-1, "https://[%s]:%u", ret, port);
#else
        snprintf(dest, sizeof(dest)-1, "http://[%s]:%u", ret, port);
#endif
    }
    free(ret);
    return safeStrdup(dest);
}
/* vim: set tabstop=4 shiftwidth=4 ff=unix expandtab: */
