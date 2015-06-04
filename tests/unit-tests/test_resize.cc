#include "catch.hpp"

#include <cmath>

#include "../../src/cuckoohash_map.hh"

TEST_CASE("rehash empty table", "[resize]") {
    cuckoohash_map<int, int> table(1);
    REQUIRE(table.hashpower() == 1);

    table.rehash(20);
    REQUIRE(table.hashpower() == 20);

    table.rehash(1);
    REQUIRE(table.hashpower() == 1);
}

TEST_CASE("reserve empty table", "[resize]") {
    cuckoohash_map<int, int> table(1);
    table.reserve(100);
    REQUIRE(table.hashpower() == (size_t)ceil(log2(100.0 / SLOT_PER_BUCKET)));

    table.reserve(1);
    REQUIRE(table.hashpower() == 1);

    table.reserve(2);
    REQUIRE(table.hashpower() == 1);
}
