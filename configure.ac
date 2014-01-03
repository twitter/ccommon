# Define the package version numbers and the bug reporting address
m4_define([CC_MAJOR], 0)
m4_define([CC_MINOR], 0)
m4_define([CC_PATCH], 1)
m4_define([CC_BUGS], [yao@twitter.com])

# Initialize autoconf
AC_PREREQ([2.64])
AC_INIT([ccommon], [CC_MAJOR.CC_MINOR.CC_PATCH], [CC_BUGS])
AC_CONFIG_SRCDIR([src/cc_util.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_HEADERS([config.h:config.h.in])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_SYSTEM

# Initialize automake
AM_INIT_AUTOMAKE

# Define macro variables for the package version numbers
AC_DEFINE(CC_VERSION_MAJOR, CC_MAJOR, [Define the major version number])
AC_DEFINE(CC_VERSION_MINOR, CC_MINOR, [Define the minor version number])
AC_DEFINE(CC_VERSION_PATCH, CC_PATCH, [Define the patch version number])
AC_DEFINE(CC_VERSION_STRING, "CC_MAJOR.CC_MINOR.CC_PATCH", [Define the version string])

# Checks for language
AC_LANG([C])

# Checks for programs
AC_PROG_CC
AC_PROG_MAKE_SET

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
#AC_CHECK_HEADERS([float.h limits.h])
AC_CHECK_HEADERS([inttypes.h stdint.h])
AC_CHECK_HEADERS([ctype.h errno.h execinfo.h time.h])
AC_CHECK_HEADERS([fcntl.h string.h unistd.h])
AC_CHECK_HEADERS([sys/mman.h sys/stat.h])
#AC_CHECK_HEADERS([sys/ioctl.h sys/time.h sys/uio.h])
#AC_CHECK_HEADERS([sys/socket.h sys/un.h netinet/in.h arpa/inet.h netdb.h])
#AC_CHECK_HEADERS([sys/epoll.h], [], [AC_MSG_ERROR([required sys/epoll.h header file is missing])])

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

# Check whether to enable debug logs and asserts
AC_MSG_CHECKING([whether to enable debug logs and asserts])
AC_ARG_ENABLE([debug],
[AS_HELP_STRING(
    [--enable-debug=@<:@full|yes|log|no@:>@],
    [enable debug logs and asserts @<:@default=no@:>@])
  ],
  [],
  [enable_debug=no])
AS_CASE([x$enable_debug],
  [xfull], [AC_DEFINE([HAVE_ASSERT_PANIC], [1],
                      [Define to 1 if panic on an assert is enabled])
            AC_DEFINE([HAVE_DEBUG_LOG], [1], [Define to 1 if debug log is enabled])
           ],
  [xyes], [AC_DEFINE([HAVE_ASSERT_LOG], [1],
                     [Define to 1 if log on an assert is enabled])
           AC_DEFINE([HAVE_DEBUG_LOG], [1], [Define to 1 if debug log is enabled])
          ],
  [xlog], [AC_DEFINE([HAVE_DEBUG_LOG], [1], [Define to 1 if debug log is enabled])],
  [xno], [],
  [AC_MSG_FAILURE([invalid value ${enable_debug} for --enable-debug])])
AC_MSG_RESULT([$enable_debug])

# Define Makefiles
AC_CONFIG_FILES([Makefile
                 src/Makefile])

# Generate the "configure" script
AC_OUTPUT
