cmake_minimum_required(VERSION 2.6)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

set(CMAKE_C_FLAGS_DEBUG "-D_DEBUG")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)

set(REDFISH_HDR_PUBLIC 
   ${CMAKE_CURRENT_SOURCE_DIR}/include/redfish.h
   ${CMAKE_CURRENT_SOURCE_DIR}/include/redfishEvent.h
   ${CMAKE_CURRENT_SOURCE_DIR}/include/redfishPayload.h
   ${CMAKE_CURRENT_SOURCE_DIR}/include/redfishRawAsync.h
   ${CMAKE_CURRENT_SOURCE_DIR}/include/redfishService.h
   ${CMAKE_CURRENT_SOURCE_DIR}/include/redpath.h)

file(GLOB REDFISH_SRC src/*.c src/entities/*.c)

source_group("Library Sources" FILES ${REDFISH_SRC})

find_package(CURL REQUIRED)
find_package(Jansson REQUIRED)
find_package(CZMQ)
find_package(OpenSSL)
find_package(Readline)

include_directories(${CURL_INCLUDE_DIR})
include_directories(${JANSSON_INCLUDE_DIRS})

add_library(redfish SHARED ${REDFISH_SRC} ${REDFISH_HDR_PRIVATE})
target_link_libraries(redfish jansson curl)
if(CZMQ_FOUND)
  target_link_libraries(redfish czmq)
else()
  add_definitions( -DNO_CZMQ )
endif()

if(OPENSSL_FOUND)
  add_definitions(-DHAVE_OPENSSL)
  target_link_libraries(redfish ${OPENSSL_SSL_LIBRARY})
endif()


add_executable(redfishtest "${CMAKE_CURRENT_SOURCE_DIR}/examples/test.c")
target_link_libraries(redfishtest redfish)

add_executable(redfishtestasync "${CMAKE_CURRENT_SOURCE_DIR}/examples/testAsync.c")
target_link_libraries(redfishtestasync redfish)

add_executable(redfishcli "${CMAKE_CURRENT_SOURCE_DIR}/examples/cli.c")
target_link_libraries(redfishcli redfish jansson)
if(READLINE_FOUND)
    add_definitions(-DHAVE_READLINE)
    target_link_libraries(redfishcli ${Readline_LIBRARY})
endif()

if(CZMQ_FOUND)
  add_executable(redfishevent "${CMAKE_CURRENT_SOURCE_DIR}/httpd/cgi.c")
  target_link_libraries(redfishevent czmq)
endif()

install(TARGETS redfishtest redfish
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib)
install(FILES ${REDFISH_HDR_PUBLIC} DESTINATION include)

ENABLE_TESTING()

if(CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wdeclaration-after-statement -ggdb3")
endif()
if(MSVC)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /LIBPATH:.. /LIBPATH:../curl/lib")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LIBPATH:.. /LIBPATH:../curl/lib")
endif()
