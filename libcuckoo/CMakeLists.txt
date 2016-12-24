add_library(libcuckoo INTERFACE)

# Include relative to the base directory
target_include_directories(libcuckoo INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Enable C++11 for all targets that link with libcuckoo
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(libcuckoo INTERFACE -std=c++11 -stdlib=libc++)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(libcuckoo INTERFACE -std=gnu++11)
endif()


install(
FILES
    city_hasher.hh
    cuckoohash_config.hh
    cuckoohash_map.hh
    cuckoohash_util.hh
    lazy_array.hh
DESTINATION
    ${CMAKE_INSTALL_PREFIX}/include/libcuckoo
)
