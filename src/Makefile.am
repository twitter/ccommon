lib_LTLIBRARIES = libccommon-@CCOMMON_RELEASE_VERSION@.la

libccommon_@CCOMMON_RELEASE_VERSION@_la_LDFLAGS = -version-info $(CCOMMON_SO_VERSION)

MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64
#AM_CPPFLAGS += -I $(top_srcdir)/src/event

AM_CFLAGS = -Wall
AM_CFLAGS += -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls
AM_CFLAGS += -Wunused-function -Wunused-value -Wunused-variable
AM_CFLAGS += -fstrict-aliasing -flat_namespace

AM_LDFLAGS = -rdynamic -Wl -flat_namespace


libccommon_@CCOMMON_RELEASE_VERSION@_la_SOURCES =			\
	cc_array.c							\
	cc_debug.c							\
	cc_log.c							\
	cc_mbuf.c							\
	cc_mm.c								\
	cc_nio.c							\
	cc_string.c							\
	event/cc_epoll.c						\
	event/cc_kqueue.c						\
	hash/cc_hash.c							\
	hash/cc_hash_table.c						\
	mem/cc_mem_globals.c						\
	mem/cc_mem_interface.c						\
	mem/cc_items.c							\
	mem/cc_settings.c						\
	mem/cc_slabs.c							\
	mem/cc_time.c                                                   \
	mem/cc_zipmap.c

ccommon_includedir = $(includedir)/ccommon-$(CCOMMON_RELEASE_VERSION)
ccommon_include_HEADERS =						\
	cc_array.h							\
	cc_debug.h							\
	cc_log.h							\
	cc_mbuf.h							\
	cc_mm.h								\
	cc_nio.h							\
	cc_queue.h							\
	cc_string.h							\
	cc_define.h							\
	cc_util.h							\
	event/cc_event.h						\
	hash/cc_hash.h							\
	hash/cc_hash_table.h						\
	mem/cc_mem_globals.h						\
	mem/cc_mem_interface.h						\
	mem/cc_items.h							\
	mem/cc_queue.h							\
	mem/cc_settings.h						\
	mem/cc_slabs.h							\
	mem/cc_time.h                                                   \
	mem/cc_zipmap.h
