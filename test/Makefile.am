TESTS = 				\
	check_mbuf			\
	check_util

check_PROGRAMS = 		\
	check_mbuf			\
	check_util

check_util_SOURCES = check_util.c $(top_builddir)/src/cc_util.h
check_util_CFLAGS = @CHECK_CFLAGS@
check_util_LDADD = $(top_builddir)/src/libcommon.la @CHECK_LIBS@

check_mbuf_SOURCES = check_mbuf.c $(top_builddir)/src/cc_mbuf.h
check_util_CFLAGS = @CHECK_CFLAGS@
check_util_LDADD = $(top_builddir)/src/libcommon.la @CHECK_LIBS@

