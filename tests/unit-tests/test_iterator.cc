#include "catch.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "../../src/cuckoohash_map.hh"
#include "unit_test_util.hh"

template <class Iterator>
void AssertIteratorIsBegin(Iterator& it) {
    REQUIRE(it.is_begin());
    REQUIRE_THROWS_AS(it--, std::out_of_range);
}

template <class Iterator>
void AssertIteratorIsEnd(Iterator& it) {
    REQUIRE(it.is_end());
    REQUIRE_THROWS_AS(it++, std::out_of_range);
    REQUIRE_THROWS_AS(*it, std::out_of_range);
}


TEST_CASE("empty table iteration", "[iterator]") {
    IntIntTable table;
    auto it = table.begin();
    AssertIteratorIsBegin(it);
    it.release();
    it = table.cbegin();
    AssertIteratorIsBegin(it);
    it.release();

    it = table.end();
    AssertIteratorIsEnd(it);
    it.release();
    it = table.cend();
    AssertIteratorIsEnd(it);
}

template <class Iterator>
void AssertIteratorIsReleased(Iterator& it) {
    REQUIRE_THROWS_AS(*it, std::runtime_error);
    REQUIRE_THROWS_AS((void) it->first, std::runtime_error);

    REQUIRE_THROWS_AS(it++, std::runtime_error);
    REQUIRE_THROWS_AS(++it, std::runtime_error);
    REQUIRE_THROWS_AS(it--, std::runtime_error);
    REQUIRE_THROWS_AS(--it, std::runtime_error);

    REQUIRE_THROWS_AS(it.set_value(10), std::runtime_error);
}

TEST_CASE("iterator release", "[iterator]") {
    IntIntTable table;
    table.insert(10, 10);

    SECTION("explicit release") {
        auto it = table.begin();
        it.release();
        AssertIteratorIsReleased(it);
    }

    SECTION("release through destructor") {
        auto it = table.begin();
        it.IntIntTable::iterator::~iterator();
        AssertIteratorIsReleased(it);
        it.release();
        AssertIteratorIsReleased(it);
    }
}

TEST_CASE("iterator walkthrough", "[iterator]") {
    IntIntTable table;
    for (int i = 0; i < 10; ++i) {
        table.insert(i, i);
    }

    SECTION("forward postfix walkthrough") {
        auto it = table.cbegin();
        for (size_t i = 0; i < table.size(); ++i) {
            REQUIRE((*it).first == (*it).second);
            REQUIRE(it->first == it->second);
            it++;
        }
        REQUIRE(it.is_end());
    }

    SECTION("forward prefix walkthrough") {
        auto it = table.cbegin();
        for (size_t i = 0; i < table.size(); ++i) {
            REQUIRE((*it).first == (*it).second);
            REQUIRE(it->first == it->second);
            ++it;
        }
        REQUIRE(it.is_end());
    }

    SECTION("backwards postfix walkthrough") {
        auto it = table.cend();
        for (size_t i = 0; i < table.size(); ++i) {
            it--;
            REQUIRE((*it).first == (*it).second);
            REQUIRE(it->first == it->second);
        }
        REQUIRE(it.is_begin());
    }

    SECTION("backwards prefix walkthrough") {
        auto it = table.cend();
        for (size_t i = 0; i < table.size(); ++i) {
            --it;
            REQUIRE((*it).first == (*it).second);
            REQUIRE(it->first == it->second);
        }
        REQUIRE(it.is_begin());
    }
}

TEST_CASE("iterator modification", "[iterator]") {
    IntIntTable table;
    for (int i = 0; i < 10; ++i) {
        table.insert(i, i);
    }

    for (auto it = table.begin(); !it.is_end(); ++it) {
        it.set_value(it->second + 1);
    }

    auto it = table.cbegin();
    for (size_t i = 0; i < table.size(); ++i) {
        REQUIRE(it->first == it->second - 1);
        ++it;
    }
    REQUIRE(it.is_end());
}


TEST_CASE("empty table snapshot", "[iterator]") {
    cuckoohash_map<std::string, int> table;
    auto snapshot = table.snapshot_table();
    REQUIRE(snapshot.size() == 0);
}

TEST_CASE("filled table snapshot", "[iterator]") {
    cuckoohash_map<std::string, int> table;
    size_t TABLE_SIZE = 10;
    for (size_t i = 0; i < TABLE_SIZE; ++i) {
        table.insert(std::to_string(i), i);
    }

    auto snapshot = table.snapshot_table();
    REQUIRE(snapshot.size() == TABLE_SIZE);

    for (size_t i = 0; i < TABLE_SIZE; ++i) {
        REQUIRE(std::find(snapshot.begin(), snapshot.end(),
                          std::pair<const std::string, int>(
                              std::to_string(i), i))
                != snapshot.end());
    }
}

TEST_CASE("iterator blocks inserts", "[iterator]") {
    IntIntTable table;
    auto it = table.begin();
    std::thread thread([&table] () {
            for (int i = 0; i < 10; ++i) {
                table.insert(i, i);
            }
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(table.size() == 0);
    REQUIRE(it.is_begin());
    REQUIRE(it.is_end());
    it.release();
    thread.join();

    REQUIRE(table.size() == 10);
}
