cmake_minimum_required (VERSION 2.8)
project (ccommon)

# detect platform
set (OperatingSystem "OS_UNKNOWN")

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set (OperatingSystem "OS_DARWIN")
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set (OperatingSystem "OS_LINUX")
endif ()

if (OperatingSystem STREQUAL "OS_UNKNOWN")
    message(FATAL_ERROR "unsupported operating system")
endif()

# the following sections work with config.h(.in): version, compile variables
# config.h.in has to include entries set/tested here for them to have effect
# how to use config.h.in to generate config.h
configure_file (
  "${PROJECT_SOURCE_DIR}/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
  )
# version info
set (CCOMMON_VERSION_MAJOR 1)
set (CCOMMON_VERSION_MINOR 0)
set (CCOMMON_VERSION_PATCH 0)
set (CCOMMON_VERSION
    ${CCOMMON_VERSION_MAJOR}.${CCOMMON_VERSION_MINOR}.${CCOMMON_VERSION_PATCH}
    )
set (CCOMMON_RELEASE_VERSION
    ${CCOMMON_VERSION_MAJOR}.${CCOMMON_VERSION_MINOR}
    )
# flags => compile-time variables: use modules/macros
include (CheckFunctionExists)
check_function_exists (epoll HAVE_EPOLL)
check_function_exists (kevent HAVE_KQUEUE)
include (TestBigEndian)
test_big_endian (HAVE_BIG_ENDIAN)

# set compiler flags
# string concat is easier in 3.0, but older versions don't have the concat subcommand
# so we are using list as input until we move to new version
set (CFLAGS_LIST
    "-std=c99 "
    "-Wall "
    "-Wmissing-prototypes -Wmissing-declarations -Wredundant-decls "
    "-Wunused-function -Wunused-value -Wunused-variable "
    "-fstrict-aliasing "
    "-ggdb3 -O2 "
    )
string (REPLACE "" "" CFLAGS ${CFLAGS_LIST})
set (CMAKE_C_FLAGS
    "${CMAKE_C_FLAGS}  ${CFLAGS}"
    )

# where to find include files
include_directories ("${PROJECT_BINARY_DIR}" include)

# all source files listed here
set (SOURCE
    src/cc_array.c
    src/cc_bstring.c
    src/cc_debug.c
    src/cc_log.c
    src/cc_mbuf.c
    src/cc_mm.c
    src/cc_nio.c
    src/cc_print.c
    src/cc_option.c
    src/cc_stream.c
    src/hash/cc_hash.c
    src/hash/cc_lookup3.c
    src/stats/cc_metric.c
    )
# some files are platform-specific
if (OperatingSystem STREQUAL "OS_DARWIN")
    set (SOURCE
        ${SOURCE}
        src/event/cc_kqueue.c
        src/time/cc_timer_darwin.c
        )
elseif (OperatingSystem STREQUAL "OS_LINUX")
    set (SOURCE
        ${SOURCE}
        src/event/cc_epoll.c
        src/time/cc_timer_linux.c
        )
endif ()

# targets to build: here we have both static and dynmaic libs
set (LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
add_library (ccommon-static STATIC ${SOURCE})
add_library (ccommon-dynamic SHARED ${SOURCE})
set_target_properties (ccommon-dynamic
    PROPERTIES
    OUTPUT_NAME ccommon
    VERSION ${CCOMMON_VERSION}
    SOVERSION 0
    )
set_target_properties (ccommon-static
    PROPERTIES
    OUTPUT_NAME ccommon-${CCOMMON_VERSION}
    )

# install instructions
install (TARGETS ccommon-static DESTINATION lib)
install (TARGETS ccommon-dynamic DESTINATION lib)
install (DIRECTORY include/
    DESTINATION include/ccommon-${CCOMMON_RELEASE_VERSION}
    FILES_MATCHING PATTERN "*.h")
