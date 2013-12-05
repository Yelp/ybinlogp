#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([ybinlogp], [0.1], [Roguelazer@gmail.com])
AC_USE_SYSTEM_EXTENSIONS
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])

LT_PREREQ([2.2])
LT_INIT([dlopen], [shared])

AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_CXX
AC_PROG_LN_S
AC_PROG_INSTALL
AC_PROG_RANLIB

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lybinlogp':
AC_CHECK_LIB([ybinlogp], [main])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_OFF_T
AC_C_RESTRICT
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_SYS_LARGEFILE

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memmove memset strdup strerror strndup])

# python stuff
AM_PATH_PYTHON

# options
AC_ARG_ENABLE(debug,
              [ --enable-debug     build with tons of debugging],
              [if test x$enableval = xyes ; then
                    AC_DEFINE([DEBUG], 1, [debugging enabled])
               fi])

AC_CONFIG_FILES([Makefile include/Makefile libybinlogp/Makefile src/Makefile py/Makefile])
AC_OUTPUT
