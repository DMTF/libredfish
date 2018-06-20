//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017 Distributed Management Task Force, Inc. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#ifndef _REDFISH_SERVICE_H_
#define _REDFISH_SERVICE_H_
#include <jansson.h>
#include <curl/curl.h>
#include <stdbool.h>

#ifdef _MSC_VER
//Windows
#define REDFISH_EXPORT __declspec(dllexport)
#else
//Linux
#define REDFISH_EXPORT
#include <pthread.h>
#endif

typedef struct {
    char* host;
    CURL* curl;
    json_t* versions;
    unsigned int flags;
    char* sessionToken;
    char* bearerToken;
#ifdef _MSC_VER
    HANDLE mutex;
#else
    pthread_mutex_t mutex;
#endif
} redfishService;

typedef struct {
    json_t* json;
    redfishService* service;
} redfishPayload;

#define REDFISH_AUTH_BASIC        0
#define REDFISH_AUTH_BEARER_TOKEN 1
#define REDFISH_AUTH_SESSION      2

typedef struct {
        unsigned int authType;
        union {
            struct {
                char* username;
                char* password;
            } userPass;
            struct {
                char* token;
            } authToken;
        } authCodes;
} enumeratorAuthentication;

/**
 * Callback for Redfish events
 *
 * @param event The redfishPayload from the event
 * @param auth The authentication data provided by the caller or null if none
 * @param context user specified value
 */
typedef void (*redfishEventCallback)(redfishPayload* event, enumeratorAuthentication* auth, const char* context);


//Values for eventTypes
#define REDFISH_EVENT_TYPE_STATUSCHANGE    0x00000001
#define REDFISH_EVENT_TYPE_RESOURCEUPDATED 0x00000002
#define REDFISH_EVENT_TYPE_RESOURCEADDED   0x00000004
#define REDFISH_EVENT_TYPE_RESOURCEREMOVED 0x00000008
#define REDFISH_EVENT_TYPE_ALERT           0x00000010

#define REDFISH_EVENT_TYPE_ALL             0x0000001F

//Values for flags
#define REDFISH_FLAG_SERVICE_NO_VERSION_DOC 0x00000001 //The Redfish Service lacks the version document (in violation of the Redfish spec)


REDFISH_EXPORT redfishService* createServiceEnumerator(const char* host, const char* rootUri, enumeratorAuthentication* auth, unsigned int flags);
REDFISH_EXPORT json_t* getUriFromService(redfishService* service, const char* uri);
REDFISH_EXPORT json_t* patchUriFromService(redfishService* service, const char* uri, const char* content);
REDFISH_EXPORT json_t* postUriFromService(redfishService* service, const char* uri, const char* content, size_t contentLength, const char* contentType);
REDFISH_EXPORT bool    deleteUriFromService(redfishService* service, const char* uri);
REDFISH_EXPORT bool    registerForEvents(redfishService* service, const char* postbackUri, unsigned int eventTypes, redfishEventCallback callback, const char* context);
REDFISH_EXPORT redfishPayload* getRedfishServiceRoot(redfishService* service, const char* version);
REDFISH_EXPORT redfishPayload* getPayloadByPath(redfishService* service, const char* path);
REDFISH_EXPORT void cleanupServiceEnumerator(redfishService* service);

//Async API
#define HTTP_GET    0
#define HTTP_PUT    1
#define HTTP_PATCH  2
#define HTTP_POST   3
#define HTTP_DELETE 4

typedef struct {
    unsigned int operation;
    const char*  uri;
    size_t       headerCount;
    char**       headers;
    char*        body;
} httpRequest;

typedef struct {
    size_t       headerCount;
    char**       headers;
    char*        body;
    size_t       responseCode;
} httpResponse;

typedef struct {
    unsigned int operation;
    const char*  uri;
    size_t       headerCount;
    char**       headers;
    json_t*      body;
} redfishRequest;

typedef struct {
    size_t       headerCount;
    char**       headers;
    json_t*      body;
    size_t       responseCode;
} redfishResponse;

typedef void (*redfishRawHttpCallback)(redfishService* service, httpResponse* response, void* context);
typedef void (*redfishHttpCallback)(redfishService* service, redfishResponse* response, void* context);

REDFISH_EXPORT int     asyncRawSendHTTPOperation(redfishService* service, httpRequest* request, redfishRawHttpCallback callback, void* context);
REDFISH_EXPORT int     asyncRedfishSendHTTPOperation(redfishService* service, redfishRequest* request, redfishHttpCallback callback, void* context);
REDFISH_EXPORT int     asyncRedfishSimpleGet(redfishService* service, const char* uri, redfishHttpCallback callback, void* context);
REDFISH_EXPORT int     asyncRedfishSimplePut(redfishService* service, const char* uri, json_t* body, redfishHttpCallback callback, void* context);
REDFISH_EXPORT int     asyncRedfishSimplePatch(redfishService* service, const char* uri, json_t* body, redfishHttpCallback callback, void* context);
REDFISH_EXPORT int     asyncRedfishSimplePost(redfishService* service, const char* uri, json_t* body, redfishHttpCallback callback, void* context);
REDFISH_EXPORT int     asyncRedfishSimpleDelete(redfishService* service, const char* uri, redfishHttpCallback callback, void* context);

void deleteRawRequest(httpRequest* request);
void deleteRawResponse(httpResponse* response);

void deleteRedfishRequest(redfishRequest* request);
void deleteRedfishResponse(redfishResponse* response);


#endif
