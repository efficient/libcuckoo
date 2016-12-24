add_subdirectory(cityhash)

add_executable(nested_table nested_table.cc)
target_link_libraries(nested_table libcuckoo)

add_executable(hellohash hellohash.cc)
target_link_libraries(hellohash libcuckoo cityhash)

add_executable(count_freq count_freq.cc)
target_link_libraries(count_freq libcuckoo cityhash)
