cmake_minimum_required(VERSION 2.8)
project(C4 C)
subdirs(src)

if(C4_BUILD_TYPE STREQUAL "Release")
  message(STATUS "*** C4 BUILD TYPE: Release ***")
  set(CMAKE_BUILD_TYPE Release)
else()
  if(DEFINED C4_BUILD_TYPE AND NOT C4_BUILD_TYPE STREQUAL "Debug")
    message(FATAL_ERROR "Unrecognized C4 Build Type: ${C4_BUILD_TYPE}")
  endif()
  message(STATUS "*** C4 BUILD TYPE: Debug ***")
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(PACKAGE_VERSION 1)
set(BUILD_SHARED_LIBS on)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(Apr REQUIRED)
find_package(Apu REQUIRED)
find_package(Bison REQUIRED)
find_package(Flex REQUIRED)
find_package(SQLite3 REQUIRED)

message (STATUS "APR version: ${APR_VERSION}")
message (STATUS "APU version: ${APU_VERSION}")

set(CMAKE_C_FLAGS "-g -Wall -Wextra -Wredundant-decls -Wstrict-prototypes -Wdeclaration-after-statement -Wmissing-declarations -Wmissing-prototypes -Winline -pedantic -std=gnu99 -fno-strict-aliasing ${APR_CFLAGS} ${CMAKE_C_FLAGS}")

if(CMAKE_BUILD_TYPE STREQUAL Debug)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -DC4_ASSERT_ENABLED")
  # $APR_CFLAGS often contains -O2, which makes debugging harder
  string(REGEX REPLACE "-O2" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
endif()

message (STATUS "CFLAGS: ${CMAKE_C_FLAGS}")
