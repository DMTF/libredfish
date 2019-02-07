//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------

/**
 * @file redfishRawAsync.h
 * @author Patrick Boyd
 * @brief File containing the interface for the asynchronous HTTP function.
 *
 * This file explains the interface for the asynchronous raw HTTP(s) calls. These function do no do
 * any extra processing for redfish including adding authentication information. Therefore, these should
 * only be used if the normal redfish calls cannot function.
 */
#ifndef _REDFISH_RAW_ASYNC_H_
#define _REDFISH_RAW_ASYNC_H_

#include <redfishService.h>

/**
 * @brief A representation of an HTTP header.
 *
 * An HTTP header structure list.
 */
typedef struct _httpHeader
{
    /** The header name **/
    char* name;
    /** The header value **/
    char* value;
    /** A link to the next header in the list **/
    struct _httpHeader* next;
} httpHeader;

/**
 * @brief An HTTP method.
 *
 * A representation of an HTTP method.
 */
typedef enum _httpMethod
{
    /** Get an HTTP resource **/
	HTTP_GET,
    /** Get the headers for an HTTP resource **/
	HTTP_HEAD,
    /** Write/Create an HTTP resource **/
	HTTP_POST,
    /** Write an HTTP resource **/
	HTTP_PUT,
    /** Delete an HTTP resource **/
    HTTP_DELETE,
    /** Get the communication options for an HTTP resource **/
	HTTP_OPTIONS,
    /** Write part of an HTTP resource **/
	HTTP_PATCH
} httpMethod;

/**
 * @brief An asynchronous HTTP(s) request.
 *
 * A structure with all the information needed to start an HTTP(s) request.
 */
typedef struct _asyncHttpRequest
{
    /** The url to send the request to **/
    char* url;
    /** The HTTP method to use **/
    httpMethod method;
    /** The timeout for the operation, 0 means never timeout **/
    unsigned long timeout;
    /** Headers to send or NULL for none **/
    httpHeader* headers;
    /** The size of the request payload body **/
    size_t bodySize;
    /** The request payload body. char* is used for convience. Binary data can be passed and the bodySize parameter dictates the length **/
    char* body;
} asyncHttpRequest;

/**
 * @brief An asynchronous HTTP(s) response.
 *
 * A structure with all the information returned by the server.
 */
typedef struct _asyncHttpResponse
{
    /** 0 on success, another value if the connection failed **/
    int connectError;
    /** The HTTP response code sent by the server **/
    long httpResponseCode; //Probably way too big, but this is curl's native type
    /** Headers sent by the server **/
    httpHeader* headers;
    /** The size of the body of the payload sent by the server **/
    size_t bodySize;
    /** The response payload body. char* is used for convience. Binary data can be passed and the bodySize parameter dictates the length **/
    char* body;
} asyncHttpResponse;

/**
 * @brief A callback when the request is finished.
 *
 *  A function callback called when the request has finished.
 *
 * @param request The request that was sent. The function should free this.
 * @param response The response that was received. The function should free this.
 * @param context The context that was passed to startRawAsyncRequest. It is up to the consumer to determine if this should be freed or not
 * @see startRawAsyncRequest
 */
typedef void (*asyncRawCallback)(asyncHttpRequest* request, asyncHttpResponse* response, void* context);

/**
 * @brief Create an asyncHttpRequest struct.
 *
 * This method creates an asyncHttpRequest struct and initializes the values per the parameters.
 *
 * @param url The url to send the request to. This should be the full URL with protocol schema.
 * @param method The HTTP method to use
 * @param bodysize The size of the HTTP body payload
 * @param body The HTTP body payload to send
 * @return NULL on failure, otherwise an asyncHttpRequest pointer
 * @see freeAsyncRequest
 */
REDFISH_EXPORT asyncHttpRequest* createRequest(const char* url, httpMethod method, size_t bodysize, char* body);
/**
 * @brief Add a new header to an existing request.
 *
 * This method adds an HTTP header to the asyncHttpRequest struct.
 *
 * @param request The request to add the header to.
 * @param name The HTTP header name
 * @param value The HTTP header value
 */
REDFISH_EXPORT void addRequestHeader(asyncHttpRequest* request, const char* name, const char* value);
/**
 * @brief Finds a header in the response.
 *
 * This method finds an HTTP header in the asyncHttpResponse struct.
 *
 * @param response The response to search.
 * @param name The HTTP header name
 * @return NULL if not found, an httpHeader* if found
 */
REDFISH_EXPORT httpHeader* responseGetHeader(asyncHttpResponse* response, const char* name);
/**
 * @brief Start an async request.
 *
 * This method will start an async request. It will initialize the queue and start the async thread if they have not already been initilized.
 *
 * @param service The redfish service used. This is used to maintain a queue and thread per service.
 * @param request The request to send
 * @param callback The function to call upon completion
 * @param context An opaque pointer to pass to the callback
 * @return false if the request could not be initiated, true otherwise
 */
REDFISH_EXPORT bool startRawAsyncRequest(redfishService* service, asyncHttpRequest* request, asyncRawCallback callback, void* context);

/**
 * @brief Free an asyncHttpRequest struct.
 *
 * This method frees an asyncHttpRequest struct and all associated values.
 *
 * @param request The request to free.
 * @see createRequest
 */
REDFISH_EXPORT void freeAsyncRequest(asyncHttpRequest* request);
/**
 * @brief Free an asyncHttpResponse struct.
 *
 * This method frees an asyncHttpResponse struct and all associated values.
 *
 * @param response The response to free.
 */
REDFISH_EXPORT void freeAsyncResponse(asyncHttpResponse* response);

#endif
