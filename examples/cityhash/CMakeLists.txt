# Run configure to generate the build files in a special directory
execute_process(COMMAND cmake -E chdir ${CMAKE_CURRENT_BINARY_DIR}
                              ${CMAKE_CURRENT_SOURCE_DIR}/configure)

# Add the current build directory as an interface library to include, since it
# contains some necessary configuration files from running 'configure'.
add_library(cityhash_config INTERFACE)
target_include_directories(cityhash_config INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

# The src subdirectory contains the actual library definition
add_subdirectory(src)
