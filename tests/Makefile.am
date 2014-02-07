LDFLAGS = -lpthread -L$(top_builddir)/libcuckoo -lcityhash
DEPS = $(top_builddir)/libcuckoo/libcityhash.la

noinst_PROGRAMS = test_insert_and_find.out test_iterator.out stress_checked.out stress_unchecked.out insert_throughput.out read_throughput.out
TESTS = test_insert_and_find.out test_iterator.out stress_checked.out stress_unchecked.out insert_throughput.out read_throughput.out

test_insert_and_find_out_SOURCES = test_insert_and_find.cc
test_iterator_out_SOURCES = test_iterator.cc
stress_checked_out_SOURCES = stress_checked.cc
stress_unchecked_out_SOURCES = stress_unchecked.cc
insert_throughput_out_SOURCES = insert_throughput.cc
read_throughput_out_SOURCES = read_throughput.cc

AM_CPPFLAGS = -I$(top_srcdir)
