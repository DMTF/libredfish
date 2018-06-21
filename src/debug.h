//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2018 Distributed Management Task Force, Inc. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
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

extern libRedfishDebugFunc gDebugFunc;

#ifdef _DEBUG
#define REDFISH_DEBUG_PRINT(pri, message, ...) { \
    if(gDebugFunc != NULL) gDebugFunc(pri, message, __VA_ARGS__); \
}

#define REDFISH_DEBUG_DEBUG_PRINT(message, ...) REDFISH_DEBUG_PRINT(LOG_DEBUG, message, __VA_ARGS__)
#define REDFISH_DEBUG_INFO_PRINT(message, ...) REDFISH_DEBUG_PRINT(LOG_INFO, message, __VA_ARGS__)
#define REDFISH_DEBUG_NOTICE_PRINT(message, ...) REDFISH_DEBUG_PRINT(LOG_NOTICE, message, __VA_ARGS__)
#define REDFISH_DEBUG_WARNING_PRINT(message, ...) REDFISH_DEBUG_PRINT(LOG_WARNING, message, __VA_ARGS__)
#define REDFISH_DEBUG_ERR_PRINT(message, ...) REDFISH_DEBUG_PRINT(LOG_ERR, message, __VA_ARGS__)
#define REDFISH_DEBUG_CRIT_PRINT(message, ...) REDFISH_DEBUG_PRINT(LOG_CRIT, message, __VA_ARGS__)

#else
#define REDFISH_DEBUG_PRINT(...) 
#define REDFISH_DEBUG_DEBUG_PRINT(...) 
#define REDFISH_DEBUG_INFO_PRINT(...) 
#define REDFISH_DEBUG_NOTICE_PRINT(...) 
#define REDFISH_DEBUG_WARNING_PRINT(...) 
#define REDFISH_DEBUG_ERR_PRINT(...) 
#define REDFISH_DEBUG_CRIT_PRINT(...) 
#endif
