AM_CPPFLAGS =
AM_CPPFLAGS += -I $(top_srcdir)/src

TESTS = 				\
	check_mem_1

check_PROGRAMS = 			\
	check_mem_1

#check_util_SOURCES = check_util.c $(top_builddir)/src/cc_util.h
#check_util_CFLAGS = @CHECK_CFLAGS@
#check_util_LDADD = $(top_builddir)/src/libcommon.la @CHECK_LIBS@

#check_mbuf_SOURCES = check_mbuf.c $(top_builddir)/src/cc_mbuf.h
#check_mbuf_CFLAGS = @CHECK_CFLAGS@
#check_mbuf_LDADD = $(top_builddir)/src/libcommon.la @CHECK_LIBS@

check_mem_1_SOURCES = check_mem_1.c $(top_builddir)/src/cc_interface.h $(top_builddir)/src/cc_settings.h $(top_builddir)/src/cc_assoc.h $(top_builddir)/src/cc_slabs.h $(top_builddir)/src/cc_items.h
check_mem_1_CFLAGS = @CHECK_CFLAGS@
check_mem_1_LDADD = $(top_builddir)/src/libccommon-1.0.la @CHECK_LIBS@
