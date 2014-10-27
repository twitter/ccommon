MAINTAINERCLEANFILES = Makefile.in aclocal.m4 configure config.h.in config.h.in~ stamp-h.in

# TODO: include default location on Linux if the path exists
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ccommon-$(CCOMMON_RELEASE_VERSION).pc

SUBDIRS = src .

ccommon_includedir = $(includedir)/ccommon-$(CCOMMON_RELEASE_VERSION)
ccommon_include_HEADERS =						\
	config.h							\
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
