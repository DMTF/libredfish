//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017 Distributed Management Task Force, Inc. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/LICENSE.md
//----------------------------------------------------------------------------
#include <string.h>

#include "redfishPayload.h"

static redfishPayload* getOpResult(redfishPayload* payload, const char* propName, const char* op, const char* value);
static redfishPayload* collectionEvalOp(redfishPayload* payload, const char* propName, const char* op, const char* value);
static redfishPayload* arrayEvalOp(redfishPayload* payload, const char* propName, const char* op, const char* value);
static redfishPayload* createCollection(redfishService* service, size_t count, redfishPayload** payloads);
static json_t*         json_object_get_by_index(json_t* json, size_t index);

redfishPayload* createRedfishPayload(json_t* value, redfishService* service)
{
    redfishPayload* payload;
    payload = (redfishPayload*)malloc(sizeof(redfishPayload));
    if(payload != NULL)
    {
        payload->json = value;
        payload->service = service;
    }
    return payload;
}

redfishPayload* createRedfishPayloadFromString(const char* value, redfishService* service)
{
    json_t* jValue = json_loads(value, 0, NULL);
    if(jValue == NULL)
    {
        return NULL;
    }
    return createRedfishPayload(jValue, service);
}

bool isPayloadCollection(redfishPayload* payload)
{
    json_t* members;
    json_t* count;

    if(!payload || !json_is_object(payload->json))
    {
        return false;
    }
    members = json_object_get(payload->json, "Members");
    count = json_object_get(payload->json, "Members@odata.count");
    return ((members != NULL) && (count != NULL));
}

bool isPayloadArray(redfishPayload* payload)
{
    if(!payload || !json_is_array(payload->json))
    {
        return false;
    }
    return true;
}

char* getPayloadStringValue(redfishPayload* payload)
{
    json_t* tmp;
    const char* value = json_string_value(payload->json);
    if(value == NULL)
    {
        if(json_object_size(payload->json) == 1)
        {
            tmp = json_object_get_by_index(payload->json, 0);
            if(tmp != NULL)
            {
                value = json_string_value(tmp);
            }
        }
        if(value == NULL)
        {
            return NULL;
        }
    }
    return strdup(value);
}

int getPayloadIntValue(redfishPayload* payload)
{
    return (int)json_integer_value(payload->json);
}

redfishPayload* getPayloadByNodeName(redfishPayload* payload, const char* nodeName)
{
    json_t* value;
    json_t* odataId;
    const char* uri;

    if(!payload || !nodeName)
    {
        return NULL;
    }

    value = json_object_get(payload->json, nodeName);
    if(value == NULL)
    {
        return NULL;
    }
    json_incref(value);
    if(json_object_size(value) == 1)
    {
        odataId = json_object_get(value, "@odata.id");
        if(odataId != NULL)
        {
            json_incref(odataId);
            uri = json_string_value(odataId);
            json_decref(value);
            value = getUriFromService(payload->service, uri);
            json_decref(odataId);
            if(value == NULL)
            {
                return NULL;
            }
        }
    }
    if(json_is_string(value))
    {
        odataId = json_object();
        json_object_set(odataId, nodeName, value);
        json_decref(value);
        value = odataId;
    }
    return createRedfishPayload(value, payload->service);
}

redfishPayload* getPayloadByIndex(redfishPayload* payload, size_t index)
{
    json_t* value = NULL;
    json_t* odataId;
    const char* uri;

    if(!payload)
    {
        return NULL;
    }
    if(isPayloadCollection(payload))
    {
        redfishPayload* members = getPayloadByNodeName(payload, "Members");
        redfishPayload* ret = getPayloadByIndex(members, index);
        cleanupPayload(members);
        return ret;
    }
    if(json_is_array(payload->json))
    {
        value = json_array_get(payload->json, index);
    }
    else if(json_is_object(payload->json))
    {
        value = json_object_get_by_index(payload->json, index);
    }

    if(value == NULL)
    {
        return NULL;
    }

    json_incref(value);
    if(json_object_size(value) == 1)
    {
        odataId = json_object_get(value, "@odata.id");
        if(odataId != NULL)
        {
            uri = json_string_value(odataId);
            json_decref(value);
            value = getUriFromService(payload->service, uri);
            if(value == NULL)
            {
                return NULL;
            }
        }
    }
    return createRedfishPayload(value, payload->service);
}

size_t getValueCountFromPayload(redfishPayload* payload)
{
    if(!payload)
    {
        return 0;
    }
    if(json_is_array(payload->json))
    {
        return json_array_size(payload->json);
    }
    else if(json_is_object(payload->json))
    {
        return json_object_size(payload->json);
    }
    else
    {
        return 1;
    }
}

redfishPayload* getPayloadForPath(redfishPayload* payload, redPathNode* redpath)
{
    redfishPayload* ret = NULL;
    redfishPayload* tmp;

    if(!payload || !redpath)
    {
        return NULL;
    }

    if(redpath->nodeName)
    {
        ret = getPayloadByNodeName(payload, redpath->nodeName);
    }
    else if(redpath->isIndex)
    {
        ret = getPayloadByIndex(payload, redpath->index);
    }
    else if(redpath->op)
    {
        ret = getOpResult(payload, redpath->propName, redpath->op, redpath->value);
    }
    else
    {
        return NULL;
    }

    if(redpath->next == NULL || ret == NULL)
    {
        return ret;
    }
    else
    {
        tmp = getPayloadForPath(ret, redpath->next);
        cleanupPayload(ret);
        return tmp;
    }
}

redfishPayload* getPayloadForPathString(redfishPayload* payload, const char* string)
{
    redPathNode* redpath;
    redfishPayload* ret;

    if(!string)
    {
        return NULL;
    }
    redpath = parseRedPath(string);
    if(redpath == NULL)
    {
        return NULL;
    }
    ret = getPayloadForPath(payload, redpath);
    cleanupRedPath(redpath);
    return ret;
}

size_t getCollectionSize(redfishPayload* payload)
{
    json_t* members;
    json_t* count;

    if(!payload || !json_is_object(payload->json))
    {
        return 0;
    }
    members = json_object_get(payload->json, "Members");
    count = json_object_get(payload->json, "Members@odata.count");
    if(!members || !count)
    {
        return 0;
    }
    return (size_t)json_integer_value(count);
}

redfishPayload* patchPayloadStringProperty(redfishPayload* payload, const char* propertyName, const char* value)
{
    json_t* json;
    json_t* json2;
    char* content;
    char* uri;

    if(!payload || !propertyName || !value)
    {
        return NULL;
    }

    json = json_object_get(payload->json, "@odata.id");
    if(json == NULL)
    {
        return NULL;
    }
    uri = strdup(json_string_value(json));

    json = json_object();
    json2 = json_string(value);
    json_object_set(json, propertyName, json2);
    content = json_dumps(json, 0);
    json_decref(json2);
    json_decref(json);

    json = patchUriFromService(payload->service, uri, content);
    free(uri);
    free(content);
    if(json == NULL)
    {
        return NULL;
    }
    return createRedfishPayload(json, payload->service);
}

redfishPayload* postContentToPayload(redfishPayload* target, const char* data, size_t dataSize, const char* contentType)
{
    json_t* json;
    char* uri;

    if(!target || !data)
    {
        return NULL;
    }
    json = json_object_get(target->json, "@odata.id");
    if(json == NULL)
    {
        json = json_object_get(target->json, "target");
        if(json == NULL)
        {
            return NULL;
        }
    }
    uri = strdup(json_string_value(json));
    json = postUriFromService(target->service, uri, data, dataSize, contentType);
    free(uri);
    if(json == NULL)
    {
        return NULL;
    }
    return createRedfishPayload(json, target->service);
}

redfishPayload* postPayload(redfishPayload* target, redfishPayload* payload)
{
    char* content;
    redfishPayload* ret;

    if(!target || !payload)
    {
        return NULL;
    }

    if(!json_is_object(payload->json))
    {
        return NULL;
    }
    content = payloadToString(payload, false);
    ret = postContentToPayload(target, content, strlen(content), NULL);
    free(content);
    return ret;
}

bool deletePayload(redfishPayload* payload)
{
    json_t* json;
    char* uri;
    bool ret;

    if(!payload)
    {
        return false;
    }

    json = json_object_get(payload->json, "@odata.id");
    if(json == NULL)
    {
        return NULL;
    }
    uri = strdup(json_string_value(json));

    ret = deleteUriFromService(payload->service, uri);
    free(uri);
    return ret;
}

void cleanupPayload(redfishPayload* payload)
{
    if(!payload)
    {
        return;
    }
    json_decref(payload->json);
    //Don't free payload->service, let the caller handle cleaning up the service
    free(payload);
}

char* payloadToString(redfishPayload* payload, bool prettyPrint)
{
    size_t flags = 0;
	if(!payload)
    {
        return NULL;
    }
    if(prettyPrint)
    {
        flags = JSON_INDENT(2);
    }
    return json_dumps(payload->json, flags);
}

static redfishPayload* getOpResult(redfishPayload* payload, const char* propName, const char* op, const char* value)
{
    const char* propStr;
    json_t* stringProp;
    bool ret = false;
    redfishPayload* prop;
    long long intVal, intPropVal;

    if(isPayloadCollection(payload))
    {
        return collectionEvalOp(payload, propName, op, value);
    }
    if(isPayloadArray(payload))
    {
        return arrayEvalOp(payload, propName, op, value);
    }

    prop = getPayloadByNodeName(payload, propName);
    if(prop == NULL)
    {
        return NULL;
    }
    stringProp = prop->json;
    switch(json_typeof(prop->json))
    {
        case JSON_OBJECT:
            stringProp = json_object_get(prop->json, propName);
        case JSON_STRING:
            if(strcmp(op, "=") == 0)
            {
                propStr = json_string_value(stringProp);
                if(propStr == NULL)
                {
                    cleanupPayload(prop);
                    return NULL;
                }
                ret = (strcmp(propStr, value) == 0);
            }
            break;
        case JSON_TRUE:
            if(strcmp(op, "=") == 0)
            {
                ret = (strcmp(value, "true") == 0);
            }
            break;
        case JSON_FALSE:
            if(strcmp(op, "=") == 0)
            {
                ret = (strcmp(value, "false") == 0);
            }
            break;
        case JSON_INTEGER:
            intPropVal = json_integer_value(prop->json);
            intVal = strtoll(value, NULL, 0);
            if(strcmp(op, "=") == 0)
            {
                ret = (intPropVal == intVal);
            }
            else if(strcmp(op, "<") == 0)
            {
                ret = (intPropVal < intVal);
            }
            else if(strcmp(op, ">") == 0)
            {
                ret = (intPropVal > intVal);
            }
            else if(strcmp(op, "<=") == 0)
            {
                ret = (intPropVal <= intVal);
            }
            else if(strcmp(op, ">=") == 0)
            {
                ret = (intPropVal >= intVal);
            }
            break;
        default:
            break;
    }
    cleanupPayload(prop);
    if(ret)
    {
        return payload;
    }
    else
    {
        return NULL;
    }
}

static redfishPayload* collectionEvalOp(redfishPayload* payload, const char* propName, const char* op, const char* value)
{
    redfishPayload* ret;
    redfishPayload* tmp;
    redfishPayload* members;
    redfishPayload** valid;
    size_t validMax;
    size_t validCount = 0;
    size_t i;

    validMax = getCollectionSize(payload);
    if(validMax == 0)
    {
        return NULL;
    }

    valid = (redfishPayload**)calloc(validMax, sizeof(redfishPayload*));
    if(valid == NULL)
    {
        return NULL;
    }
    /*Technically getPayloadByIndex would do this, but this optimizes things*/
    members = getPayloadByNodeName(payload, "Members");
    for(i = 0; i < validMax; i++)
    {
        tmp = getPayloadByIndex(members, i);
        valid[validCount] = getOpResult(tmp, propName, op, value);
        if(valid[validCount] != NULL)
        {
            validCount++;
        }
        else
        {
            cleanupPayload(tmp);
        }
    }
    cleanupPayload(members);
    if(validCount == 0)
    {
        free(valid);
        return NULL;
    }
    if(validCount == 1)
    {
        ret = valid[0];
        free(valid);
        return ret;
    }
    else
    {
        ret = createCollection(payload->service, validCount, valid);
        free(valid);
        return ret;
    }
}

static redfishPayload* arrayEvalOp(redfishPayload* payload, const char* propName, const char* op, const char* value)
{
    redfishPayload* ret;
    redfishPayload* tmp;
    redfishPayload** valid;
    size_t validMax;
    size_t validCount = 0;
    size_t i;

    validMax = json_array_size(payload->json);
    if(validMax == 0)
    {
        return NULL;
    }

    valid = (redfishPayload**)calloc(validMax, sizeof(redfishPayload*));
    if(valid == NULL)
    {
        return NULL;
    }
    for(i = 0; i < validMax; i++)
    {
        tmp = getPayloadByIndex(payload, i);
        valid[validCount] = getOpResult(tmp, propName, op, value);
        if(valid[validCount] != NULL)
        {
            validCount++;
        }
        else
        {
            cleanupPayload(tmp);
        }
    }
    if(validCount == 0)
    {
        free(valid);
        return NULL;
    }
    if(validCount == 1)
    {
        ret = valid[0];
        free(valid);
        return ret;
    }
    else
    {
        ret = createCollection(payload->service, validCount, valid);
        free(valid);
        return ret;
    }
}

static redfishPayload* createCollection(redfishService* service, size_t count, redfishPayload** payloads)
{
    redfishPayload* ret;
    json_t* collectionJson = json_object();
    json_t* jcount = json_integer((json_int_t)count);
    json_t* members = json_array();
    size_t i;

    if(!collectionJson)
    {
        return NULL;
    }
    if(!members)
    {
        json_decref(collectionJson);
        return NULL;
    }
    json_object_set(collectionJson, "Members@odata.count", jcount);
    json_decref(jcount);
    for(i = 0; i < count; i++)
    {
        json_array_append(members, payloads[i]->json);
        cleanupPayload(payloads[i]);
    }
    json_object_set(collectionJson, "Members", members);
    json_decref(members);

    ret = createRedfishPayload(collectionJson, service);
    return ret;
}

static json_t* json_object_get_by_index(json_t* json, size_t index)
{
    void* iter;
    size_t i;

    iter = json_object_iter(json);
    for(i = 0; i < index; i++)
    {
        iter = json_object_iter_next(json, iter);
        if(iter == NULL) break;
    }
    if(iter == NULL)
    {
        return NULL;
    }
    return json_object_iter_value(iter);
}
/* vim: set tabstop=4 shiftwidth=4 expandtab: */
