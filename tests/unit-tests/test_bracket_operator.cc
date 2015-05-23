#include "catch.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "../../src/cuckoohash_map.hh"

TEST_CASE("bracket find empty table", "[bracket]") {
    cuckoohash_map<int, int> table;
    auto ref = table[10];
    REQUIRE_THROWS_AS((void) ((int) ref), std::out_of_range);
}

TEST_CASE("bracket find filled table", "[bracket]") {
    cuckoohash_map<std::string, int> table;
    for (int i = 0; i < 10; ++i) {
        table.insert(std::to_string(i), i);
    }

    for (int i = 0; i < 10; ++i) {
        REQUIRE(table[std::to_string(i)] == i);
    }
}

TEST_CASE("bracket insert", "[bracket]") {
    cuckoohash_map<int, int> table;
    for (int i = 0; i < 10; ++i) {
        table[i] = i + 1;
    }
    for (int i = 0; i < 10; ++i) {
        REQUIRE(table[i] == i + 1);
    }
}

TEST_CASE("bracket assign to reference", "[bracket]") {
    cuckoohash_map<int, int> table;
    table.insert(0, 0);
    for (int i = 1; i < 10; ++i) {
        table[i] = table[i - 1];
    }
    for (int i = 0; i < 10; ++i) {
        REQUIRE(table[i] == 0);
    }
}

TEST_CASE("bracket assign updates", "[bracket]") {
    cuckoohash_map<int, int> table;
    table.insert(0, 0);
    REQUIRE(table[0] == 0);
    table[0] = 10;
    REQUIRE(table[0] == 10);
}
