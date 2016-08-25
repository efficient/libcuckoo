#include <iostream>

#include "catch.hpp"

#include "../../src/cuckoohash_map.hh"
#include "unit_test_util.hh"

class BadHashFunction {
public:
    int operator()(int) {
        return 0;
    }
};

TEST_CASE("caps automatic expansion", "[minimum load fator]") {
    const size_t slot_per_bucket = 4;
    cuckoohash_map<int, int, BadHashFunction, std::equal_to<int>,
                   std::allocator<std::pair<const int, int>>,
                   slot_per_bucket> tbl(16);
    tbl.minimum_load_factor(0.6);

    for (size_t i = 0; i < 2 * slot_per_bucket; ++i) {
        tbl.insert(i, i);
    }

    REQUIRE_THROWS_AS(tbl.insert(2*slot_per_bucket, 0),
                      libcuckoo_load_factor_too_low);
}

TEST_CASE("invalid minimum load factor", "[minimum load factor]") {
    REQUIRE_THROWS_AS(IntIntTable(5, -0.01), std::invalid_argument);
    REQUIRE_THROWS_AS(IntIntTable(5, 1.01), std::invalid_argument);

    IntIntTable t;
    REQUIRE(t.minimum_load_factor() == DEFAULT_MINIMUM_LOAD_FACTOR);
    REQUIRE_THROWS_AS(t.minimum_load_factor(-0.01), std::invalid_argument);
    REQUIRE_THROWS_AS(t.minimum_load_factor(1.01), std::invalid_argument);
}
