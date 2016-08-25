AM_CXXFLAGS = -g -O0

if HAVE_CXX14
TESTS = test_runner.out test_runner_cxx14.out
else
TESTS = test_runner.out
endif

noinst_PROGRAMS = $(TESTS)

test_runner_out_SOURCES = \
	test_iterator.cc \
	test_bracket_operator.cc \
	test_resize.cc \
	test_constructor.cc \
	test_minimum_load_factor.cc \
	test_maximum_hashpower.cc \
	test_hash_properties.cc \
	test_noncopyable_types.cc \
	unit_test_util.cc \
	test_runner.cc catch.hpp

if HAVE_CXX14
test_runner_cxx14_out_SOURCES = \
	test_heterogeneous_compare.cc \
	test_runner.cc catch.hpp
endif
