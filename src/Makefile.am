lib_LTLIBRARIES = libccommon-@CCOMMON_RELEASE_VERSION@.la


libccommon_@CCOMMON_RELEASE_VERSION@_la_LDFLAGS = -version-info $(CCOMMON_SO_VERSION)

AM_CPPFLAGS = -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64

AM_CFLAGS = -Wall
AM_CFLAGS += -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls
AM_CFLAGS += -Wunused-function -Wunused-value -Wunused-variable
AM_CFLAGS += -fstrict-aliasing

LDFLAGS += -rdynamic

libccommon_@CCOMMON_RELEASE_VERSION@_la_SOURCES =					\
	cc_log.c							\
	cc_mm.c								\
	cc_debug.c							\
	cc_string.c

ccommon_includedir = $(includedir)/ccommon-$(CCOMMON_RELEASE_VERSION)
nobase_ccommon_include_HEADERS =		\
	cc_type.h							\
	cc_util.h							\
	cc_log.h							\
	cc_mm.h								\
	cc_debug.h							\
	cc_string.h
