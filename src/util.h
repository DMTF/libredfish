//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2019 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
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
 * @param interface The interface to obtain the IPv4 address of
 * @return The string version of the IPv4 address or NULL on error
 */
char* getIpv4Address(const char* interface);

/**
 * @brief Get the string version of the IPv6 address for the specified interface.
 *
 * Get the string version of the IPv6 address for the specified interface.
 *
 * @param interface The interface to obtain the IPv6 address of
 * @return The string version of the IPv6 address or NULL on error
 */
char* getIpv6Address(const char* interface);

/**
 * @brief Get a socket bound to a random port on the specified ip.
 *
 * Get a socket bound to a random port on the specified ip.
 *
 * @param ip The ip to bind to
 * @param portNum A pointer to an integer describing the port number
 * @return The socket or -1 on error
 */
int getRandomSocket(const char* ip, unsigned int* portNum);

/**
 * @brief Get the id of the currently running thread.
 *
 * Get the id of the currently running thread.
 *
 * @return The id of the currently running thread
 */
thread getThreadId();

#endif
/* vim: set tabstop=4 shiftwidth=4 expandtab: */
