#include "catch.hpp"

#include <string>
#include <utility>

#include "../../src/cuckoohash_map.hh"
#include "unit_test_util.hh"

typedef std::unique_ptr<int> uptr;
struct uptr_hash {
    size_t operator()(const uptr& ptr) const {
        return (*ptr) * 0xc6a4a7935bd1e995;
    }
};

struct uptr_eq {
    bool operator()(const uptr& ptr1, const uptr& ptr2) const {
        return *ptr1 == *ptr2;
    }
};

typedef cuckoohash_map<uptr, uptr, uptr_hash, uptr_eq> uptr_tbl;

const size_t TBL_INIT = 1;
const size_t TBL_SIZE = TBL_INIT * uptr_tbl::slot_per_bucket * 2;

void check_key_eq(uptr_tbl& tbl, int key, int expected_val) {
    REQUIRE(tbl.contains(uptr(new int(key))));
    tbl.update_fn(uptr(new int(key)), [expected_val](const uptr& ptr) {
            REQUIRE(*ptr == expected_val);
        });
}

TEST_CASE("noncopyable insert and update", "[noncopyable]") {
    uptr_tbl tbl(TBL_INIT);
    for (size_t i = 0; i < TBL_SIZE; ++i) {
        tbl.insert(uptr(new int(i)), uptr(new int(i)));
    }
    for (size_t i = 0; i < TBL_SIZE; ++i) {
        check_key_eq(tbl, i, i);
    }
    for (size_t i = 0; i < TBL_SIZE; ++i) {
        tbl.update(uptr(new int(i)), uptr(new int(i+1)));
    }
    for (size_t i = 0; i < TBL_SIZE; ++i) {
        check_key_eq(tbl, i, i+1);
    }
}

TEST_CASE("noncopyable upsert", "[noncopyable]") {
    uptr_tbl tbl(TBL_INIT);
    auto increment = [](uptr& ptr) {
        *ptr += 1;
    };
    for (size_t i = 0; i < TBL_SIZE; ++i) {
        tbl.upsert(uptr(new int(i)), increment, uptr(new int(i)));
    }
    for (size_t i = 0; i < TBL_SIZE; ++i) {
        check_key_eq(tbl, i, i);
    }
    for (size_t i = 0; i < TBL_SIZE; ++i) {
        tbl.upsert(uptr(new int(i)), increment, uptr(new int(i)));
    }
    for (size_t i = 0; i < TBL_SIZE; ++i) {
        check_key_eq(tbl, i, i+1);
    }
}

TEST_CASE("noncopyable iteration", "[noncopyable]") {
    uptr_tbl tbl(TBL_INIT);
    for (size_t i = 0; i < TBL_SIZE; ++i) {
        tbl.insert(uptr(new int(i)), uptr(new int(i)));
    }
    {
        auto locked_tbl = tbl.lock_table();
        for (auto& kv : locked_tbl) {
            REQUIRE(*kv.first == *kv.second);
            *kv.second += 1;
        }
    }
    {
        auto locked_tbl = tbl.lock_table();
        for (auto& kv : locked_tbl) {
            REQUIRE(*kv.first == *kv.second - 1);
        }
    }
}

TEST_CASE("nested table", "[noncopyable]") {
    typedef cuckoohash_map<char, std::string> inner_tbl;
    typedef cuckoohash_map<std::string, std::unique_ptr<inner_tbl>> nested_tbl;
    nested_tbl tbl;
    std::string keys[] = {"abc", "def"};
    for (std::string& k : keys) {
        tbl.insert(std::string(k), nested_tbl::mapped_type(new inner_tbl));
        tbl.update_fn(k, [&k](nested_tbl::mapped_type& t) {
                for (char c : k) {
                    t->insert(c, std::string(k));
                }
            });
    }
    for (std::string& k : keys) {
        REQUIRE(tbl.contains(k));
        tbl.update_fn(k, [&k](nested_tbl::mapped_type& t) {
                for (char c : k) {
                    REQUIRE(t->find(c) == k);
                }
            });
    }
}
