#include "catch.hpp"

#include <cmath>

#include "../../src/cuckoohash_map.hh"

using Table = cuckoohash_map<
    int, int, std::hash<int>, std::equal_to<int>, 4>;

TEST_CASE("rehash empty table", "[resize]") {
    Table table(1);
    REQUIRE(table.hashpower() == 1);

    table.rehash(20);
    REQUIRE(table.hashpower() == 20);

    table.rehash(1);
    REQUIRE(table.hashpower() == 1);
}

TEST_CASE("reserve empty table", "[resize]") {
    Table table(1);
    table.reserve(100);
    REQUIRE(table.hashpower() == 5);

    table.reserve(1);
    REQUIRE(table.hashpower() == 1);

    table.reserve(2);
    REQUIRE(table.hashpower() == 1);
}
