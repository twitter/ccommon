m4_define([CC_MAJOR], 1)
m4_define([CC_MINOR], 0)
m4_define([CC_INTERFACE], 0)
m4_define([CC_REVISION], 0)
m4_define([CC_AGE], 0)
m4_define([CC_BUGS], [yao@twitter.com])

# Initialize autoconf
AC_PREREQ([2.60])
AC_INIT([libccommon], [CC_MAJOR.CC_MINOR], [CC_BUGS])
AC_CONFIG_SRCDIR([src/cc_util.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_HEADERS([config.h:config.h.in])
AC_CONFIG_MACRO_DIR([m4])

# Checks for OS and set OS variables
AC_CANONICAL_HOST
case "$host_os" in
    linux*)
        linux=yes
        AC_DEFINE([OS_LINUX], [], [linux])
        ;;
    darwin*)
        darwin=yes
        AC_DEFINE([OS_DARWIN], [], [Mac OSX])
        ;;
    *)
        AC_MSG_ERROR([Your platform is not currently supported]) ;;
esac

# Initialize automake
AM_INIT_AUTOMAKE([1.10 check-news no-define foreign silent-rules subdir-objects
                  -Wall -Werror])
AM_SILENT_RULES([yes])

# Set library release and version numbers
AC_SUBST([CCOMMON_RELEASE_VERSION], [CC_MAJOR.CC_MINOR])
AC_SUBST([CCOMMON_SO_VERSION], [CC_INTERFACE:CC_REVISION:CC_AGE])

# Checks for language
AC_LANG([C])

# Checks for programs
AC_PROG_CC
AC_PROG_MAKE_SET
AM_PROG_CC_C_O
AM_PROG_AR

# Initialize libtool
LT_INIT
AC_PROG_LIBTOOL

# Set conditional variables
AM_CONDITIONAL([OS_LINUX], [test "$linux" = yes])
AM_CONDITIONAL([OS_DARWIN], [test "$darwin" = yes])

# Checks for typedefs, structures, and compiler characteristics
AC_C_INLINE
AC_C_CONST
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INTMAX_T
AC_TYPE_INTPTR_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINTMAX_T
AC_TYPE_UINTPTR_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UID_T

# Checks for header files
AC_HEADER_STDBOOL
AC_CHECK_HEADERS([stdarg.h stddef.h stdlib.h])
AC_CHECK_HEADERS([inttypes.h stdint.h limits.h])
AC_CHECK_HEADERS([ctype.h errno.h execinfo.h time.h])
AC_CHECK_HEADERS([fcntl.h string.h unistd.h])
AC_CHECK_HEADERS([sys/mman.h sys/stat.h sys/socket.h sys/types.h sys/uio.h])

# Checks for library functions
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([dup2])
AC_CHECK_FUNCS([gethostname])
AC_CHECK_FUNCS([gettimeofday])
AC_CHECK_FUNCS([strerror])
AC_CHECK_FUNCS([socket])
AC_CHECK_FUNCS([memchr memmove memset])
AC_CHECK_FUNCS([strchr strndup strtol strtoul strtoull])
AC_CHECK_FUNCS([backtrace])
AC_CHECK_FUNCS([read write readv writev])


# Check whether to enable asserts
AC_MSG_CHECKING([whether to enable assert log and panic])
AC_ARG_ENABLE([assert],
  [AS_HELP_STRING(
    [--enable-assert=@<:@panic|log|no@:>@],
    [enable assert and assert panic @<:@default=log@:>@])
  ],
  [],
  [enable_assert=log])
AS_CASE([x$enable_assert],
  [xpanic], [AC_DEFINE([HAVE_ASSERT_PANIC], [1],
                      [Define to 1 if panic on an assert is enabled])
            AC_DEFINE([HAVE_LOGGING], [1], [Define to 1 if logging is enabled])
           ],
  [xlog], [AC_DEFINE([HAVE_ASSERT_LOG], [1],
                     [Define to 1 if log on an assert is enabled])
           AC_DEFINE([HAVE_LOGGING], [1], [Define to 1 if logging is enabled])
          ],
  [xno], [],
  [AC_MSG_FAILURE([invalid value ${enable_assert} for --enable-assert])])
AC_MSG_RESULT([$enable_assert])

AC_MSG_CHECKING([whether to disable logging])
AC_ARG_ENABLE([logging],
  [AS_HELP_STRING(
    [--disable-logging],
    [disable logging])
  ],
  [disable_logging=yes],
  [disable_logging=no])
AS_IF([test "x$disable_logging" = xyes],
      [],
      [AC_DEFINE([HAVE_LOGGING], [1], [Define to 1 if logging is not disabled])])
AC_MSG_RESULT([$disable_logging])

AC_MSG_CHECKING([whether to disable stats])
AC_ARG_ENABLE([stats],
  [AS_HELP_STRING(
    [--disable-stats],
    [disable stats])
  ],
  [disable_stats=yes],
  [disable_stats=no])
AS_IF([test "x$disable_stats" = xyes],
  [],
  [AC_DEFINE([HAVE_STATS], [1], [Define to 1 if stats is not disabled])])
AC_MSG_RESULT($disable_stats)

# This is called on Linux systems
AC_CACHE_CHECK([if epoll works], [ac_cv_epoll_works],
  AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
int
main(int argc, char **argv)
{
    int fd;

    fd = epoll_create(256);
    if (fd < 0) {
        perror("epoll_create:");
        exit(1);
    }
    exit(0);
}
  ], [ac_cv_epoll_works=yes], [ac_cv_epoll_works=no]))
AS_IF([test "x$ac_cv_epoll_works" = "xyes"],
      [AC_DEFINE([HAVE_EPOLL], [1], [Define to 1 if epoll is supported])], [])

# This is called on Darwin systems
AC_CACHE_CHECK([if kqueue works], [ac_cv_kqueue_works],
  AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
int
main(int argc, char **argv)
{
    int fd;

    fd = kqueue();
    if (fd < 0) {
        perror("kqueue:");
        exit(1);
    }
    exit(0);
}
  ], [ac_cv_kqueue_works=yes], [ac_cv_kqueue_works=no]))
AS_IF([test "x$ac_cv_kqueue_works" = "xyes"],
      [AC_DEFINE([HAVE_KQUEUE], [1], [Define to 1 if kqueue is supported])], [])

AS_IF([test "x$ac_cv_epoll_works" = "xno" &&
       test "x$ac_cv_kqueue_works" = "xno"],
      [AC_MSG_ERROR([either epoll or kqueue support is required])], [])


# Check whether the test libraries are present
# Note: disabling test for now
# PKG_CHECK_MODULES([CHECK], [check >= 0.9.10])

# Check if we're a little-endian or a big-endian system
AC_C_BIGENDIAN(
  [AC_DEFINE(HAVE_BIG_ENDIAN, 1, [Define to 1 if machine is big endian])],
  [AC_DEFINE(HAVE_LITTLE_ENDIAN, 1, [Define to 1 if machine is little endian])],
  [AC_MSG_ERROR([endianess of this machine is unknown])],
  [AC_MSG_ERROR([universial endianess not supported])]
)

# Define Makefiles
AC_CONFIG_FILES([Makefile
                 src/Makefile
                 ccommon-${CCOMMON_RELEASE_VERSION}.pc:ccommon.pc.in])

# Generate the "configure" script
AC_OUTPUT
