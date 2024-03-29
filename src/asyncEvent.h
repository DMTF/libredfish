//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2019 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/main/LICENSE.md
//----------------------------------------------------------------------------

/**
 * @file asyncEvent.h
 * @author Patrick Boyd
 * @brief File containing the interface for the async events.
 *
 * This file explains the interface for the internal async event system
 */
#ifndef _ASYNC_EVENT_H_
#define _ASYNC_EVENT_H_

#include <redfishService.h>

#define CONNECT_TYPE_TCP 1
#define CONNECT_TYPE_SSL 2
#define CONNECT_TYPE_ANY 3

bool registerCallback(redfishService* service, redfishEventCallback callback, unsigned int eventTypes, const char* context);
bool unregisterCallback(redfishService* service, redfishEventCallback callback, unsigned int eventTypes, const char* context);
void startEventThread(redfishService* service);
void terminateAsyncEventThread(redfishService* service);

bool startSSEListener(redfishService* service, const char* sseUri);
bool startTCPListener(redfishService* service, SOCKET socket, int type);
bool startZeroMQListener(redfishService* service);

#endif
/* vim: set tabstop=4 shiftwidth=4 ff=unix expandtab: */
