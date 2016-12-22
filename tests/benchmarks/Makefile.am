AM_CXXFLAGS = -O3 -DNDEBUG

TESTS = benchmark-driver.sh
noinst_PROGRAMS = insert_throughput.out read_throughput.out \
	read_insert_throughput.out universal_benchmark.out

insert_throughput_out_SOURCES = insert_throughput.cc
read_throughput_out_SOURCES = read_throughput.cc
read_insert_throughput_out_SOURCES = read_insert_throughput.cc

universal_benchmark_out_SOURCES = universal_benchmark.cc
universal_benchmark_out_CXXFLAGS = $(AM_CXXFLAGS) -DKEY=$(KEY) -DVALUE=$(VALUE)
