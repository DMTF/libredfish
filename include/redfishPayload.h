//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2018 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------

/**
 * @file redfishPayload.h
 * @author Patrick Boyd
 * @brief File containing the interface for payload level interactions.
 *
 * This file explains the interface for the payload level interactions with a redfish service
 */
#ifndef _REDFISH_PAYLOAD_H_
#define _REDFISH_PAYLOAD_H_

//redfishPayload is defined here...
#include "redfishService.h"

#include "redpath.h"

/**
 * @brief Create a redfish payload from json and a service
 *
 * Create a new redfish payload representation from a json_t* and a service
 *
 * @param value The json value that is the payload content
 * @param service The redfish service for this payload
 * @return A new redfish payload structure
 * @see cleanupPayload
 */
REDFISH_EXPORT redfishPayload* createRedfishPayload(json_t* value, redfishService* service);
/**
 * @brief Create a redfish payload from a string and a service
 *
 * Create a new redfish payload representation from a char* and a service
 *
 * @param value The json string value that is the payload content
 * @param service The redfish service for this payload
 * @return A new redfish payload structure
 * @see cleanupPayload
 */
REDFISH_EXPORT redfishPayload* createRedfishPayloadFromString(const char* value, redfishService* service);
/**
 * @brief Create a redfish payload from a HTTP response
 *
 * Create a new redfish payload representation from data obtained from an HTTP response
 *
 * @param content The content for the payload. If the contentType indicates JSON this will be automatically parsed.
 * @param contentLength The length of the content buffer
 * @param contentType The type of the content according to the HTTP server
 * @param service The redfish service for this payload
 * @return A new redfish payload structure
 * @see cleanupPayload
 */
REDFISH_EXPORT redfishPayload* createRedfishPayloadFromContent(const char* content, size_t contentLength, const char* contentType, redfishService* service);

/**
 * @brief Is the payload a Redfish Collection?
 *
 * Is the payload a Redfish Collection or not?
 * While OData uses the term collection pretty loosly and would call an array a collection. This function only returns true on Redfish Collections (i.e. something that is a ResourceCollection).
 *
 * @param payload The payload to check
 * @return True if the payload is a Redfish Collection, False otherwise
 */
REDFISH_EXPORT bool            isPayloadCollection(redfishPayload* payload);
/**
 * @brief Is the payload an array?
 *
 * Is the payload an array?
 * While OData uses the term collection pretty loosly and would call an array a collection. This function only returns true on arrays.
 *
 * @param payload The payload to check
 * @return True if the payload is an array, False otherwise
 */
REDFISH_EXPORT bool            isPayloadArray(redfishPayload* payload);

/**
 * @brief Get the size of the payload if it were represented as a character buffer
 *
 * Return the size of the payload if it were represented as a character buffer (i.e. to be sent over the network)
 *
 * @param payload The payload to check
 * @return The size of the payload in bytes
 */
REDFISH_EXPORT size_t          getPayloadSize(redfishPayload* payload);
/**
 * @brief Get the payload as a character buffer
 *
 * Return the payload represented as a character buffer (i.e. to be sent over the network)
 *
 * @param payload The payload to check
 * @return The payload as a character buffer
 */
REDFISH_EXPORT char*           getPayloadBody(redfishPayload* payload);
/**
 * @brief Get the payload content type
 *
 * Get the payload content type as specified in RFC7231
 *
 * @param payload The payload to check
 * @return The content type string
 */
REDFISH_EXPORT char*           getPayloadContentType(redfishPayload* payload);

/**
 * @brief Get the payload uri
 *
 * Get the payload uri (the @odata.id)
 *
 * @param payload The payload to return the URI of
 * @return The uri
 */
REDFISH_EXPORT char*           getPayloadUri(redfishPayload* payload);

/**
 * @brief Get the string value for the payload
 *
 * If the value of the payload is not a string (i.e. is a number, object, array, etc.) Return NULL. Otherwise return the string contained in the payload
 *
 * @param payload The payload to get the string value of
 * @return string contained in the payload or NULL if not a string entity
 */
REDFISH_EXPORT char*           getPayloadStringValue(redfishPayload* payload);
/**
 * @brief Get the int value for the payload
 *
 * If the value of the payload is not an int (i.e. is a float, string, object, array, etc.) Return 0. Otherwise return the int contained in the payload
 *
 * @param payload The payload to get the int value of
 * @return int contained in the payload or 0 if not a string entity
 */
REDFISH_EXPORT int             getPayloadIntValue(redfishPayload* payload);

/**
 * @brief Obtain the node in the payload identified by the specified nodeName
 *
 * Obtain the node in the payload identified by the specified nodeName. If the node in question is a navigation property to another URI then the 
 * content of that URI will be obtained synchronously.
 *
 * @param payload The payload to get the child node of
 * @param nodeName The name of the node to obtain
 * @return The child node or NULL if it does not exist or could not be obtained.
 * @see getPayloadByNodeNameAsync
 * @see getPayloadByNodeNameNoNetwork
 */
REDFISH_EXPORT redfishPayload* getPayloadByNodeName(redfishPayload* payload, const char* nodeName);
/**
 * @brief Obtain the node in the payload by index
 *
 * Obtain the node in the payload by zero-based index.
 * - If the payload is an array it will be the Nth element of the array.
 * - If the payload is a collection it will be the Nth element in the Members element.
 * - If the payload is an object it will be the element at the Nth key in the object.
 * - If the payload is any other type it will return NULL.
 * - If the element obtained is a navigation property to another URI then the content of that URI will be obtained synchronously.
 *
 * @param payload The payload to get the child node of
 * @param index The index of the payload to obtain
 * @return The child payload or NULL if it does not exist or could not be obtained.
 * @see getPayloadByIndexAsync
 * @see getPayloadByIndexNoNetwork
 */
REDFISH_EXPORT redfishPayload* getPayloadByIndex(redfishPayload* payload, size_t index);
/**
 * @brief Obtain the child payload according to Redpath
 *
 * Obtain a child node identified by redpath synchronously.
 *
 * @param payload The payload to get the child node of
 * @param redpath The parsed redpath to use to obtain the child
 * @return The child payload or NULL if it does not exist or could not be obtained.
 * @see getPayloadForPathAsync
 */
REDFISH_EXPORT redfishPayload* getPayloadForPath(redfishPayload* payload, redPathNode* redpath);
/**
 * @brief Obtain the child payload according to Redpath string
 *
 * Parse the redpath string and obtain a child node identified by redpath synchronously.
 *
 * @param payload The payload to get the child node of
 * @param string The string redpath to use to obtain the child
 * @return The child payload or NULL if it does not exist or could not be obtained.
 */
REDFISH_EXPORT redfishPayload* getPayloadForPathString(redfishPayload* payload, const char* string);
/**
 * @brief Obtain the number of members of a collection
 *
 * Return the number of elements in the collection.
 *
 * @param payload The payload to get number of elements in
 * @return 0 if the payload is not a collection. The total number of elements in the collection otherwise. 
 */
REDFISH_EXPORT size_t          getCollectionSize(redfishPayload* payload);
/**
 * @brief PATCH a string property of the payload (works for enums too).
 *
 * Synchronously PATCH a string or enum property of a redfish payload with a new value.
 *
 * @param payload The payload to update
 * @param propertyName The name of the property to update
 * @param value The new value for the property
 * @return NULL if failure, the resulting redfishPayload on success. 
 */
REDFISH_EXPORT redfishPayload* patchPayloadStringProperty(redfishPayload* payload, const char* propertyName, const char* value);
/**
 * @brief POST new data to the URI reresented by a payload.
 *
 * Synchronously POST a character buffer to the URI the payload was obtained from.
 *
 * @param target The payload to update
 * @param data The character buffer to write
 * @param dataSize The size of the buffer to write
 * @param contentType The value for the "content-type" header
 * @return NULL if failure, the resulting redfishPayload on success. 
 */
REDFISH_EXPORT redfishPayload* postContentToPayload(redfishPayload* target, const char* data, size_t dataSize, const char* contentType);
/**
 * @brief POST a new payload to the URI reresented by a payload.
 *
 * Synchronously POST a new payload to the URI the payload was obtained from.
 *
 * @param target The payload to update
 * @param payload The payload to write
 * @return NULL if failure, the resulting redfishPayload on success. 
 */
REDFISH_EXPORT redfishPayload* postPayload(redfishPayload* target, redfishPayload* payload);
/**
 * @brief DELETE a redfish payload
 *
 * Synchronously send a DELETE operation to the URI the payload was obtained from.
 *
 * @param payload The payload to delete
 * @return False if failure, true on success. 
 */
REDFISH_EXPORT bool            deletePayload(redfishPayload* payload);
/**
 * @brief Convert a payload to a string
 *
 * Convert a payload to a string value.
 *
 * @param payload The payload to convert
 * @param prettyPrint Set to true will indicate the output should use newlines and spaces to enhance readability
 * @return NULL on failure, a string representation of the payload otherwise
 */
REDFISH_EXPORT char*           payloadToString(redfishPayload* payload, bool prettyPrint);
/**
 * @brief Free the local copy of the payload
 *
 * Free the local copy of the payload. Does not interact with the redfish service at all. 
 *
 * @param payload The payload to free
 */
REDFISH_EXPORT void            cleanupPayload(redfishPayload* payload);

/**
 * @brief Obtain the node in the payload identified by the specified nodeName
 *
 * Obtain the node in the payload identified by the specified nodeName. If the node in question is a navigation property to another URI then the 
 * content of that URI will be obtained asynchronously.
 *
 * @param payload The payload to get the child node of
 * @param nodeName The name of the node to obtain
 * @param options The redfish options to use if needing to obtain another URI or NULL for defaults
 * @param callback The callback to use when the payload is obtained
 * @param context An opaque data pointer to send to the callback
 * @return True if the request could start. False otherwise.
 * @see getPayloadByNodeNameAsync
 * @see getPayloadByNodeName
 */
REDFISH_EXPORT bool            getPayloadByNodeNameAsync(redfishPayload* payload, const char* nodeName, redfishAsyncOptions* options, redfishAsyncCallback callback, void* context);
/**
 * @brief Obtain the node in the payload by index
 *
 * Obtain the node in the payload by zero-based index.
 * - If the payload is an array it will be the Nth element of the array.
 * - If the payload is a collection it will be the Nth element in the Members element.
 * - If the payload is an object it will be the element at the Nth key in the object.
 * - If the payload is any other type it will return NULL.
 * - If the element obtained is a navigation property to another URI then the content of that URI will be obtained asynchronously.
 *
 * @param payload The payload to get the child node of
 * @param index The index of the payload to obtain
 * @param options The redfish options to use if needing to obtain another URI or NULL for defaults
 * @param callback The callback to use when the payload is obtained
 * @param context An opaque data pointer to send to the callback
 * @return True if the request could start. False otherwise.
 * @see getPayloadByIndex
 */
REDFISH_EXPORT bool            getPayloadByIndexAsync(redfishPayload* payload, size_t index, redfishAsyncOptions* options, redfishAsyncCallback callback, void* context);
/**
 * @brief Obtain the child payload according to Redpath
 *
 * Obtain a child node identified by redpath asynchronously.
 *
 * @param payload The payload to get the child node of
 * @param redpath The parsed redpath to use to obtain the child
 * @param options The redfish options to use if needing to obtain another URI or NULL for defaults
 * @param callback The callback to use when the payload is obtained
 * @param context An opaque data pointer to send to the callback
 * @return True if the request could start. False otherwise.
 * @see getPayloadForPath
 */
REDFISH_EXPORT bool            getPayloadForPathAsync(redfishPayload* payload, redPathNode* redpath, redfishAsyncOptions* options, redfishAsyncCallback callback, void* context);
/**
 * @brief Obtain the child payload according to Redpath string
 *
 * Parse the redpath string and obtain a child node identified by redpath asynchronously.
 *
 * @param payload The payload to get the child node of
 * @param string The string redpath to use to obtain the child
 * @param options The redfish options to use if needing to obtain another URI or NULL for defaults
 * @param callback The callback to use when the payload is obtained
 * @param context An opaque data pointer to send to the callback
 * @return True if the request could start. False otherwise.
 * @see getPayloadForPathString
 */
REDFISH_EXPORT bool            getPayloadForPathStringAsync(redfishPayload* payload, const char* string, redfishAsyncOptions* options, redfishAsyncCallback callback, void* context);
/**
 * @brief PATCH a payload to the URI reresented by a target asynchronously.
 *
 * Asynchronously PATCH a payload to the URI the target was obtained from.
 *
 * @param target The payload to update
 * @param payload The payload to patch
 * @return NULL if failure, the resulting redfishPayload on success. 
 */
REDFISH_EXPORT bool            patchPayloadAsync(redfishPayload* target, redfishPayload* payload, redfishAsyncOptions* options, redfishAsyncCallback callback, void* context);
/**
 * @brief POST a new payload to the URI reresented by a target asynchronously.
 *
 * Asynchronously POST a new payload to the URI the target was obtained from.
 *
 * @param target The payload to update
 * @param payload The payload to write
 * @return NULL if failure, the resulting redfishPayload on success. 
 */
REDFISH_EXPORT bool            postPayloadAsync(redfishPayload* target, redfishPayload* payload, redfishAsyncOptions* options, redfishAsyncCallback callback, void* context);
/**
 * @brief DELETE a redfish payload asynchronously
 *
 * Asynchronously send a DELETE operation to the URI the payload was obtained from.
 *
 * @param payload The payload to delete
 * @return False if failure, true on success. 
 */
REDFISH_EXPORT bool            deletePayloadAsync(redfishPayload* payload, redfishAsyncOptions* options, redfishAsyncCallback callback, void* context);

/**
 * @brief Obtain the node in the payload identified by the specified nodeName
 *
 * Obtain the node in the payload identified by the specified nodeName. If the node in question is a navigation property to another URI then return the navigation property.
 *
 * @param payload The payload to get the child node of
 * @param nodeName The name of the node to obtain
 * @return The child node or NULL if it does not exist or could not be obtained.
 * @see getPayloadByNodeNameAsync
 * @see getPayloadByNodeName
 */
REDFISH_EXPORT redfishPayload* getPayloadByNodeNameNoNetwork(redfishPayload* payload, const char* nodeName);

/**
 * @brief Obtain the node in the payload by index
 *
 * Obtain the node in the payload by zero-based index. If the node in question is a navigation property to another URI then return the navigation property.
 * - If the payload is an array it will be the Nth element of the array.
 * - If the payload is a collection it will be the Nth element in the Members element.
 * - If the payload is an object it will be the element at the Nth key in the object.
 * - If the payload is any other type it will return NULL.
 * - If the element obtained is a navigation property to another URI then the content of that URI will be obtained synchronously.
 *
 * @param payload The payload to get the child node of
 * @param index The index of the payload to obtain
 * @return The child payload or NULL if it does not exist or could not be obtained.
 * @see getPayloadByIndexAsync
 * @see getPayloadByIndex
 */
REDFISH_EXPORT redfishPayload* getPayloadByIndexNoNetwork(redfishPayload* payload, size_t index);


#endif
