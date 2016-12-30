add_executable(universal_benchmark
    universal_benchmark.cc
    universal_gen.hh
    universal_table_wrapper.hh
)

target_link_libraries(universal_benchmark
    PRIVATE test_util
    PRIVATE libcuckoo
    PRIVATE pcg
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

target_compile_options(universal_benchmark
    PRIVATE -DKEY=${UNIVERSAL_KEY}
    PRIVATE -DVALUE=${UNIVERSAL_VALUE}
    PRIVATE -D${UNIVERSAL_TABLE}
)

add_optimized_cxx_flags(universal_benchmark)

add_test(NAME pure_read
         COMMAND universal_benchmark --reads 100 --prefill 90 --total-ops 500 --initial-capacity 25)
add_test(NAME pure_insert
         COMMAND universal_benchmark --inserts 100 --total-ops 90 --initial-capacity 25)
add_test(NAME pure_erase
         COMMAND universal_benchmark --erases 100 --prefill 90 --total-ops 90 --initial-capacity 25)
add_test(NAME pure_update
         COMMAND universal_benchmark --updates 100 --prefill 90 --total-ops 500 --initial-capacity 25)
add_test(NAME pure_upsert
         COMMAND universal_benchmark --upserts 100 --prefill 25 --total-ops 200 --initial-capacity 25)

add_test(NAME insert_expansion
         COMMAND universal_benchmark --inserts 100 --initial-capacity 4 --total-ops 209715200)
add_test(NAME read_insert_expansion
         COMMAND universal_benchmark --reads 80 --inserts 20 --initial-capacity 10 --total-ops 16384000)
