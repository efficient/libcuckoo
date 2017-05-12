#include <catch.hpp>

#include <cmath>
#include <stdexcept>

#include "unit_test_util.hh"
#include <libcuckoo/cuckoohash_map.hh>

TEST_CASE("default size", "[constructor]") {
  IntIntTable tbl;
  REQUIRE(tbl.size() == 0);
  REQUIRE(tbl.empty());
  if (LIBCUCKOO_DEFAULT_SIZE < 4) {
    REQUIRE(tbl.hashpower() == 0);
  } else {
    REQUIRE(tbl.hashpower() == (size_t)log2(LIBCUCKOO_DEFAULT_SIZE / 4));
  }
  REQUIRE(tbl.bucket_count() == 1UL << tbl.hashpower());
  REQUIRE(tbl.load_factor() == 0);
}

TEST_CASE("given size", "[constructor]") {
  IntIntTable tbl(1);
  REQUIRE(tbl.size() == 0);
  REQUIRE(tbl.empty());
  REQUIRE(tbl.hashpower() == 0);
  REQUIRE(tbl.bucket_count() == 1);
  REQUIRE(tbl.load_factor() == 0);
}

TEST_CASE("frees even with exceptions", "[constructor]") {
  typedef IntIntTableWithAlloc<TrackingAllocator<int, 0>> no_space_table;
  // Should throw when allocating the TableInfo struct
  REQUIRE_THROWS_AS(no_space_table(1), std::bad_alloc);
  REQUIRE(get_unfreed_bytes() == 0);

  typedef IntIntTableWithAlloc<
      TrackingAllocator<int, UnitTestInternalAccess::IntIntBucketSize * 2>>
      some_space_table;
  // Should throw when allocating the counters, after the buckets
  REQUIRE_THROWS_AS(some_space_table(1), std::bad_alloc);
  REQUIRE(get_unfreed_bytes() == 0);
}

TEST_CASE("custom hasher", "[constructor]") {
  auto hashfn = [](size_t) -> size_t { return 0; };
  cuckoohash_map<size_t, size_t, decltype(hashfn)> map(LIBCUCKOO_DEFAULT_SIZE,
                                                       hashfn);
  for (int i = 0; i < 1000; ++i) {
    REQUIRE(map.hash_function()(i) == 0);
  }
}

TEST_CASE("custom equality", "[constructor]") {
  auto eqfn = [](size_t, size_t) -> bool { return false; };
  cuckoohash_map<size_t, size_t, std::hash<size_t>, decltype(eqfn)> map(
      LIBCUCKOO_DEFAULT_SIZE, std::hash<size_t>(), eqfn);

  for (int i = 0; i < 1000; ++i) {
    REQUIRE(map.key_eq()(i, i) == false);
    REQUIRE(map.key_eq()(i, i + 1) == false);
  }
}

template <typename T> struct StatefulAllocator {
  using value_type = T;
  StatefulAllocator() : state(0) {}
  template <typename U>
  StatefulAllocator(const StatefulAllocator<U> &other) : state(other.state) {}

  T *allocate(size_t n) { return std::allocator<T>().allocate(n); }

  void deallocate(T *p, size_t n) { std::allocator<T>().deallocate(p, n); }

  int state;
};

template <typename T, typename U>
bool operator==(const StatefulAllocator<T> &a1,
                const StatefulAllocator<U> &a2) {
  return a1.state == a2.state;
}

template <typename T, typename U>
bool operator!=(const StatefulAllocator<T> &a1,
                const StatefulAllocator<U> &a2) {
  return a1.state != a2.state;
}

TEST_CASE("custom allocator", "[constructor]") {
  typedef std::pair<const int, int> value_type;
  cuckoohash_map<int, int, std::hash<int>, std::equal_to<int>,
                 StatefulAllocator<value_type>>
      tbl;
  REQUIRE(tbl.get_allocator().state == 0);

  StatefulAllocator<value_type> alloc;
  alloc.state = 10;

  cuckoohash_map<int, int, std::hash<int>, std::equal_to<int>,
                 StatefulAllocator<value_type>>
  tbl2(LIBCUCKOO_DEFAULT_SIZE, std::hash<int>(), std::equal_to<int>(), alloc);
  REQUIRE(tbl2.get_allocator().state == 10);
}
