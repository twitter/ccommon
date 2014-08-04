AM_CPPFLAGS =
AM_CPPFLAGS += -I $(top_srcdir)/src

AM_CFLAGS = -Wall
AM_CFLAGS += -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls
AM_CFLAGS += -Wunused-function -Wunused-value -Wunused-variable
AM_CFLAGS += -fstrict-aliasing -flat_namespace
AM_CFLAGS += -g

AM_LDFLAGS = -rdynamic -Wl -flat_namespace

bin_PROGRAMS = membenchmark

membenchmark_SOURCES =							\
	$(top_builddir)/src/cc_debug.c $(top_builddir)/src/cc_debug.h	\
	$(top_builddir)/src/cc_log.c $(top_builddir)/src/cc_log.h	\
	$(top_builddir)/src/cc_mm.c $(top_builddir)/src/cc_mm.h		\
	$(top_builddir)/src/cc_queue.h					\
	$(top_builddir)/src/cc_string.c $(top_builddir)/src/cc_string.h \
	$(top_builddir)/src/cc_define.h					\
	$(top_builddir)/src/hash/cc_hash.c				\
	$(top_builddir)/src/hash/cc_hash.h				\
	$(top_builddir)/src/hash/cc_hash_table.c			\
	$(top_builddir)/src/hash/cc_hash_table.h			\
	$(top_builddir)/src/mem/cc_mem_interface.c			\
	$(top_builddir)/src/mem/cc_mem_interface.h			\
	$(top_builddir)/src/mem/cc_items.c				\
	$(top_builddir)/src/mem/cc_items.h				\
	$(top_builddir)/src/mem/cc_settings.c				\
	$(top_builddir)/src/mem/cc_settings.h				\
	$(top_builddir)/src/mem/cc_slabs.c				\
	$(top_builddir)/src/mem/cc_slabs.h				\
	$(top_builddir)/src/mem/cc_time.c				\
	$(top_builddir)/src/mem/cc_time.h				\
	$(top_builddir)/src/data_structure/cc_zipmap.c			\
	$(top_builddir)/src/data_structure/cc_zipmap.h			\
	cc_membench.c

membenchmark_CFLAGS = $(AM_CFLAGS)
