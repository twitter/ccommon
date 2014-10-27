lib_LTLIBRARIES = libccommon-@CCOMMON_RELEASE_VERSION@.la

libccommon_@CCOMMON_RELEASE_VERSION@_la_LDFLAGS = -version-info $(CCOMMON_SO_VERSION)

MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS =
AM_CPPFLAGS += -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64
AM_CPPFLAGS += -I $(top_srcdir)/src/include

AM_CFLAGS =
AM_CFLAGS += -Wall
AM_CFLAGS += -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls
AM_CFLAGS += -Wunused-function -Wunused-value -Wunused-variable
AM_CFLAGS += -fstrict-aliasing -std=c99
AM_CFLAGS += -g

AM_LDFLAGS =
AM_LDFLAGS += -rdynamic

libccommon_@CCOMMON_RELEASE_VERSION@_la_SOURCES =			\
	cc_array.c							\
	cc_bstring.c							\
	cc_debug.c							\
	cc_log.c							\
	cc_mbuf.c							\
	cc_mm.c								\
	cc_nio.c							\
	cc_print.c							\
	cc_option.c							\
	cc_stream.c							\
	hash/cc_hash.c							\
	hash/cc_lookup3.c						\
	stats/cc_metric.c

if OS_LINUX
libccommon_@CCOMMON_RELEASE_VERSION@_la_SOURCES +=			\
	event/cc_epoll.c						\
	time/cc_timer_linux.c
endif

# APPLE / OSX
if OS_DARWIN
libccommon_@CCOMMON_RELEASE_VERSION@_la_SOURCES +=			\
	event/cc_kqueue.c						\
	time/cc_timer_darwin.c
endif

ccommon_includedir = $(includedir)/ccommon-$(CCOMMON_RELEASE_VERSION)
ccommon_include_HEADERS =						\
	include/cc_array.h						\
	include/cc_bstring.h						\
	include/cc_debug.h						\
	include/cc_define.h						\
	include/cc_log.h						\
	include/cc_mbuf.h						\
	include/cc_mm.h							\
	include/cc_nio.h						\
	include/cc_pool.h						\
	include/cc_print.h						\
	include/cc_queue.h						\
	include/cc_option.h						\
	include/cc_stream.h						\
	include/cc_util.h						\
	include/cc_event.h						\
	include/cc_hash.h						\
	include/cc_lookup3.h						\
	include/cc_metric.h						\
	include/cc_timer.h
