add_library(libcuckoo-c INTERFACE)

# Link the implementation to libcuckoo
target_link_libraries(libcuckoo-c INTERFACE libcuckoo)

install(
FILES
    cuckoo_table_template.h
    cuckoo_table_template.cc
DESTINATION
    ${CMAKE_INSTALL_PREFIX}/include/libcuckoo-c
)
