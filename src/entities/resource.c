//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2018 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------
#include <string.h>

#include "entities/resource.h"
#include "redfishPayload.h"
#include "../debug.h"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

static redfishHealth _getResourceHealth(redfishPayload* payload, const char* subElement, const char* function);

redfishHealth getResourceHealth(redfishPayload* payload)
{
    return _getResourceHealth(payload, "Health", __FUNCTION__);
}

redfishHealth getResourceRollupHealth(redfishPayload* payload)
{
    return _getResourceHealth(payload, "HealthRollup", __FUNCTION__);
}

/** A struct to help map the string enum values produced by a redfish service to more C friendly enum values **/
typedef struct {
    /** The redfish string enum value **/
    const char* string;
    /** The C enum value **/
    redfishState state;
} stringToStateMap;

static stringToStateMap stateMapping[] = {
    {"Enabled", RedfishStateEnabled},
    {"Disabled", RedfishStateDisabled},
    {"StandbyOffline", RedfishStateStandbyOffline},
    {"StandbySpare", RedfishStateStandbySpare},
    {"InTest", RedfishStateInTest},
    {"Starting", RedfishStateStarting},
    {"Absent", RedfishStateAbsent},
    {"UnavailableOffline", RedfishStateUnavailableOffline},
    {"Deferring", RedfishStateDeferring},
    {"Quiesced", RedfishStateQuiesced},
    {"Updating", RedfishStateUpdating},
    {NULL, RedfishStateUnknown}
};

redfishState getResourceState(redfishPayload* payload)
{
    redfishPayload* status;
    redfishPayload* state;
    char* stateStr;
    size_t i;

    if(payload == NULL)
    {
        REDFISH_DEBUG_WARNING_PRINT("%s: Payload is NULL\n", __FUNCTION__);
        return RedfishStateError;
    }
    status = getPayloadByNodeNameNoNetwork(payload, "Status");
    if(status == NULL)
    {
        REDFISH_DEBUG_WARNING_PRINT("%s: Unable to obtain Status resource from payload...\n", __FUNCTION__);
        return RedfishStateError;
    }
    state = getPayloadByNodeNameNoNetwork(status, "State");
    cleanupPayload(status);
    if(state == NULL)
    {
        REDFISH_DEBUG_WARNING_PRINT("%s: Unable to obtain State resource from payload...\n", __FUNCTION__);
        return RedfishStateError;
    }
    stateStr = getPayloadStringValue(state);
    cleanupPayload(state);
    if(stateStr == NULL)
    {
        return RedfishStateUnknown;
    }
    for(i = 0; stateMapping[i].string != NULL; i++)
    {
        if(strcasecmp(stateStr, stateMapping[i].string) == 0)
        {
            free(stateStr);
            return stateMapping[i].state;
        }
    }
    REDFISH_DEBUG_WARNING_PRINT("%s: Got unknown state string %s...\n", __FUNCTION__, stateStr);
    free(stateStr);
    return RedfishStateUnknown;
}

static redfishHealth _getResourceHealth(redfishPayload* payload, const char* subElement, const char* function)
{
    redfishPayload* status;
    redfishPayload* health;
    char* healthStr;

    if(payload == NULL)
    {
        REDFISH_DEBUG_WARNING_PRINT("%s: Payload is NULL\n", function);
        return RedfishHealthError;
    }
    status = getPayloadByNodeNameNoNetwork(payload, "Status");
    if(status == NULL)
    {
        REDFISH_DEBUG_WARNING_PRINT("%s: Unable to obtain Status resource from payload...\n", function);
        return RedfishHealthError;
    }
    health = getPayloadByNodeNameNoNetwork(status, subElement);
    cleanupPayload(status);
    if(health == NULL)
    {
        REDFISH_DEBUG_WARNING_PRINT("%s: Unable to obtain %s resource from payload...\n", function, subElement);
        return RedfishHealthError;
    }
    healthStr = getPayloadStringValue(health);
    cleanupPayload(health);
    if(healthStr == NULL)
    {
        return RedfishHealthUnknown;
    }
    if(strcasecmp(healthStr, "OK") == 0)
    {
        free(healthStr);
        return RedfishHealthOK;
    }
    if(strcasecmp(healthStr, "Warning") == 0)
    {
        free(healthStr);
        return RedfishHealthWarning;
    }
    if(strcasecmp(healthStr, "Critical") == 0)
    {
        free(healthStr);
        return RedfishHealthCritical;
    }
    REDFISH_DEBUG_WARNING_PRINT("%s: Got unknown health string %s...\n", function, healthStr);
    free(healthStr);
    return RedfishHealthUnknown;
}
