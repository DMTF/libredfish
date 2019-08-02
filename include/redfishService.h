//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017-2019 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------

/**
 * @file redfishService.h
 * @author Patrick Boyd
 * @brief File containing the interface for the service level interactions.
 *
 * This file explains the interface for interacting with a remote Redfish service.
 */
#ifndef _REDFISH_SERVICE_H_
#define _REDFISH_SERVICE_H_
#include <jansson.h>
#include <curl/curl.h>
#include <stdbool.h>

#ifdef _MSC_VER
//Windows
/** This function should be exposed outside the library **/
#define REDFISH_EXPORT __declspec(dllexport)
#else
//Linux
/** This function should be exposed outside the library **/
#define REDFISH_EXPORT
#include <pthread.h>
#endif

#ifndef _INT_SERVICE_H_
/** The redfish service data **/
typedef struct _redfishService redfishService;
#endif

/**
 * @brief Content type of the redfishPayload object
 */
typedef enum {
    /** Payload is JSON **/
    PAYLOAD_CONTENT_JSON,
    /** Payload is something else **/
    PAYLOAD_CONTENT_OTHER = 0xFF
} redfishContentType;

/**
 * @brief A representation of a redfish payload
 *
 * A structure representing a redfish payload to send or receive
 */
typedef struct {
    /** The json for the payload. Only valid if contentType is PAYLOAD_CONTENT_JSON **/
    json_t* json;
    /** The redfish service this payload was created by or should be sent to **/
    redfishService* service;
    /** The raw payload content. Only valid if contentType is PAYLOAD_CONTENT_OTHER **/
    char* content;
    /** The raw payload lengh. Only valid if contentType is PAYLOAD_CONTENT_OTHER **/
    size_t contentLength;
    /** The content type of the payload **/
    redfishContentType contentType;
    /** The content type string. Only valid if contentType is PAYLOAD_CONTENT_OTHER **/
    char* contentTypeStr;
} redfishPayload;

/** The connection should use HTTP basic authentication to authenticate to the Redfish service**/
#define REDFISH_AUTH_BASIC        0
/** The connection should use a bearer token to authenticate to the Redfish service **/
#define REDFISH_AUTH_BEARER_TOKEN 1
/** The connection should use Redfish service authentication as documented in the Redfish specification to authenticate to the Redfish service **/
#define REDFISH_AUTH_SESSION      2

/**
 * @brief A representation of how to authenticate to the service
 *
 * A structure representing redfish authentication methods
 */
typedef struct {
        /** The authentication type to use **/
        unsigned int authType;
        /** The actual authentication codes **/
        union {
            /** The data used for REDFISH_AUTH_BASIC or REDFISH_AUTH_SESSION **/
            struct {
                /** The username to authenticate with **/
                char* username;
                /** The password to authenticate with **/
                char* password;
            } userPass;
            /** The data used for REDFISH_AUTH_BEARER_TOKEN **/
            struct {
                /** The bearer token to authenticate with **/
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
/** A value corresponding to EventTypes.StatusChange **/
#define REDFISH_EVENT_TYPE_STATUSCHANGE    0x00000001
/** A value corresponding to EventTypes.ResourceUpdated **/
#define REDFISH_EVENT_TYPE_RESOURCEUPDATED 0x00000002
/** A value corresponding to EventTypes.ResourceAdded **/
#define REDFISH_EVENT_TYPE_RESOURCEADDED   0x00000004
/** A value corresponding to EventTypes.ResourceRemoved **/
#define REDFISH_EVENT_TYPE_RESOURCEREMOVED 0x00000008
/** A value corresponding to EventTypes.Alert **/
#define REDFISH_EVENT_TYPE_ALERT           0x00000010

/** A value corresponding to all currrently known event types **/
#define REDFISH_EVENT_TYPE_ALL             0x0000001F

//Values for flags
/** A flag used to indicate that the Redfish Service lacks the version document (in violation of the Redfish spec) **/
#define REDFISH_FLAG_SERVICE_NO_VERSION_DOC 0x00000001
/** A flag used to indicate that the Redfish Service is not RFC compliant in terms of issuing Redirects **/
#define REDFISH_FLAG_SERVICE_BAD_REDIRECTS  0x00000002

/**
 * @brief Create a redfish service connection.
 *
 * Create a new redfish service connection.
 *
 * Host examples:
 * Connection Type | Host Identity Type | Example
 * --------------- | ------------------ | ----------------------------------
 * HTTP            | Domain Name        | http://example.com
 * HTTPS           | Domain Name        | https://example.com
 * HTTP            | IPv4               | http://127.0.0.1
 * HTTPS           | IPv4               | https://127.0.0.1
 * HTTP            | IPv6               | http://[::1]
 * HTTPS           | IPv6               | https://[::1]
 *
 * @param host The host to connect to. This must contain the protocol schema see above for more details.
 * @param rootUri The root URI of the redfish service. If NULL the connection with assume /redfish
 * @param auth The authentication method to use for the redfish service. If NULL the connection will be made with no authentication
 * @param flags Any extra flags to pass to the service
 * @return A new redfish service structure representing the connection.
 * @see serviceDecRef
 * @see serviceDecRefAndWait
 */
REDFISH_EXPORT redfishService* createServiceEnumerator(const char* host, const char* rootUri, enumeratorAuthentication* auth, unsigned int flags);
/**
 * @brief Obtain the JSON payload corresponding to the given URI on the service.
 *
 * Obtain the JSON payload corresponding to the given URI on the service synchronously.
 *
 * @param service The service to obtain data from
 * @param uri The URI to obtain data from (must contain the full URI for the service, i.e. should start with /redfish normally)
 * @return A json representation of the URI content or NULL on error
 * @see getUriFromServiceAsync
 * @deprecated This function if just a wrapper for the async API getUriFromServiceAsync and will not be updated to handle non-JSON returns or
 * other features of the newer API.
 */
REDFISH_EXPORT json_t* getUriFromService(redfishService* service, const char* uri);
/**
 * @brief Use a PATCH operation on a given URI.
 *
 * Send a patch operation to the given URI on the service synchronously.
 *
 * @param service The service to send data to
 * @param uri The URI to send data to (must contain the full URI for the service, i.e. should start with /redfish normally)
 * @param content The content to send
 * @return A json representation of the URI content or NULL on error
 * @see patchUriFromServiceAsync
 * @deprecated This function if just a wrapper for the async API patchUriFromServiceAsync and will not be updated to handle non-JSON returns or
 * other features of the newer API.
 */
REDFISH_EXPORT json_t* patchUriFromService(redfishService* service, const char* uri, const char* content);
/**
 * @brief Use a POST operation on a given URI.
 *
 * Send a POST operation to the given URI on the service synchronously.
 *
 * @param service The service to send data to
 * @param uri The URI to send data to (must contain the full URI for the service, i.e. should start with /redfish normally)
 * @param content The content to send
 * @param contentLength The size of the content buffer
 * @param contentType The contentType header to send
 * @return A json representation of the URI content or NULL on error
 * @see postUriFromServiceAsync
 * @deprecated This function if just a wrapper for the async API postUriFromServiceAsync and will not be updated to handle non-JSON returns or
 * other features of the newer API.
 */
REDFISH_EXPORT json_t* postUriFromService(redfishService* service, const char* uri, const char* content, size_t contentLength, const char* contentType);
/**
 * @brief Use a DELETE operation on a given URI.
 *
 * Send a DELETE operation to the given URI on the service synchronously.
 *
 * @param service The service to send data to
 * @param uri The URI to send data to (must contain the full URI for the service, i.e. should start with /redfish normally)
 * @return A boolean indicating success or failure
 * @see deleteUriFromServiceAsync
 * @deprecated This function if just a wrapper for the async API deleteUriFromServiceAsync and will not be updated for other features of the newer API.
 */
REDFISH_EXPORT bool    deleteUriFromService(redfishService* service, const char* uri);
/**
 * @brief Register for notification of async redfish events.
 *
 * Register for notification of async redfish events.
 *
 * Postback examples:
 * If the postbackUri starts with libredfish: then libredfish will create a socket to listen to events directly.
 * The format for this is libredfish:<interface name>[:<ipv4/ipv6>[:<port>]]
 * To use this format the caller must supply the name of the interface to listen on (libredfish will aquire the interfaces IP to send to the redfish server)
 * By default IPv4 and a random port number will be used. However a caller can specify to listen on IPv6 and specify a port number if desired.
 *
 * @param service The service to obtain events from
 * @param postbackUri The URI for the redfish service to send events to. This must point to an event listener that can send events back to this process. See the httpd/cgi.c for an example
 * @param eventTypes The event types to be notified about
 * @param callback The function to call upon reciept of an event
 * @param context An event context to pass to the Redfish Service
 * @return A boolean indicating success or failure
 */
REDFISH_EXPORT bool    registerForEvents(redfishService* service, const char* postbackUri, unsigned int eventTypes, redfishEventCallback callback, const char* context);
/**
 * @brief Obtain the redfish service root.
 *
 * Obtain the redfish service root on the service synchronously.
 *
 * @param service The service to obtain data from
 * @param version The redfish version to obtain the root of. Assumes "v1" if NULL.
 * @return A payload representing the Service Root
 * @see getRedfishServiceRootAsync
 * @deprecated This function if just a wrapper for the async API getRedfishServiceRootAsync and will not be updated for other features of the newer API.
 */
REDFISH_EXPORT redfishPayload* getRedfishServiceRoot(redfishService* service, const char* version);
/**
 * @brief Obtain the redfish payload corresponding to the redpath.
 *
 * Obtain the redfish payload corresponding to the redpath on the service synchronously.
 *
 * @param service The service to obtain data from
 * @param path A redpath string to use to locate data.
 * @return A payload representing the data at the path or NULL on error
 * @see getPayloadByPathAsync
 * @deprecated This function if just a wrapper for the async API getPayloadByPathAsync and will not be updated for other features of the newer API.
 */
REDFISH_EXPORT redfishPayload* getPayloadByPath(redfishService* service, const char* path);
/**
 * @brief Free the service connection.
 *
 * Free the redfish service connection object and associated data.
 *
 * @param service The service to free
 * @see serviceDecRef
 * @see serviceDecRefAndWait
 * @deprecated This function if just a wrapper for the async API serviceDecRef.
 */
REDFISH_EXPORT void cleanupServiceEnumerator(redfishService* service);

/**
 * @brief Indicate that this connection is in use.
 *
 * Indicate that this connection is in use. This will increase the internal reference count of the service.
 *
 * @param service The service to indicate is in use
 * @see createServiceEnumerator
 * @see serviceDecRef
 * @see serviceDecRefAndWait
 */
REDFISH_EXPORT void serviceIncRef(redfishService* service);
/**
 * @brief Indicate that this connection is no longer in use.
 *
 * Indicate that this connection is no longer in use. Once the internal reference count reaches 0 the assocated data will be freed.
 * Therefore this function or serviceDecRefAndWait should be called an equivalent number of times to serviceIncRef -1 (as the initial create
 * will set the count to 1).
 *
 * @param service The service to indicate is no longer in use
 * @see createServiceEnumerator
 * @see serviceIncRef
 * @see serviceDecRefAndWait
 */
REDFISH_EXPORT void serviceDecRef(redfishService* service);
/**
 * @brief Indicate that this connection is no longer in use.
 *
 * Indicate that this connection is no longer in use and wait until no one else is using it either. This function is similar to
 * serviceDecRef. However, it will wait until all outstanding requests are processed before returning.
 *
 * @param service The service to indicate is no longer in use
 * @see createServiceEnumerator
 * @see serviceIncRef
 * @see serviceDecRef
 */
REDFISH_EXPORT void serviceDecRefAndWait(redfishService* service);

/** There was an error parsing the returned payload **/
#define REDFISH_ERROR_PARSING 0xFFFE

/** Accept any type of response **/
#define REDFISH_ACCEPT_ALL  0xFFFFFFFF
/** Accept a JSON response **/
#define REDFISH_ACCEPT_JSON 1
/** Accept an XML response **/
#define REDFISH_ACCEPT_XML  2

/** Try Registering for events through SSE, if supported will be tried first **/
#define REDFISH_REG_TYPE_SSE  1
/** Try Registering for events through EventDestination POST **/
#define REDFISH_REG_TYPE_POST 2

/** Get the IPv4 address **/
#define REDFISH_REG_IP_TYPE_4 4
/** Get the IPv6 address **/
#define REDFISH_REG_IP_TYPE_6 6

#ifndef _MSC_VER
/** Make Unix and Windows use same type for sockets **/
typedef int SOCKET;
#endif

/** An already opened socket, provided in the socket field **/
#define REDFISH_EVENT_FRONT_END_OPEN_SOCKET 1
/** Open a TCP socket, socketInterface and socketIPType should be specified, socketPort may be specified if not random port will be used **/
#define REDFISH_EVENT_FRONT_END_TCP_SOCKET  2
/** Open a SSL/TLS socket, socketInterface and socketIPType should be specified, socketPort may be specified if not random port will be used **/
#define REDFISH_EVENT_FRONT_END_SSL_SOCKET  3
/** Open a POSIX domain socket, socketName should be specified **/
#define REDFISH_EVENT_FRONT_END_DOMAIN_SOCKET 4

/** Extra async options for the call **/
typedef struct
{
    /** The type of response payload to accept **/
    int accept;
    /** The timeout for the operation, 0 means never timeout **/
    unsigned long timeout;
} redfishAsyncOptions;

typedef struct
{
    /** Event Registration Types to try **/
    int regTypes;
    /** The context to send **/
    char* context;
    /** POST Back URI (only valid if regTypes contains REDFISH_REG_TYPE_POST), to have libredfish obtain interface IP use %s **/
    char* postBackURI;
    /** POST Back Interface IP Type (only valid if regTypes contains REDFISH_REG_TYPE_POST and postBackURI contains %s), the type of IP to obtain **/
    int postBackInterfaceIPType;
    /** POST Back Interface (only valid if regTypes contains REDFISH_REG_TYPE_POST and postBackURI contains %s), the interface to obtain the IP for **/
    char* postBackInterface;
} redfishEventRegistration;

typedef struct
{
    /** The type of font end to use */
    int frontEndType;
    /** An already open socket pointer to use. Only valid if frontEndType is REDFISH_EVENT_FRONT_END_OPEN_SOCKET **/
    SOCKET socket;
    /** The IP Address type to use. Only valid if frontEndType is REDFISH_EVENT_FRONT_END_TCP_SOCKET or REDFISH_EVENT_FRONT_END_SSL_SOCKET **/
    int socketIPType;
    /** The IP interface to use, if NULL is specified then the library will listen on all. Only valid if frontEndType is REDFISH_EVENT_FRONT_END_TCP_SOCKET or REDFISH_EVENT_FRONT_END_SSL_SOCKET **/
    char* socketInterface;
    /** The TCP port to use, if 0 is specified then the library will use a random open port. Only valid if frontEndType is REDFISH_EVENT_FRONT_END_TCP_SOCKET or REDFISH_EVENT_FRONT_END_SSL_SOCKET **/
    unsigned int socketPort;
    /** The name of the socket to use. Only valid if frontEndType is REDFISH_EVENT_FRONT_END_DOMAIN_SOCKET **/
    char* socketName;
} redfishEventFrontEnd;

typedef void (*redfishCreateAsyncCallback)(redfishService* service, void* context);

/**
 * @brief Create a redfish service connection asynchronously.
 *
 * Create a new redfish service connection asynchronously.
 *
 * Host examples:
 * Connection Type | Host Identity Type | Example
 * --------------- | ------------------ | ----------------------------------
 * HTTP            | Domain Name        | http://example.com
 * HTTPS           | Domain Name        | https://example.com
 * HTTP            | IPv4               | http://127.0.0.1
 * HTTPS           | IPv4               | https://127.0.0.1
 * HTTP            | IPv6               | http://[::1]
 * HTTPS           | IPv6               | https://[::1]
 *
 * @param host The host to connect to. This must contain the protocol schema see above for more details.
 * @param rootUri The root URI of the redfish service. If NULL the connection with assume /redfish
 * @param auth The authentication method to use for the redfish service. If NULL the connection will be made with no authentication
 * @param flags Any extra flags to pass to the service
 * @param callback The callback to call when the service is created
 * @param context The context to pass to the callback
 * @return True if the callback will be called, false otherwise.
 * @see createServiceEnumerator
 * @see serviceDecRef
 * @see serviceDecRefAndWait
 */
REDFISH_EXPORT bool createServiceEnumeratorAsync(const char* host, const char* rootUri, enumeratorAuthentication* auth, unsigned int flags, redfishCreateAsyncCallback callback, void* context);

/**
 * Callback for Redfish async calls
 *
 * @param success Indicates if the call was successful or not
 * @param httpCode The HTTP status code returned by the service
 * @param payload The redfish payload returned from the call
 * @param context An opaque pointer sent to the original call
 */
typedef void (*redfishAsyncCallback)(bool success, unsigned short httpCode, redfishPayload* payload, void* context);

/**
 * @brief Obtain the redfish payload corresponding to the given URI on the service.
 *
 * Obtain the redfish payload corresponding to the given URI on the service asynchronously.
 *
 * @param service The service to obtain data from
 * @param uri The URI to obtain data from (must contain the full URI for the service, i.e. should start with /redfish normally)
 * @param options Options to use for this request or any subsequent requests triggered by this request. If NULL a resonable set of defaults will be used
 * @param callback A function to call upon completion of the request
 * @param context An opaque data pointer to pass to the callback function
 * @return false if the request could not be started. True otherwise
 */
REDFISH_EXPORT bool getUriFromServiceAsync(redfishService* service, const char* uri, redfishAsyncOptions* options, redfishAsyncCallback callback, void* context);
/**
 * @brief Initiate a PATCH to the given URI on the service.
 *
 * Initiate a PATCH operation to the given URI on the service and asynchronously obtain the resulting payload
 *
 * @param service The service to send data to
 * @param uri The URI to send data to (must contain the full URI for the service, i.e. should start with /redfish normally)
 * @param payload The payload to send to the URI
 * @param options Options to use for this request or any subsequent requests triggered by this request. If NULL a resonable set of defaults will be used
 * @param callback A function to call upon completion of the request
 * @param context An opaque data pointer to pass to the callback function
 * @return false if the request could not be started. True otherwise
 */
REDFISH_EXPORT bool patchUriFromServiceAsync(redfishService* service, const char* uri, redfishPayload* payload, redfishAsyncOptions* options, redfishAsyncCallback callback, void* context);
/**
 * @brief Initiate a POST to the given URI on the service.
 *
 * Initiate a POST operation to the given URI on the service and asynchronously obtain the resulting payload
 *
 * @param service The service to send data to
 * @param uri The URI to send data to (must contain the full URI for the service, i.e. should start with /redfish normally)
 * @param payload The payload to send to the URI
 * @param options Options to use for this request or any subsequent requests triggered by this request. If NULL a resonable set of defaults will be used
 * @param callback A function to call upon completion of the request
 * @param context An opaque data pointer to pass to the callback function
 * @return false if the request could not be started. True otherwise
 */
REDFISH_EXPORT bool postUriFromServiceAsync(redfishService* service, const char* uri, redfishPayload* payload, redfishAsyncOptions* options, redfishAsyncCallback callback, void* context);
/**
 * @brief Initiate a DELETE to the given URI on the service.
 *
 * Initiate a DELETE operation to the given URI on the service and asynchronously obtain the resulting payload
 *
 * @param service The service to send data to
 * @param uri The URI to send data to (must contain the full URI for the service, i.e. should start with /redfish normally)
 * @param options Options to use for this request or any subsequent requests triggered by this request. If NULL a resonable set of defaults will be used
 * @param callback A function to call upon completion of the request
 * @param context An opaque data pointer to pass to the callback function
 * @return false if the request could not be started. True otherwise
 */
REDFISH_EXPORT bool deleteUriFromServiceAsync(redfishService* service, const char* uri, redfishAsyncOptions* options, redfishAsyncCallback callback, void* context);

/**
 * @brief Obtain the redfish service root.
 *
 * Obtain the redfish service root on the service asynchronously.
 *
 * @param service The service to obtain data from
 * @param version The redfish version to obtain the root of. Assumes "v1" if NULL.
 * @param options Options to use for this request or any subsequent requests triggered by this request. If NULL a resonable set of defaults will be used
 * @param callback A function to call upon completion of the request
 * @param context An opaque data pointer to pass to the callback function
 * @return false if the request could not be started. True otherwise
 */
REDFISH_EXPORT bool getRedfishServiceRootAsync(redfishService* service, const char* version, redfishAsyncOptions* options, redfishAsyncCallback callback, void* context);
/**
 * @brief Obtain the redfish payload corresponding to the redpath.
 *
 * Obtain the redfish payload corresponding to the redpath on the service asynchronously.
 *
 * @param service The service to obtain data from
 * @param path A redpath string to use to locate data.
 * @param options Options to use for this request or any subsequent requests triggered by this request. If NULL a resonable set of defaults will be used
 * @param callback A function to call upon completion of the request
 * @param context An opaque data pointer to pass to the callback function
 * @return false if the request could not be started. True otherwise
 */
REDFISH_EXPORT bool getPayloadByPathAsync(redfishService* service, const char* path, redfishAsyncOptions* options, redfishAsyncCallback callback, void* context);
/**
 * @brief Register for notification of async redfish events, with the registration done asynchronously.
 *
 * Register for notification of async redfish events, with the registration done asynchronously.
 *
 * @param service The service to obtain events from
 * @param registration The type and information needed for the registration
 * @param frontend Information on how libredfish will recieve the events
 * @param callback The function to call upon reciept of an event

 * @return A boolean indicating success or failure
 */
REDFISH_EXPORT bool    registerForEventsAsync(redfishService* service, redfishEventRegistration* registration, redfishEventFrontEnd* frontend, redfishEventCallback callback);

#endif
