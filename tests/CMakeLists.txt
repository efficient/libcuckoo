add_library(test_util INTERFACE)
target_include_directories(test_util INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

# Add pcg if we're doing stress tests, specific benchmarks, or universal
# benchmark
if (BUILD_TESTS OR
    BUILD_STRESS_TESTS OR
    BUILD_SPECIFIC_BENCHMARKS OR
    BUILD_UNIVERSAL_BENCHMARK)
    add_subdirectory(pcg)
endif()

# Add catch for unit tests
if (BUILD_TESTS OR BUILD_UNIT_TESTS)
    add_subdirectory(Catch)
    add_subdirectory(unit-tests)
endif()

if (BUILD_TESTS OR BUILD_STRESS_TESTS)
    add_subdirectory(stress-tests)
endif()

if (BUILD_TESTS OR BUILD_SPECIFIC_BENCHMARKS)
    add_subdirectory(specific-benchmarks)
endif()

if (BUILD_TESTS OR BUILD_UNIVERSAL_BENCHMARK)
    add_subdirectory(universal-benchmark)
endif()
