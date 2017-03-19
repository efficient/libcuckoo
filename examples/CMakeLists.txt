add_subdirectory(cityhash)

add_executable(nested_table nested_table.cc)
target_link_libraries(nested_table libcuckoo)

add_executable(hellohash hellohash.cc)
target_link_libraries(hellohash libcuckoo cityhash)

add_executable(count_freq count_freq.cc)
target_link_libraries(count_freq libcuckoo cityhash)

add_library(int_str_table STATIC int_str_table.h int_str_table.cc)
target_link_libraries(int_str_table PUBLIC libcuckoo-c)

add_executable(hellohash_c hellohash_c.c)
target_link_libraries(hellohash_c PRIVATE int_str_table)
set_property(TARGET hellohash_c PROPERTY C_STANDARD 99)
