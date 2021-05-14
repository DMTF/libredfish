# Change Log

## [1.3.1] - 2021-05-14
- Added support for SSE streams to call their callback routine

## [1.3.0] - 2020-06-26
- Various fixes to handling cleanup of asyncToSync

## [1.2.9] - 2020-06-12
- Corrected extraneous cleanup being performed when connections drop that will prevent the version resource from being read again

## [1.2.8] - 2020-03-26
- Added timeout to conditional wait to prevent queue threads from hanging

## [1.2.7] - 2019-11-21
- Added getPayloadLongLongValue helper

## [1.2.6] - 2019-11-15
- Added getPayloadDoubleValue helper

## [1.2.5] - 2019-11-08
- Added getPayloadBoolValue helper

## [1.2.4] - 2019-10-11
- Changed event received log message severity from error to warning
- Added getArraySize helper

## [1.2.3] - 2019-08-09
- Added support for asynchronous event notification
- Made fix to path searching

## [1.2.2] - 2019-06-07
- Added error checking for when 200 OK is returned, but the payload could not be parsed

## [1.2.1] - 2019-05-10
- Fixed various leaks
- Added support for OpenSSL 1.1.0

## [1.2.0] - 2019-04-12
- Added Chassis entity support
- Various compiler, warning, and code checker improvements
- Various NULL checks added
- Updated Fedora build to use newer versions
- Added Debian package support
- Added sample CLI code

## [1.1.9] - 2019-03-26
- Made change to ensure session based auth calls back in separate thread

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
