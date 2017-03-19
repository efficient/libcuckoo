add_library(libcuckoo-c INTERFACE)

# Include relative to the base directory
target_include_directories(libcuckoo-c INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Link to libcuckoo
target_link_libraries(libcuckoo-c INTERFACE libcuckoo)

install(
FILES
    cuckoo_table_template.h
    cuckoo_table_template.cc
DESTINATION
    ${CMAKE_INSTALL_PREFIX}/include/libcuckoo-c
)
