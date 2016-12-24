add_library(catch INTERFACE)
target_include_directories(catch INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/single_include>
)