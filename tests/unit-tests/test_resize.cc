#include "catch.hpp"

#include "../../src/cuckoohash_map.hh"
#include "unit_test_util.hh"

TEST_CASE("rehash empty table", "[resize]") {
    IntIntTable table(1);
    REQUIRE(table.hashpower() == 1);

    table.rehash(20);
    REQUIRE(table.hashpower() == 20);

    table.rehash(1);
    REQUIRE(table.hashpower() == 1);
}

TEST_CASE("reserve empty table", "[resize]") {
    IntIntTable table(1);
    table.reserve(100);
    REQUIRE(table.hashpower() == 5);

    table.reserve(1);
    REQUIRE(table.hashpower() == 1);

    table.reserve(2);
    REQUIRE(table.hashpower() == 1);
}

TEST_CASE("reserve calc", "[resize]") {
    const size_t slot_per_bucket = IntIntTable::slot_per_bucket;
    REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(0) == 1);
    REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
                1 * slot_per_bucket) == 1);

    REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
                2 * slot_per_bucket) == 1);
    REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
                3 * slot_per_bucket) == 2);
    REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
                4 * slot_per_bucket) == 2);
    REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
                2500000 * slot_per_bucket) == 22);

    REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
                (1UL << 31) * slot_per_bucket) == 31);
    REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
                ((1UL << 31) + 1) * slot_per_bucket == 31));

    REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
                (1UL << 61) * slot_per_bucket) == 61);
    REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
                ((1UL << 61) + 1) * slot_per_bucket == 61));

}
