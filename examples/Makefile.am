LDFLAGS = -lpthread -L$(top_builddir)/libcuckoo -lcityhash
DEPS = $(top_builddir)/libcuckoo/libcityhash.la

noinst_PROGRAMS = hellohash count_freq

hellohash_SOURCES = hellohash.cc
count_freq_SOURCES = count_freq.cc

AM_CPPFLAGS = -I$(top_srcdir)
