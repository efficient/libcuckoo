#           -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.67])
AC_INIT([libcuckoo], [1.0], [fawn-dev@mailman.srv.cs.cmu.edu])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign])

AC_CONFIG_FILES([ Makefile
                  src/Makefile
                  examples/Makefile
                ])
AC_CONFIG_SRCDIR([src/cuckoohash_map.hh])
AC_CONFIG_SUBDIRS([cityhash-1.1.1 tests])
AC_CONFIG_MACRO_DIR([m4])

# Empty defaults for flags
: ${CFLAGS=""}
: ${CXXFLAGS=""}
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

AX_PTHREAD
LIBS="$PTHREAD_LIBS $LIBS"
CXXFLAGS="$CXXFLAGS $PTHREAD_CFLAGS"
CC="$PTHREAD_CC"

AX_CXX_COMPILE_STDCXX_11([noext],[mandatory])

CXXFLAGS="$CXXFLAGS -Wall -Wextra -Wshadow -pedantic"

AC_OUTPUT
