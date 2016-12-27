# Run configure to generate the build files in a special directory
set(cityhash_build_dir ${CMAKE_BINARY_DIR}/cityhash-build)
execute_process(COMMAND cmake -E make_directory ${cityhash_build_dir})
execute_process(COMMAND cmake -E chdir ${cityhash_build_dir}
                              ${CMAKE_CURRENT_SOURCE_DIR}/configure)

# Add the current directory as an interface library to include
add_library(cityhash_config INTERFACE)
target_include_directories(cityhash_config INTERFACE
    $<BUILD_INTERFACE:${cityhash_build_dir}>
)

# The src subdirectory contains the actual library definition
add_subdirectory(src)
