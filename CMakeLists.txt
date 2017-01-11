cmake_minimum_required(VERSION 3.1.0)
project(libcuckoo LANGUAGES CXX)
set(libcuckoo_VERSION_MAJOR 0)
set(libcuckoo_VERSION_MINOR 2)
set(CMAKE_BUILD_TYPE Release CACHE STRING "Default build type")

# Add the libcuckoo interface target
add_subdirectory(libcuckoo)

# Build examples
if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# Build tests -- this only builds tests that were specified
enable_testing()
add_subdirectory(tests)
