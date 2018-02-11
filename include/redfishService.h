//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017 Distributed Management Task Force, Inc. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/LICENSE.md
//----------------------------------------------------------------------------
#ifndef _REDFISH_SERVICE_H_
#define _REDFISH_SERVICE_H_
#include <jansson.h>
#include <curl/curl.h>
#include <stdbool.h>

#ifdef _MSC_VER
#define REDFISH_EXPORT __declspec(dllexport)
#else
#define REDFISH_EXPORT
#endif

typedef struct {
    char* host;
    CURL* curl;
    json_t* versions;
    unsigned int flags;
    char* sessionToken;
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

#endif
