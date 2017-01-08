add_executable(unit_tests
    test_constructor.cc
    test_hash_properties.cc
    test_heterogeneous_compare.cc
    test_iterator.cc
    test_maximum_hashpower.cc
    test_minimum_load_factor.cc
    test_noncopyable_types.cc
    test_resize.cc
    test_runner.cc
    test_user_exceptions.cc
    unit_test_util.cc
    unit_test_util.hh
)

target_link_libraries(unit_tests
    PRIVATE catch
    PRIVATE libcuckoo
)

add_debug_cxx_flags(unit_tests)

add_test(NAME unit_tests COMMAND unit_tests)
