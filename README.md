# libRedfish

Copyright 2017 DMTF. All rights reserved.

## About

libRedfish is a C client library that allows for Creation of Entities (POST), Read of Entities (GET), Update of Entities (PATCH), Deletion of Entities (DELETE), running Actions (POST), receiving events, and providing some basic query abilities.

## Pre-requisists

libRedfish is based on C and the compiling system is required to have:
* CMake
* C Compiler
* libjanson - http://www.digip.org/jansson/
* libcurl - https://curl.haxx.se/libcurl/
To receive events a user needs an existing webserver supporting FastCGI (such as Apache or nginx) and libczmq (https://github.com/zeromq/czmq).

## Compilation

Run cmake.

## RedPath

libRedfish uses a query language based on XPath (https://www.w3.org/TR/1999/REC-xpath-19991116/). This library and query language essentially treat the entire Redfish Service like it was a single JSON document. In other words whenever it encounters an @odata.id it will retrieve the new document (if needed).

| Expression        | Description                                                                                                    |
| ----------------- | -------------------------------------------------------------------------------------------------------------- |
| *nodename*        | Selects the JSON entity with the name "nodename"                                                               |
| /                 | Selects from the root node                                                                                     |
| [*index*]         | Selects the index number JSON entity from an array or object                                                   |
| [*nodename*]      | Selects all the elements from an array or object that contain a property named "nodename"                      |
| [*name*=*value*]  | Selects all the elements from an array or object where the property "name" is equal to "value"                 |
| [*name*<*value*]  | Selects all the elements from an array or object where the property "name" is less than "value"                |
| [*name*<=*value*] | Selects all the elements from an array or object where the property "name" is less than or equal to "value"    |
| [*name*>*value*]  | Selects all the elements from an array or object where the property "name" is greater than "value"             |
| [*name*>=*value*] | Selects all the elements from an array or object where the property "name" is greater than or equal to "value" |

Some examples:

* /Chassis[1] - Will return the first Chassis instance
* /Chassis[SKU=1234] - Will return all Chassis instances with a SKU field equal to 1234
* /Systems[Storage] - Will return all the System instances that have Storage field populated

## C Example

```C
#include <redfish.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    redfishService* service = createRedfishServiceEnumerator(argv[1], NULL, NULL, 0);
    redfishPayload* payload = getPayloadByPath(service, argv[2]);
    char* payloadStr = payloadToString(payload, true);
    printf("Payload Value = %s\n", payloadStr);
    free(payloadStr);
    cleanupPayload(payload);
    cleanupEnumerator(service);
}
```
