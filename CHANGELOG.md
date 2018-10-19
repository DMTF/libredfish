# Change Log

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
