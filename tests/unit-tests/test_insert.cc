#include <iostream>

#include "catch.hpp"

#include "../../src/cuckoohash_map.hh"
#include "unit_test_util.hh"

class BadHashFunction {
public:
    int operator()(int key) {
        return 0;
    }
};

TEST_CASE("minimum load factor caps automatic expansion", "[insert]") {
    const size_t slot_per_bucket = 4;
    cuckoohash_map<int, int, BadHashFunction, std::equal_to<int>,
                   std::allocator<std::pair<const int, int>>,
                   slot_per_bucket> tbl(2);

    for (size_t i = 0; i < 2 * slot_per_bucket; ++i) {
        tbl.insert(i, i);
    }

    std::cout << "load factor = " << tbl.load_factor() << std::endl;

    REQUIRE_THROWS_AS(tbl.insert(2*slot_per_bucket, 0),
                      libcuckoo_load_factor_too_low);
}
