# libRedfish

Copyright 2017-2019 DMTF. All rights reserved.

## About

libRedfish is a C client library that allows for Creation of Entities (POST), Read of Entities (GET), Update of Entities (PATCH), Deletion of Entities (DELETE), running Actions (POST), receiving events, and providing some basic query abilities.

# Installation

## CentOS 7/Redhat Linux 7

1. Add the EPEL repository
```# yum install https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm```
2. Install libjansson, libcurl, and libreadline
```# yum install jansson libcurl readline```
3. Download RPM (example only please download latest RPM)
```$ wget https://github.com/DMTF/libredfish/releases/download/1.2.0/libredfish-1.2.0-1.el7.x86_64.rpm```
4. Install the RPM (substititue the file name from the lastest RPM)
```# rpm -ivh libredfish-1.2.0-1.el7.x86_64.rpm```

## CentOS 6/Redhat Linux 6

1. Add the EPEL repository
```# yum install https://dl.fedoraproject.org/pub/epel/epel-release-latest-6.noarch.rpm```
2. Install libjansson, libcurl, and libreadline
```# yum install jansson libcurl readline```
3. Download RPM (example only please download latest RPM)
```$ wget https://github.com/DMTF/libredfish/releases/download/1.2.0/libredfish-1.2.0-1.el6.x86_64.rpm```
4. Install the RPM (substititue the file name from the lastest RPM)
```# rpm -ivh libredfish-1.2.0-1.el6.x86_64.rpm```

## Ubuntu

1. Install libjansson, libcurl, and libreadline
```apt-get install libjansson4 libcurl4 libreadline7```
2. Download Debian Package
3. Install Debian Package
```#dpkg -i libredfish-1.2.0-1.x86_64.deb```

## Other OS/Distro

Compile from source, see below.

# Compilation

## Pre-requisists

libRedfish is based on C and the compiling system is required to have:
* CMake
* C Compiler
* libjansson - http://www.digip.org/jansson/
* libcurl - https://curl.haxx.se/libcurl/
To receive events a user needs an existing webserver supporting FastCGI (such as Apache or nginx) and libczmq (https://github.com/zeromq/czmq).

## Build

Run cmake.

# RedPath

libRedfish uses a query language based on XPath (https://www.w3.org/TR/1999/REC-xpath-19991116/). This library and query language essentially treat the entire Redfish Service like it was a single JSON document. In other words whenever it encounters an @odata.id it will retrieve the new document (if needed).

| Expression        | Description                                                                                                    |
| ----------------- | -------------------------------------------------------------------------------------------------------------- |
| *nodename*        | Selects the JSON entity with the name "nodename"                                                               |
| /                 | Selects from the root node                                                                                     |
| [*index*]         | Selects the index number JSON entity from an array or object                                                   |
| [last()]          | Selects the last index number JSON entity from an array or object                                              |
| [*nodename*]      | Selects all the elements from an array or object that contain a property named "nodename"                      |
| [*name*=*value*]  | Selects all the elements from an array or object where the property "name" is equal to "value"                 |
| [*name*<*value*]  | Selects all the elements from an array or object where the property "name" is less than "value"                |
| [*name*<=*value*] | Selects all the elements from an array or object where the property "name" is less than or equal to "value"    |
| [*name*>*value*]  | Selects all the elements from an array or object where the property "name" is greater than "value"             |
| [*name*>=*value*] | Selects all the elements from an array or object where the property "name" is greater than or equal to "value" |
| [*name*!=*value*] | Selects all the elements from an array or object where the property "name" does not equal "value"              |
| [*]               | Selects all the elements from an array or object                                                               |
| [*node*.*child*]  | Selects all the elements from an array or object that contain a property named "node" which contains "child"   |

Some examples:

* /Chassis[1] - Will return the first Chassis instance
* /Chassis[SKU=1234] - Will return all Chassis instances with a SKU field equal to 1234
* /Systems[Storage] - Will return all the System instances that have Storage field populated
* /Systems[*] - Will return all the System instances
* /SessionService/Sessions[last()] - Will return the last Session instance
* /Chassis[Location.Info] - Will return all the Chassis instances that have a Location field and a Info subfield of Location
* /Systems[Status.Health=OK] - Will return all System instances that have a Health of OK

# C Example

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

# Release Process

Run the `release.sh` script to publish a new version.

```bash
sh release.sh <NewVersion>
```

Enter the release notes when prompted; an empty line signifies no more notes to add.
