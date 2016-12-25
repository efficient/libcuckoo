add_executable(insert_throughput insert_throughput.cc)
target_link_libraries(insert_throughput
    PRIVATE test_util
    PRIVATE pcg
    PRIVATE libcuckoo
)

add_executable(read_throughput read_throughput.cc)
target_link_libraries(read_throughput
    PRIVATE test_util
    PRIVATE pcg
    PRIVATE libcuckoo
)

add_executable(read_insert_throughput read_insert_throughput.cc)
target_link_libraries(read_insert_throughput
    PRIVATE test_util
    PRIVATE pcg
    PRIVATE libcuckoo
)

add_optimized_cxx_flags(insert_throughput read_throughput read_insert_throughput)

add_test(NAME insert_throughput COMMAND insert_throughput --table-capacity 1)
add_test(NAME read_throughput COMMAND read_throughput)
add_test(NAME read_insert_throughput COMMAND read_insert_throughput --table-capacity 1)
