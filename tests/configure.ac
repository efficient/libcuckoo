#           -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.67])
AC_INIT([libcuckoo-tests], [1.0])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])

AC_CONFIG_FILES([ Makefile
                  benchmarks/Makefile
                  stress-tests/Makefile
                  unit-tests/Makefile
                ])
AC_CONFIG_MACRO_DIR([../m4])

# Empty defaults for flags
: ${CFLAGS=""}
: ${CXXFLAGS=""}
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

AX_PTHREAD
LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
CXXFLAGS="$CFLAGS $PTHREAD_CFLAGS"
CC="$PTHREAD_CC"

AX_CXX_COMPILE_STDCXX_11([noext],[mandatory])

CXXFLAGS="$CXXFLAGS -Wall -Wextra -Wshadow -pedantic"

AC_OUTPUT
