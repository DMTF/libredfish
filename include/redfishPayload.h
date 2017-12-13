//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017 Distributed Management Task Force, Inc. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/LICENSE.md
//----------------------------------------------------------------------------
#ifndef _REDFISH_PAYLOAD_H_
#define _REDFISH_PAYLOAD_H_

//redfishPayload is defined here...
#include "redfishService.h"

#include "redpath.h"

redfishPayload* createRedfishPayload(json_t* value, redfishService* service);
redfishPayload* createRedfishPayloadFromString(const char* value, redfishService* service);
bool            isPayloadCollection(redfishPayload* payload);

char*           getPayloadStringValue(redfishPayload* payload);
int             getPayloadIntValue(redfishPayload* payload);

redfishPayload* getPayloadByNodeName(redfishPayload* payload, const char* nodeName);
redfishPayload* getPayloadByIndex(redfishPayload* payload, size_t index);
redfishPayload* getPayloadForPath(redfishPayload* payload, redPathNode* redpath);
redfishPayload* getPayloadForPathString(redfishPayload* payload, const char* string);
size_t          getCollectionSize(redfishPayload* payload);
redfishPayload* patchPayloadStringProperty(redfishPayload* payload, const char* propertyName, const char* value);
redfishPayload* postContentToPayload(redfishPayload* target, const char* data, size_t dataSize, const char* contentType);
redfishPayload* postPayload(redfishPayload* target, redfishPayload* payload);
bool            deletePayload(redfishPayload* payload);
char*           payloadToString(redfishPayload* payload, bool prettyPrint);
void            cleanupPayload(redfishPayload* payload);

#endif
