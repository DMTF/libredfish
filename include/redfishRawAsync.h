//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017 Distributed Management Task Force, Inc. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#ifndef _REDFISH_RAW_ASYNC_H_
#define _REDFISH_RAW_ASYNC_H_

#include <redfishService.h>

typedef struct _httpHeader
{
    char* name;
    char* value;
    struct _httpHeader* next;
} httpHeader;

typedef enum
{
    GET,
    HEAD,
    POST,
    PUT,
    DELETE,
    OPTIONS,
    PATCH
} httpMethod;

typedef struct
{
    char* url;
    httpMethod method;
    httpHeader* headers;
    size_t bodySize;
    char* body;
} asyncHttpRequest;

typedef struct
{
    int connectError;
    long httpResponseCode; //Probably way too big, but this is curl's native type
    httpHeader* headers;
    size_t bodySize;
    char* body;
} asyncHttpResponse;

typedef void (*asyncRawCallback)(asyncHttpRequest* request, asyncHttpResponse* response, void* context);

REDFISH_EXPORT asyncHttpRequest* createRequest(const char* url, httpMethod method, size_t bodysize, char* body);
REDFISH_EXPORT void addRequestHeader(asyncHttpRequest* request, const char* name, const char* value);
REDFISH_EXPORT httpHeader* responseGetHeader(asyncHttpResponse* response, const char* name);
REDFISH_EXPORT bool startRawAsyncRequest(redfishService* service, asyncHttpRequest* request, asyncRawCallback callback, void* context);

REDFISH_EXPORT void freeAsyncRequest(asyncHttpRequest* request);
REDFISH_EXPORT void freeAsyncResponse(asyncHttpResponse* response);

#endif
