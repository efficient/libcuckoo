add_executable(universal_benchmark
    universal_benchmark.cc
    universal_gen.hh
    universal_table_wrapper.hh
)

target_link_libraries(universal_benchmark
    PRIVATE test_util
    PRIVATE libcuckoo
)

if(NOT DEFINED UNIVERSAL_KEY)
    set(UNIVERSAL_KEY uint64_t)
endif()

if(NOT DEFINED UNIVERSAL_VALUE)
    set(UNIVERSAL_VALUE uint64_t)
endif()

if(NOT DEFINED UNIVERSAL_TABLE)
    set(UNIVERSAL_TABLE LIBCUCKOO)
endif()

if(UNIVERSAL_TABLE STREQUAL TBB)
    target_link_libraries(universal_benchmark PRIVATE tbb)
endif()

target_compile_options(universal_benchmark
    PRIVATE -DKEY=${UNIVERSAL_KEY}
    PRIVATE -DVALUE=${UNIVERSAL_VALUE}
    PRIVATE -D${UNIVERSAL_TABLE}
)

add_optimized_cxx_flags(universal_benchmark)
