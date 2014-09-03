AM_CPPFLAGS =
AM_CPPFLAGS += -I $(top_srcdir)/src

AM_CFLAGS = -Wall
AM_CFLAGS += -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls
AM_CFLAGS += -Wunused-function -Wunused-value -Wunused-variable
AM_CFLAGS += -fstrict-aliasing
AM_CFLAGS += -g

AM_LDFLAGS = -rdynamic

bin_PROGRAMS = membenchmark

membenchmark_SOURCES =					\
	$(top_srcdir)/src/cc_debug.h			\
	$(top_srcdir)/src/cc_define.h			\
	$(top_srcdir)/src/cc_log.h			\
	$(top_srcdir)/src/cc_mm.h			\
	$(top_srcdir)/src/cc_queue.h			\
	$(top_srcdir)/src/cc_string.h			\
	$(top_srcdir)/src/cc_time.h			\
	$(top_srcdir)/src/data_structure/cc_zipmap.h	\
	$(top_srcdir)/src/hash/cc_hash.h		\
	$(top_srcdir)/src/hash/cc_hash_table.h		\
	$(top_srcdir)/src/mem/cc_item.h			\
	$(top_srcdir)/src/mem/cc_mem_interface.h	\
	$(top_srcdir)/src/mem/cc_settings.h		\
	$(top_srcdir)/src/mem/cc_slab.h			\
	cc_membench.c

membenchmark_CFLAGS = $(AM_CFLAGS)

membenchmark_LDADD = $(top_srcdir)/src/libccommon-@CCOMMON_RELEASE_VERSION@.la
