// Tests that old TableInfos are deleted upon resize and explicitly with purge

#include "catch.hpp"

#include <atomic>
#include <thread>
#include <utility>

#include "../../src/cuckoohash_map.hh"
#include "unit_test_util.hh"

TEST_CASE("empty table", "[free_old_tables]") {
    IntIntTable tbl;
    REQUIRE(UnitTestInternalAccess::old_table_info_size(tbl) == 0);
}

TEST_CASE("single-threaded resize", "[free_old_tables]") {
    IntIntTable tbl(10);
    REQUIRE(UnitTestInternalAccess::old_table_info_size(tbl) == 0);
    tbl.rehash(5);
    REQUIRE(UnitTestInternalAccess::old_table_info_size(tbl) == 0);
    tbl.rehash(15);
    REQUIRE(UnitTestInternalAccess::old_table_info_size(tbl) == 0);
}

TEST_CASE("single-threaded inserts", "[free_old_tables]") {
    IntIntTable tbl(1);
    size_t capacity = table_capacity(tbl);
    for (size_t i = 0; i < capacity + 1; ++i) {
        tbl.insert(i, i);
    }
    REQUIRE(tbl.hashpower() > 1);
    REQUIRE(UnitTestInternalAccess::old_table_info_size(tbl) == 0);
}

TEST_CASE("concurrent resize and snapshot", "[free_old_tables]") {
    IntIntTable tbl(1);
    // If we resize while another thread has snapshotted the table (and thus set
    // its hazard pointer), the resize should not free the old TableInfo until
    // we explicitly purge it. We do this twice, so that there should be two
    // unfreed tables before we purge.
    std::atomic<bool> finished_resize_one(false);
    std::atomic<bool> finished_resize_two(false);
    auto staller = [&tbl] (std::atomic<bool>& finished_resize) {
            auto res = UnitTestInternalAccess::snapshot_table_nolock(tbl);
            // Block with the hazard pointer set until the resize is finished
            while (!finished_resize.load());
    };

    std::thread t1(staller, std::ref(finished_resize_one));
    tbl.rehash(2);
    REQUIRE(tbl.hashpower() == 2);
    REQUIRE(UnitTestInternalAccess::old_table_info_size(tbl) == 1);

    std::thread t2(staller, std::ref(finished_resize_two));
    tbl.rehash(3);
    REQUIRE(tbl.hashpower() == 3);
    REQUIRE(UnitTestInternalAccess::old_table_info_size(tbl) == 2);

    finished_resize_one.store(true);
    finished_resize_two.store(true);
    t1.join();
    t2.join();
    REQUIRE(UnitTestInternalAccess::old_table_info_size(tbl) == 2);

    tbl.purge();
    REQUIRE(UnitTestInternalAccess::old_table_info_size(tbl) == 0);
}
