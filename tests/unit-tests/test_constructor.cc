#include "catch.hpp"

#include <cmath>
#include <stdexcept>

#include "../../src/cuckoohash_map.hh"
#include "unit_test_util.hh"

TEST_CASE("default size", "[constructor]") {
    IntIntTable tbl;
    REQUIRE(tbl.size() == 0);
    REQUIRE(tbl.empty());
    REQUIRE(tbl.hashpower() == (size_t) log2(DEFAULT_SIZE / 4));
    REQUIRE(tbl.bucket_count() == DEFAULT_SIZE / 4);
    REQUIRE(tbl.load_factor() == 0);
}

TEST_CASE("given size", "[constructor]") {
    IntIntTable tbl(1);
    REQUIRE(tbl.size() == 0);
    REQUIRE(tbl.empty());
    REQUIRE(tbl.hashpower() == 1);
    REQUIRE(tbl.bucket_count() == 2);
    REQUIRE(tbl.load_factor() == 0);
}

TEST_CASE("frees even with exceptions", "[constructor]") {
    typedef IntIntTableWithAlloc< TrackingAllocator<int, 0>> no_space_table;
    // Should throw when allocating the TableInfo struct
    REQUIRE_THROWS_AS(no_space_table(1), std::bad_alloc);
    REQUIRE(get_unfreed_bytes() == 0);

    typedef IntIntTableWithAlloc<
        TrackingAllocator<int, UnitTestInternalAccess::IntIntTableInfoSize>>
        some_space_table;
    // Should throw when constructing the TableInfo struct, which involves
    // allocating the buckets and counters
    REQUIRE_THROWS_AS(some_space_table(1), std::bad_alloc);
    REQUIRE(get_unfreed_bytes() == 0);
}
