//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2018-2019 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------

/**
 * @file redfish.h
 * @author Patrick Boyd
 * @brief File containing the interface for the library level interactions.
 *
 * This file explains the interface for interacting with the libredfish library and includes all external interfaces.
 */
#ifndef _REDFISH_H_
#define _REDFISH_H_

#include <redfishService.h>
#include <redfishPayload.h>
#include <redpath.h>
#include <entities/resource.h>
#include <entities/chassis.h>

/**
 * syslog style function used to debug libredfish.
 */
typedef void (*libRedfishDebugFunc)(int priority, const char* message, ...);

/**
 * Set the debug function to be used for libredfish. NOTE: This will do nothing unless
 * the _DEBUG macro is set during compilation time of the library.
 *
 * @param debugFunc The debug function to use, NULL disables debug.
 */
void REDFISH_EXPORT libredfishSetDebugFunction(libRedfishDebugFunc debugFunc);

/**
 * malloc style function to be used by libredfish
 */
typedef void* (*libRedfishMallocFunc)(size_t size);

/**
 * free style function to be used by libredfish
 */
typedef void (*libRedfishFreeFunc)(void* ptr);

/**
 * realloc style function to be used by libredfish
 */
typedef void* (*libRedfishReallocFunc)(void* ptr, size_t size);

/**
 * strdup style function to be used by libredfish
 */
typedef char* (*libRedfishStrdupFunc)(const char* str);

/**
 * calloc style function to be used by libredfish
 */
typedef void* (*libRedfishCallocFunc)(size_t nmemb, size_t size);

/**
 * Set the memory functions to be used by libredfish
 *
 * @param malloc_func the function to replace malloc with, NULL to use malloc()
 * @param free_func the function to replace free with, NULL to use free()
 * @param realloc_func the function to replace realloc with, NULL to use realloc()
 * @param strdup_func the function to replace strdup with, NULL to use strdup()
 * @param calloc_func the function to replace strdup with, NULL to use calloc()
 */
void REDFISH_EXPORT libredfishSetMemoryFunctions(libRedfishMallocFunc malloc_func, 
                                                 libRedfishFreeFunc free_func,
                                                 libRedfishReallocFunc realloc_func,
                                                 libRedfishStrdupFunc strdup_func,
                                                 libRedfishCallocFunc calloc_func);





#endif
