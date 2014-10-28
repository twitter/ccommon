cmake_minimum_required (VERSION 2.8)
project (ccommon)

set (CCOMMON_VERSION_MAJOR 1)
set (CCOMMON_VERSION_MINOR 0)
set (CCOMMON_VERSION_PATCH 0)

include (CheckFunctionExists)
check_function_exists (epoll HAVE_EPOLL)
check_function_exists (kevent HAVE_KQUEUE)
include (TestBigEndian)
test_big_endian (HAVE_BIG_ENDIAN)

configure_file (
  "${PROJECT_SOURCE_DIR}/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
  )

include_directories ("${PROJECT_BINARY_DIR}" include)

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
    src/event/cc_kqueue.c
    src/time/cc_timer_darwin.c
    )

add_library(ccommon ${SOURCE})
