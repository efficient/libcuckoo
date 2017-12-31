#include <array>

#include <catch.hpp>

#include "unit_test_util.hh"
#include <libcuckoo/cuckoohash_map.hh>

TEST_CASE("rehash empty table", "[resize]") {
  IntIntTable table(1);
  REQUIRE(table.hashpower() == 0);

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
  REQUIRE(table.hashpower() == 0);

  table.reserve(2);
  REQUIRE(table.hashpower() == 0);
}

TEST_CASE("reserve calc", "[resize]") {
  const size_t slot_per_bucket = IntIntTable::slot_per_bucket();
  REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(0) == 0);
  REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
              1 * slot_per_bucket) == 0);

  REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
              2 * slot_per_bucket) == 1);
  REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
              3 * slot_per_bucket) == 2);
  REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
              4 * slot_per_bucket) == 2);
  REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
              2500000 * slot_per_bucket) == 22);

  //for 64+bit platform
  if (sizeof(size_t) >= sizeof(uint64_t)) {
	  REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
		  (static_cast<uint64_t>(1) << 31) * slot_per_bucket) == 31);
	  REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
		  ((static_cast<uint64_t>(1) << 31) + 1) * slot_per_bucket) == 32);

	  REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
		  (static_cast<uint64_t>(1) << 61) * slot_per_bucket) == 61);
	  REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
		  ((static_cast<uint64_t>(1) << 61) + 1) * slot_per_bucket) == 62);
  }
  //for 32bit platform
  else if (sizeof(size_t) == sizeof(uint32_t)) {
	  REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
		  (static_cast<uint32_t>(1) << 15) * slot_per_bucket) == 15);
	  REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
		  ((static_cast<uint32_t>(1) << 15) + 1) * slot_per_bucket) == 16);

	  REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
		  (static_cast<uint32_t>(1) << 29) * slot_per_bucket) == 29);
	  REQUIRE(UnitTestInternalAccess::reserve_calc<IntIntTable>(
		  ((static_cast<uint32_t>(1) << 29) + 1) * slot_per_bucket) == 30);

  }
}

struct my_type {
  int x;
  ~my_type() { ++num_deletes; }
  static size_t num_deletes;
};

size_t my_type::num_deletes = 0;

TEST_CASE("Resizing number of frees", "[resize]") {
  my_type val{0};
  size_t num_deletes_after_resize;
  {
    // Should allocate 2 buckets of 4 slots
    cuckoohash_map<int, my_type, std::hash<int>, std::equal_to<int>,
                   std::allocator<std::pair<const int, my_type>>, 4>
        map(8);
    for (int i = 0; i < 9; ++i) {
      map.insert(i, val);
    }
    // All of the items should be moved during resize to the new region of
    // memory. Then up to 8 of them can be moved to their new bucket.
    REQUIRE(my_type::num_deletes >= 8);
    REQUIRE(my_type::num_deletes <= 16);
    num_deletes_after_resize = my_type::num_deletes;
  }
  REQUIRE(my_type::num_deletes == num_deletes_after_resize + 9);
}

// Taken from https://github.com/facebook/folly/blob/master/folly/docs/Traits.md
class NonRelocatableType {
public:
  std::array<char, 1024> buffer;
  char *pointerToBuffer;
  NonRelocatableType() : pointerToBuffer(buffer.data()) {}
  NonRelocatableType(char c) : pointerToBuffer(buffer.data()) {
    buffer.fill(c);
  }

  NonRelocatableType(const NonRelocatableType &x) noexcept
      : buffer(x.buffer), pointerToBuffer(buffer.data()) {}

  NonRelocatableType &operator=(const NonRelocatableType &x) {
    buffer = x.buffer;
    return *this;
  }
};

TEST_CASE("Resize on non-relocatable type", "[resize]") {
  cuckoohash_map<int, NonRelocatableType, std::hash<int>, std::equal_to<int>,
                 std::allocator<std::pair<const int, NonRelocatableType>>, 1>
      map(0);
  REQUIRE(map.hashpower() == 0);
  // Make it resize a few times to ensure the vector capacity has to actually
  // change when we resize the buckets
  const size_t num_elems = 16;
  for (int i = 0; i < num_elems; ++i) {
    map.insert(i, 'a');
  }
  // Make sure each pointer actually points to its buffer
  NonRelocatableType value;
  std::array<char, 1024> ref;
  ref.fill('a');
  auto lt = map.lock_table();
  for (const auto &kvpair : lt) {
    REQUIRE(ref == kvpair.second.buffer);
    REQUIRE(kvpair.second.pointerToBuffer == kvpair.second.buffer.data());
  }
}
