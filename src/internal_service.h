//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2018 Distributed Management Task Force, Inc. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#ifndef _INT_SERVICE_H_
#define _INT_SERVICE_H_

#include <jansson.h>
#include <curl/curl.h>
#include "queue.h"

typedef struct _redfishService {
    char* host;
    queue* queue;
    thread asyncThread;
    CURL* curl;
    json_t* versions;
    unsigned int flags;
    char* sessionToken;
    char* bearerToken;
    char* otherAuth;
#ifdef _MSC_VER
    HANDLE mutex;
#else
    pthread_mutex_t mutex;
#endif
    size_t refCount;
} redfishService;


#endif
