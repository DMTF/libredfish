//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2018-2019 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/main/LICENSE.md
//----------------------------------------------------------------------------
#include <string.h>

#include "entities/chassis.h"
#include "redfishPayload.h"
#include "../debug.h"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

/** A struct to help map the string enum values produced by a redfish service to more C friendly enum values **/
typedef struct {
    /** The redfish string enum value **/
    const char* string;
    /** The C enum value **/
    redfishChassisType type;
} stringToChassisTypeMap;

static stringToChassisTypeMap typeMapping[] = {
    {"Rack", RedfishChassisTypeRack},
    {"Blade", RedfishChassisTypeBlade},
    {"Enclosure", RedfishChassisTypeEnclosure},
    {"StandAlone", RedfishChassisTypeStandAlone},
    {"RackMount", RedfishChassisTypeRackMount},
    {"Card", RedfishChassisTypeCard},
    {"Cartridge", RedfishChassisTypeCartridge},
    {"Row", RedfishChassisTypeRow},
    {"Pod", RedfishChassisTypePod},
    {"Expansion", RedfishChassisTypeExpansion},
    {"Sidecar", RedfishChassisTypeSidecar},
    {"Zone", RedfishChassisTypeZone},
    {"Sled", RedfishChassisTypeSled},
    {"Shelf", RedfishChassisTypeShelf},
    {"Drawer", RedfishChassisTypeDrawer},
    {"Module", RedfishChassisTypeModule},
    {"Component", RedfishChassisTypeComponent},
    {"IPBasedDrive", RedfishChassisTypeIPBasedDrive},
    {"RackGroup", RedfishChassisTypeRackGroup},
    {"StorageEnclosure", RedfishChassisTypeStorageEnclosure},
    {"Other", RedfishChassisTypeOther},
    {NULL, RedfishChassisTypeUnknown}
};

/** A struct to help map the string enum values produced by a redfish service to more C friendly enum values **/
typedef struct {
    /** The redfish string enum value **/
    const char* string;
    /** The C enum value **/
    redfishIndicatorLED led;
} stringToIndicatorLEDMap;

static stringToIndicatorLEDMap ledMapping[] = {
    {"Lit", RedfishIndicatorLEDLit},
    {"Blinking", RedfishIndicatorLEDBlinking},
    {"Off", RedfishIndicatorLEDOff},
    {NULL, RedfishIndicatorLEDUnknown}
};

redfishChassisType getChassisType(redfishPayload* payload)
{
    redfishPayload* chassistype;
    char* chassistypeStr;
    size_t i;

    if(payload == NULL)
    {
        REDFISH_DEBUG_WARNING_PRINT("%s: Payload is NULL\n", __func__);
        return RedfishChassisTypeError;
    }
    chassistype = getPayloadByNodeNameNoNetwork(payload, "ChassisType");
    if(chassistype == NULL)
    {
        REDFISH_DEBUG_WARNING_PRINT("%s: Unable to obtain ChassisType resource from payload...\n", __func__);
        return RedfishChassisTypeError;
    }
    chassistypeStr = getPayloadStringValue(chassistype);
    cleanupPayload(chassistype);
    if(chassistypeStr == NULL)
    {
        return RedfishChassisTypeUnknown;
    }
    for(i = 0; typeMapping[i].string != NULL; i++)
    {
        if(strcasecmp(chassistypeStr, typeMapping[i].string) == 0)
        {
            free(chassistypeStr);
            return typeMapping[i].type;
        }
    }
    REDFISH_DEBUG_WARNING_PRINT("%s: Got unknown chassis type string %s...\n", __func__, chassistypeStr);
    free(chassistypeStr);
    return RedfishChassisTypeUnknown;
}

redfishIndicatorLED getIndicatorLED(redfishPayload* payload)
{
    redfishPayload* indicatorled;
    char* ledStr;
    size_t i;

    if(payload == NULL)
    {
        REDFISH_DEBUG_WARNING_PRINT("%s: Payload is NULL\n", __func__);
        return RedfishIndicatorLEDError;
    }
    indicatorled = getPayloadByNodeNameNoNetwork(payload, "IndicatorLED");
    if(indicatorled == NULL)
    {
        REDFISH_DEBUG_WARNING_PRINT("%s: Unable to obtain IndicatorLED resource from payload...\n", __func__);
        return RedfishIndicatorLEDError;
    }
    ledStr = getPayloadStringValue(indicatorled);
    cleanupPayload(indicatorled);
    if(ledStr == NULL)
    {
        return RedfishIndicatorLEDUnknown;
    }
    for(i = 0; ledMapping[i].string != NULL; i++)
    {
        if(strcasecmp(ledStr, ledMapping[i].string) == 0)
        {
            free(ledStr);
            return ledMapping[i].led;
        }
    }
    REDFISH_DEBUG_WARNING_PRINT("%s: Got unknown LED string %s...\n", __func__, ledStr);
    free(ledStr);
    return RedfishIndicatorLEDUnknown;
}

int setIndicatorLED(redfishPayload* payload, redfishIndicatorLED newLEDState)
{
    const char* ledStr = NULL;
    size_t i;
    char patchPayloadStr[256];
    char* uri;
    json_t* ret;

    if(payload == NULL)
    {
        REDFISH_DEBUG_WARNING_PRINT("%s: Payload is NULL\n", __func__);
        return -1;
    }
    for(i = 0; ledMapping[i].string != NULL; i++)
    {
        if(newLEDState == ledMapping[i].led)
        {
            ledStr = ledMapping[i].string;
            break;
        }
    }
    if(ledStr == NULL)
    {
        REDFISH_DEBUG_WARNING_PRINT("%s: Unknown LED State input value %d\n", __func__, newLEDState);
        return -1;
    }
    snprintf(patchPayloadStr, sizeof(patchPayloadStr)-1, "{\"IndicatorLED\": \"%s\"}", ledStr);
    patchPayloadStr[255] = 0;
    uri = getPayloadUri(payload);
    if(uri == NULL)
    {
        REDFISH_DEBUG_WARNING_PRINT("%s: Cannot find URI in provided payload!\n", __func__);
        return -1;
    }
    ret = patchUriFromService(payload->service, uri, patchPayloadStr);
    free(uri);
    if(ret)
    {
        printf("%s\n", json_dumps(ret, JSON_INDENT(2)));
    }
    return 0;
}

bool setIndicatorLEDAsync(redfishPayload* payload, redfishIndicatorLED newLEDState, redfishAsyncCallback callback, void* context)
{
    const char* ledStr = NULL;
    size_t i;
    char patchPayloadStr[256];
    redfishPayload* patchPayload;
    bool ret;

    if(payload == NULL)
    {
        REDFISH_DEBUG_WARNING_PRINT("%s: Payload is NULL\n", __func__);
        return false;
    }
    for(i = 0; ledMapping[i].string != NULL; i++)
    {
        if(newLEDState == ledMapping[i].led)
        {
            ledStr = ledMapping[i].string;
            break;
        }
    }
    if(ledStr == NULL)
    {
        REDFISH_DEBUG_WARNING_PRINT("%s: Unknown LED State input value %d\n", __func__, newLEDState);
        return false;
    }
    snprintf(patchPayloadStr, sizeof(patchPayloadStr)-1, "{\"IndicatorLED\": \"%s\"}", ledStr);
    patchPayloadStr[255] = 0;
    patchPayload = createRedfishPayloadFromString(patchPayloadStr, payload->service);
    if(patchPayload == NULL)
    {
        REDFISH_DEBUG_WARNING_PRINT("%s: Unable to allocate payload!\n", __func__);
        return false;
    }
    ret = patchPayloadAsync(payload, patchPayload, NULL, callback, context);
    cleanupPayload(patchPayload);
    return ret;
}
/* vim: set tabstop=4 shiftwidth=4 ff=unix expandtab: */
