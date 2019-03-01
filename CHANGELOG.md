# Change Log

## [1.1.8] - 2019-03-01
- Fixed new gcc compiler warnings
- Increased the event payload size to 12KB
- Added automatic cleanup of sessions

## [1.1.7] - 2019-02-15
- Various compiler warning fixes
- Race condition fix for cURL global access

## [1.1.6] - 2019-02-08
- Added ability to listen via TCP/OpenSSL socket
- Windows build fixes

## [1.1.5] - 2019-02-01
- Fixed various leaks
- Refactored some code to remove duplications

## [1.1.4] - 2019-01-11
- Fixed various leaks
- Added ability to specify a timeout per request
- Added asynchronous initialization
- Added operation to copy redfish payload
- Fixed RPM spec file version
- Added devel package to RPM spec file

## [1.1.3] - 2018-11-30
- Workaround for implementations that do not present the required Members property in resource collections that are empty
- Error out when sync calls are done on an async callback
- Workaround for server versions that don't correctly handle Keep-Alive with Redfish

## [1.1.2] - 2018-10-26
- Minor compiler warning fixes

## [1.1.1] - 2018-10-19
- Added support for != and null operators
- Fixed reference count with several synchronous calls

## [1.1.0] - 2018-10-12
- Fixed crash due to null passed to strdup
- Added Payload PATCH/POST/DELETE helper functions
- Added getResourceName function

## [1.0.9] - 2018-10-05
- Various fixes for memory management
- Allow paths inside of collections
- Added helper to get a payload by index when no network is present

## [1.0.8] - 2018-09-28
- Added redpath for all members of a collection
- Added helper to get the URI for a payload

## [1.0.7] - 2018-09-21
- Added helper to get the path string for asynchronous operations
- Fixed size issue with synchronous POST operations

## [1.0.6] - 2018-09-07
- Added fix for handling redirects from a Redfish service

## [1.0.5] - 2018-07-16
- Added helpers to get Health, HealthRollup, and State from resources

## [1.0.4] - 2018-06-29
- Fixed Python binding support
- Added asynchronous operations

## [1.0.3] - 2018-06-22
- Fixed bearer token support
- Added debug print capabilities

## [1.0.2] - 2018-04-25
- Various fixes to how locks are handled

## [1.0.1] - 2018-02-21
- Added support for Windows x64
- Fixed compiler warnings

## [1.0.0] - 2017-10-17
- Initial Public Release
