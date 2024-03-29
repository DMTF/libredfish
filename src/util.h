//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2019 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/main/LICENSE.md
//----------------------------------------------------------------------------

/**
 * @file util.h
 * @author Patrick Boyd
 * @brief File containing the utility functions.
 *
 * This file explains the interface for various utility functions
 */
#ifndef _UTIL_H_
#define _UTIL_H_

#include "queue.h"
#include <jansson.h>

#ifndef _MSC_VER
typedef int SOCKET;
#endif

/**
 * @brief Duplicate a string.
 *
 * Duplicates a string or returns null if a null pointer is passed in.
 *
 * @param str The string to duplicate
 * @return a new copy of str or NULL
 */
char* safeStrdup(const char* str);

/**
 * @brief Return the contents of a string until a specified terminator.
 *
 * Return the contents of a string until a specified terminator.
 *
 * @param string The string to search
 * @param terminator The string to search until
 * @param retEnd [optional] The location of the terminator
 * @return A copy of the contents of string until terminator is hit
 */
char* getStringTill(const char* string, const char* terminator, char** retEnd);

/**
 * @brief Get the string version of the IPv4 address for the specified interface.
 *
 * Get the string version of the IPv4 address for the specified interface.
 *
 * @param networkInterface The interface to obtain the IPv4 address of
 * @return The string version of the IPv4 address or NULL on error
 */
char* getIpv4Address(const char* networkInterface);

/**
 * @brief Get the string version of the IPv6 address for the specified interface.
 *
 * Get the string version of the IPv6 address for the specified interface.
 *
 * @param networkInterface The interface to obtain the IPv6 address of
 * @return The string version of the IPv6 address or NULL on error
 */
char* getIpv6Address(const char* networkInterface);

/**
 * @brief Get a socket bound to a random port on the specified ip.
 *
 * Get a socket bound to a random port on the specified ip.
 *
 * @param ip The ip to bind to
 * @param portNum A pointer to an integer describing the port number. If set to an int other than 0 the function will use that port. 0 will cause a random port to be used.
 * @return The socket or -1 on error
 */
SOCKET getSocket(const char* ip, unsigned int* portNum);

/**
 * @brief Get a named socket.
 *
 * Get a named socket.
 *
 * @param name The name to bind to
 * @return The socket or -1 on error
 */
SOCKET getDomainSocket(const char* name);

/**
 * @brief Get the id of the currently running thread.
 *
 * Get the id of the currently running thread.
 *
 * @return The id of the currently running thread
 */
thread getThreadId();

/**
 * @brief Add a string value to a json array
 *
 * Add a string value to a json array
 *
 * @param array The array to add to
 * @param value The string value to add
 */
void addStringToJsonArray(json_t* array, const char* value);

/**
 * @brief Close a socket
 *
 * Close a socket
 *
 * @param socket The socket to close
 */
void socketClose(SOCKET socket);

#endif
/* vim: set tabstop=4 shiftwidth=4 expandtab: */
