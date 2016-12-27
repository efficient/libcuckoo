add_library(cityhash city.cc city.h)
target_link_libraries(cityhash cityhash_config)
target_include_directories(cityhash PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
