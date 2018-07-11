//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017 Distributed Management Task Force, Inc. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#ifndef _REDFISH_RESOURCE_H_
#define _REDFISH_RESOURCE_H_

//redfishPayload is defined here...
#include "redfishPayload.h"

typedef enum {
    RedfishHealthError = -1,
    RedfishHealthUnknown = 0,
    RedfishHealthOK = 1,
    RedfishHealthWarning = 2,
    RedfishHealthCritical = 3
} redfishHealth;

typedef enum {
    RedfishStateError = -1,
    RedfishStateUnknown = 0,
    RedfishStateEnabled = 1,
    RedfishStateDisabled = 2,
    RedfishStateStandbyOffline = 3,
    RedfishStateStandbySpare = 4,
    RedfishStateInTest = 5,
    RedfishStateStarting = 6,
    RedfishStateAbsent = 7,
    RedfishStateUnavailableOffline = 8,
    RedfishStateDeferring = 9,
    RedfishStateQuiesced = 10,
    RedfishStateUpdating = 11
} redfishState;

REDFISH_EXPORT redfishHealth getResourceHealth(redfishPayload* payload);
REDFISH_EXPORT redfishHealth getResourceRollupHealth(redfishPayload* payload);
REDFISH_EXPORT redfishState getResourceState(redfishPayload* payload);

#endif
