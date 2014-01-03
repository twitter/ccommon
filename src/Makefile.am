lib_LTLIBRARIES = libccommon.la

AM_CPPFLAGS = -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64

AM_CFLAGS = -Wall
AM_CFLAGS += -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls
AM_CFLAGS += -Wunused-function -Wunused-value -Wunused-variable
AM_CFLAGS += -fno-strict-aliasing

LDFLAGS += -rdynamic

libccommon_la_SOURCES =			\
	cc_type.h					\
	cc_util.h					\
	cc_log.c cc_log.h			\
	cc_mm.c cc_mm.h				\
	cc_debug.c cc_debug.h		\
	cc_string.c cc_string.h

include_HEADERS =				\
	cc_type.h					\
	cc_util.h					\
	cc_log.h					\
	cc_mm.h						\
	cc_debug.h					\
	cc_string.h
