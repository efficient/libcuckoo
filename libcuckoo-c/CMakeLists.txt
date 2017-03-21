add_library(libcuckoo-c-intf INTERFACE)
add_library(libcuckoo-c-impl INTERFACE)

# Include relative to the base directory
target_include_directories(libcuckoo-c-intf INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(libcuckoo-c-impl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Link the implementation to libcuckoo
target_link_libraries(libcuckoo-c-impl INTERFACE libcuckoo)

install(
FILES
    cuckoo_table_template.h
    cuckoo_table_template.cc
DESTINATION
    ${CMAKE_INSTALL_PREFIX}/include/libcuckoo-c
)
