//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2018-2019 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/main/LICENSE.md
//----------------------------------------------------------------------------

/**
 * @file resource.h
 * @author Patrick Boyd
 * @brief File containing the interface for interacting with generic Redfish resources.
 *
 * This file explains the interface for the interacting with payloads representing a Resource.Resource entity type or any type decended from that type
 */
#ifndef _REDFISH_RESOURCE_H_
#define _REDFISH_RESOURCE_H_

//redfishPayload is defined here...
#include "redfishPayload.h"

/** Redfish health enum. Corresponds to Resource.Health in CSDL **/
typedef enum {
    /** There was an error obtaining health **/
    RedfishHealthError = -1,
    /** The health is unknown **/
    RedfishHealthUnknown = 0,
    /** Normal. **/
    RedfishHealthOK = 1,
    /** A condition exists that requires attention. **/
    RedfishHealthWarning = 2,
    /** A critical condition exists that requires immediate attention. **/
    RedfishHealthCritical = 3
} redfishHealth;

/** Redfish health enum. Corresponds to Resource.State in CSDL **/
typedef enum {
    /** There was an error obtaining state **/
    RedfishStateError = -1,
    /** The state is unknown **/
    RedfishStateUnknown = 0,
    /** This function or resource has been enabled. **/
    RedfishStateEnabled = 1,
    /** This function or resource has been disabled. **/
    RedfishStateDisabled = 2,
    /** This function or resource is enabled, but awaiting an external action to activate it. **/
    RedfishStateStandbyOffline = 3,
    /** This function or resource is part of a redundancy set and is awaiting a failover or other external action to activate it. **/
    RedfishStateStandbySpare = 4,
    /** This function or resource is undergoing testing. **/
    RedfishStateInTest = 5,
    /** This function or resource is starting. **/
    RedfishStateStarting = 6,
    /** This function or resource is not present or not detected. **/
    RedfishStateAbsent = 7,
    /** This function or resource is present but cannot be used. **/
    RedfishStateUnavailableOffline = 8,
    /** The element will not process any commands but will queue new requests. **/
    RedfishStateDeferring = 9,
    /** The element is enabled but only processes a restricted set of commands. **/
    RedfishStateQuiesced = 10,
    /** The element is updating and may be unavailable or degraded. **/
    RedfishStateUpdating = 11
} redfishState;

/**
 * @brief Get the redfishHealth of the payload.
 *
 * Get the enumeration value corresponding to the reported health of the Redfish Payload
 *
 * @param payload The payload to get the health of
 * @return value of redfishHealth corresponding to the health or RedfishHealthError on failure.
 * @see redfishHealth
 */
REDFISH_EXPORT redfishHealth getResourceHealth(redfishPayload* payload);
/**
 * @brief Get the rollup redfishHealth of the payload.
 *
 * Get the enumeration value corresponding to the reported rollup health of the Redfish Payload
 *
 * @param payload The payload to get the rollup health of
 * @return value of redfishHealth corresponding to the rollup health or RedfishHealthError on failure.
 * @see redfishHealth
 */
REDFISH_EXPORT redfishHealth getResourceRollupHealth(redfishPayload* payload);
/**
 * @brief Get the redfishState of the payload.
 *
 * Get the enumeration value corresponding to the reported state of the Redfish Payload
 *
 * @param payload The payload to get the state of
 * @return value of redfishState corresponding to the state or RedfishStateError on failure.
 * @see redfishState
 */
REDFISH_EXPORT redfishState getResourceState(redfishPayload* payload);
/**
 * @brief Get the Name of the payload.
 *
 * Get the Name string of the Redfish Payload
 *
 * @param payload The payload to get the state of
 * @return The Name string or NULL on failure.
 */
REDFISH_EXPORT char* getResourceName(redfishPayload* payload);

#endif
