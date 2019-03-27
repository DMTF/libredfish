//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2018-2019 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/master/LICENSE.md
//----------------------------------------------------------------------------

/**
 * @file resource.h
 * @author Patrick Boyd
 * @brief File containing the interface for interacting with Redfish Chassis resources.
 *
 * This file explains the interface for the interacting with payloads representing a Chassis.Chassis entity type or any type decended from that type
 */
#ifndef _REDFISH_CHASSIS_H_
#define _REDFISH_CHASSIS_H_

//redfishPayload is defined here...
#include "redfishPayload.h"

/** Redfish health enum. Corresponds to Resource.Health in CSDL **/
typedef enum {
    /** There was an error obtaining ChassisType **/
    RedfishChassisTypeError = -1,
    /** The ChassisType is unknown **/
    RedfishChassisTypeUnknown = 0,
    /** Rack. An equipment rack, typically a 19-inch wide freestanding unit. **/
    RedfishChassisTypeRack = 1,
    /** Blade. An enclosed or semi-enclosed, typically vertically-oriented, system chassis which must be plugged into a multi-system chassis to function normally. **/
    RedfishChassisTypeBlade = 2,
    /** Enclosure. A generic term for a chassis that does not fit any other description. **/
    RedfishChassisTypeEnclosure = 3,
    /** StandAlone. A single, free-standing system, commonly called a tower or desktop chassis. **/
    RedfishChassisTypeStandAlone = 4,
    /** RackMount. A single system chassis designed specifically for mounting in an equipment rack. **/
    RedfishChassisTypeRackMount = 5,
    /** Card. A loose device or circuit board intended to be installed in a system or other enclosure. **/
    RedfishChassisTypeCard = 6,
    /** Cartridge. A small self-contained system intended to be plugged into a multi-system chassis. **/
    RedfishChassisTypeCartridge = 7,
    /** Row. A collection of equipment racks. **/
    RedfishChassisTypeRow = 8,
    /** Pod. A collection of equipment racks in a large, likely transportable, container. **/
    RedfishChassisTypePod = 9,
    /** Expansion. A chassis which expands the capabilities or capacity of another chassis. **/
    RedfishChassisTypeExpansion = 10,
    /** Sidecar. A chassis that mates mechanically with another chassis to expand its capabilities or capacity. **/
    RedfishChassisTypeSidecar = 11,
    /** Zone. A logical division or portion of a physical chassis that contains multiple devices or systems that cannot be physically separated. **/
    RedfishChassisTypeZone = 12,
    /** Sled. An enclosed or semi-enclosed, system chassis which must be plugged into a multi-system chassis to function normally similar to a blade type chassis. **/
    RedfishChassisTypeSled = 13,
    /** Shelf. An enclosed or semi-enclosed, typically horizontally-oriented, system chassis which must be plugged into a multi-system chassis to function normally. **/
    RedfishChassisTypeShelf = 14,
    /** Drawer. An enclosed or semi-enclosed, typically horizontally-oriented, system chassis which may be slid into a multi-system chassis. **/
    RedfishChassisTypeDrawer = 15,
    /** Module. A small, typically removable, chassis or card which contains devices for a particular subsystem or function. **/
    RedfishChassisTypeModule = 16,
    /** Component. A small chassis, card, or device which contains devices for a particular subsystem or function. **/
    RedfishChassisTypeComponent = 17,
    /** IPBasedDrive. A chassis in a drive form factor with IP-based network connections. **/
    RedfishChassisTypeIPBasedDrive = 18,
    /** RackGroup. A group of racks which form a single entity or share infrastructure. **/
    RedfishChassisTypeRackGroup = 19,
    /** StorageEnclosure. A chassis which encloses storage. **/
    RedfishChassisTypeStorageEnclosure = 20,
    /** Other. A chassis that does not fit any of these definitions. **/
    RedfishChassisTypeOther = 255
} redfishChassisType;

/** Redfish Indicator LED enum. Corresponds to various IndicatorLED properties in Redfish **/
typedef enum {
    /** There was an error obtaining IndicatorLED **/
    RedfishIndicatorLEDError = -1,
    /** The IndicatorLED is unknown **/
    RedfishIndicatorLEDUnknown = 0,
    /** The Indicator LED is lit. **/
    RedfishIndicatorLEDLit = 1,
    /** The Indicator LED is blinking. **/
    RedfishIndicatorLEDBlinking = 2,
    /** The Indicator LED is off. **/
    RedfishIndicatorLEDOff = 3
} redfishIndicatorLED;

/**
 * @brief Get the redfishChassisType of the chassis payload.
 *
 * Get the enumeration value corresponding to the reported chassis of the Redfish Chassis Payload
 *
 * @param payload The payload to get the chassis type of
 * @return value of redfishChassisType corresponding to the health or RedfishChassisTypeError on failure.
 * @see redfishChassisType
 */
REDFISH_EXPORT redfishChassisType getChassisType(redfishPayload* payload);

/**
 * @brief Get the redfishIndicatorLED of the chassis payload.
 *
 * Get the enumeration value corresponding to the reported IndicatorLED of the Redfish Payload
 *
 * @param payload The payload to get the IndicatorLED of
 * @return value of redfishIndicatorLED corresponding to the state or RedfishIndicatorLEDError on failure.
 * @see redfishIndicatorLED
 */
REDFISH_EXPORT redfishIndicatorLED getIndicatorLED(redfishPayload* payload);

/**
 * @brief Set the redfishIndicatorLED of the chassis payload synchronously.
 *
 * Set the enumeration value corresponding to the reported IndicatorLED of the Redfish Payload synchronously.
 *
 * @param payload The payload to set the IndicatorLED on
 * @param newLEDState The new state to set the IndicatorLED to
 * @return 0 on success. Any other value on error
 * @see getIndicatorLED
 * @see setIndicatorLEDAsync
 */
REDFISH_EXPORT int setIndicatorLED(redfishPayload* payload, redfishIndicatorLED newLEDState);

/**
 * @brief Set the redfishIndicatorLED of the chassis payload asynchronously.
 *
 * Set the enumeration value corresponding to the reported IndicatorLED of the Redfish Payload asynchronously.
 *
 * @param payload The payload to set the IndicatorLED on
 * @param newLEDState The new state to set the IndicatorLED to
 * @param callback The function to call when the operation is complete
 * @param context A context value to pass to the callback
 * @return true if the operation was started, false otherwise
 * @see getIndicatorLED
 * @see setIndicatorLED
 */
REDFISH_EXPORT bool setIndicatorLEDAsync(redfishPayload* payload, redfishIndicatorLED newLEDState, redfishAsyncCallback callback, void* context);

#endif
