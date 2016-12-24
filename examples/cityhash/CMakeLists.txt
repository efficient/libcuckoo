# Run configure to generate the config.h file
execute_process(COMMAND cmake -E chdir ${CMAKE_CURRENT_SOURCE_DIR}
                        ${CMAKE_CURRENT_SOURCE_DIR}/configure)

# Add the current directory as an interface library to include
add_library(cityhash_config INTERFACE)
target_include_directories(cityhash_config INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)    

# The src subdirectory contains the actual library definition
add_subdirectory(src)
