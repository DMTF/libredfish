//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2018 Distributed Management Task Force, Inc. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
/**
 * @file debug.h
 * @author Patrick Boyd
 * @brief File containing the interface for the internal debug.
 *
 * This file explains the interface for the internal debug functions
 */
#include <redfish.h>

#ifdef _MSC_VER
//The defines are the same as linux's syslog.h
#define	LOG_EMERG	0
#define	LOG_ALERT	1
#define	LOG_CRIT	2
#define	LOG_ERR		3
#define	LOG_WARNING	4
#define	LOG_NOTICE	5
#define	LOG_INFO	6
#define	LOG_DEBUG	7
#else
#include <syslog.h>
#endif

/** The global library debug function **/
extern libRedfishDebugFunc gDebugFunc;

#ifdef _DEBUG
/** 
 * The syslog style debug print function. Takes a priority, message and a variable number of args
 *
 * @param pri The priority of the message
 * @param message The message format to log
 */
#define REDFISH_DEBUG_PRINT(pri, message, ...) { \
    if(gDebugFunc != NULL) gDebugFunc(pri, message, __VA_ARGS__); \
}

/** 
 * Log debug level messages
 *
 * @param message The message format to log
 */
#define REDFISH_DEBUG_DEBUG_PRINT(message, ...) REDFISH_DEBUG_PRINT(LOG_DEBUG, message, __VA_ARGS__)
/** 
 * Log info level messages
 *
 * @param message The message format to log
 */
#define REDFISH_DEBUG_INFO_PRINT(message, ...) REDFISH_DEBUG_PRINT(LOG_INFO, message, __VA_ARGS__)
/** 
 * Log notice level messages
 *
 * @param message The message format to log
 */
#define REDFISH_DEBUG_NOTICE_PRINT(message, ...) REDFISH_DEBUG_PRINT(LOG_NOTICE, message, __VA_ARGS__)
/** 
 * Log warning level messages
 *
 * @param message The message format to log
 */
#define REDFISH_DEBUG_WARNING_PRINT(message, ...) REDFISH_DEBUG_PRINT(LOG_WARNING, message, __VA_ARGS__)
/** 
 * Log error level messages
 *
 * @param message The message format to log
 */
#define REDFISH_DEBUG_ERR_PRINT(message, ...) REDFISH_DEBUG_PRINT(LOG_ERR, message, __VA_ARGS__)
/** 
 * Log critical level messages
 *
 * @param message The message format to log
 */
#define REDFISH_DEBUG_CRIT_PRINT(message, ...) REDFISH_DEBUG_PRINT(LOG_CRIT, message, __VA_ARGS__)

#else
/** 
 * The syslog style debug print function. Takes a priority, message and a variable number of args
 */
#define REDFISH_DEBUG_PRINT(...) 
/** 
 * Log debug level messages
 */
#define REDFISH_DEBUG_DEBUG_PRINT(...) 
/** 
 * Log info level messages
 */
#define REDFISH_DEBUG_INFO_PRINT(...)
/** 
 * Log notice level messages
 */
#define REDFISH_DEBUG_NOTICE_PRINT(...) 
/** 
 * Log warning level messages
 */
#define REDFISH_DEBUG_WARNING_PRINT(...) 
/** 
 * Log error level messages
 */
#define REDFISH_DEBUG_ERR_PRINT(...) 
/** 
 * Log critical level messages
 */
#define REDFISH_DEBUG_CRIT_PRINT(...) 
#endif
