//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017 Distributed Management Task Force, Inc. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#ifndef _REDFISH_EVENT_H_
#define _REDFISH_EVENT_H_

#ifndef NO_CZMQ
#include <czmq.h>

#define REDFISH_EVENT_0MQ_SOCKET           "ipc:///var/run/libredfish/eventPipe"

#define REDFISH_EVENT_0MQ_LIBRARY_NEW_SOCK zsock_new_pull("@" REDFISH_EVENT_0MQ_SOCKET)
#define REDFISH_EVENT_0MQ_HELPER_NEW_SOCK  zsock_new_push(">" REDFISH_EVENT_0MQ_SOCKET)
#else
#define CZMQ_VERSION_MAJOR 0
#endif

#endif
