//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2018-2019 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/main/LICENSE.md
//----------------------------------------------------------------------------
/**
 * @file redfishEvent.h
 * @author Patrick Boyd
 * @brief File containing the shared data for event producers and consumers.
 *
 * This file explains the interface for the event producer and consumer interface
 */
#ifndef _REDFISH_EVENT_H_
#define _REDFISH_EVENT_H_

#ifndef NO_CZMQ
#include <czmq.h>

/** The ZeroMQ socket to use between the producer and consumer **/
#define REDFISH_EVENT_0MQ_SOCKET           "ipc:///var/run/libredfish/eventPipe"

/** The Consumer side of the socket **/
#define REDFISH_EVENT_0MQ_LIBRARY_NEW_SOCK zsock_new_pull("@" REDFISH_EVENT_0MQ_SOCKET)
/** The Producer side of the socket **/
#define REDFISH_EVENT_0MQ_HELPER_NEW_SOCK  zsock_new_push(">" REDFISH_EVENT_0MQ_SOCKET)
#else
/** This indicates that we do not have the C zeromq library **/
#define CZMQ_VERSION_MAJOR 0
#endif

#endif
