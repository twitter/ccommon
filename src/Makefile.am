lib_LTLIBRARIES = libccommon-@CCOMMON_RELEASE_VERSION@.la

libccommon_@CCOMMON_RELEASE_VERSION@_la_LDFLAGS = -version-info $(CCOMMON_SO_VERSION)

MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS =
AM_CPPFLAGS += -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64
AM_CPPFLAGS += -I $(top_srcdir)/src
AM_CPPFLAGS += -I $(top_srcdir)/src/event
AM_CPPFLAGS += -I $(top_srcdir)/src/hash
AM_CPPFLAGS += -I $(top_srcdir)/src/stats

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
	event/cc_epoll.c						\
	event/cc_kqueue.c						\
	hash/cc_hash.c							\
	hash/cc_lookup3.c						\
	stats/cc_metric.c

ccommon_includedir = $(includedir)/ccommon-$(CCOMMON_RELEASE_VERSION)
ccommon_include_HEADERS =						\
	$(top_srcdir)/config.h						\
	cc_array.h							\
	cc_bstring.h							\
	cc_debug.h							\
	cc_define.h							\
	cc_log.h							\
	cc_mbuf.h							\
	cc_mm.h								\
	cc_nio.h							\
	cc_pool.h							\
	cc_print.h							\
	cc_queue.h							\
	cc_option.h							\
	cc_stream.h							\
	cc_util.h							\
	event/cc_event.h						\
	hash/cc_hash.h							\
	hash/cc_lookup3.h						\
	stats/cc_metric.h
