cmake_minimum_required(VERSION 2.6)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

set(CMAKE_C_FLAGS_DEBUG "-D_DEBUG")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)

set(REDFISH_HDR_PUBLIC 
   ${CMAKE_CURRENT_SOURCE_DIR}/include/redfish.h
   ${CMAKE_CURRENT_SOURCE_DIR}/include/redfishPayload.h
   ${CMAKE_CURRENT_SOURCE_DIR}/include/redpath.h)

file(GLOB REDFISH_SRC src/*.c)

source_group("Library Sources" FILES ${REDFISH_SRC})

find_package(CURL REQUIRED)
find_package(Jansson REQUIRED)
find_package(CZMQ)

add_library(redfish SHARED ${REDFISH_SRC} ${REDFISH_HDR_PRIVATE})
if(CZMQ_FOUND)
  target_link_libraries(redfish jansson curl czmq)
else()
  target_link_libraries(redfish jansson curl)
  add_definitions( -DNO_CZMQ )
endif()

add_executable(redfishtest "${CMAKE_CURRENT_SOURCE_DIR}/examples/test.c")
target_link_libraries(redfishtest redfish)

if(CZMQ_FOUND)
  add_executable(redfishevent "${CMAKE_CURRENT_SOURCE_DIR}/httpd/cgi.c")
  target_link_libraries(redfishevent czmq)
endif()

ENABLE_TESTING()

if(CMAKE_COMPILER_IS_GNUCC)
    add_definitions(-Wall -Wextra -Wdeclaration-after-statement -ggdb3)
endif()
