#include "acutest.h"

#include <libcuckoo/cuckoohash_map.hh>

#include <atomic>
#include <cstdint>
#include <thread>

void test_lock_array_publication() {
  // Each fresh map grows its lock array repeatedly while a reader selects it.
  // ThreadSanitizer detects the unsynchronized publication in the old code.
  for (int round = 0; round < 200; ++round) {
    libcuckoo::cuckoohash_map<uint64_t, uint64_t> map(1);
    TEST_CHECK(map.insert(0, 42));

    std::atomic<bool> start{false};
    std::atomic<bool> done{false};
    std::atomic<bool> failed{false};
    std::thread reader([&] {
      while (!start.load(std::memory_order_acquire)) {
      }
      uint64_t value = 0;
      while (!done.load(std::memory_order_acquire)) {
        if (!map.find(0, value) || value != 42) {
          failed.store(true, std::memory_order_relaxed);
          break;
        }
      }
    });
    std::thread writer([&] {
      while (!start.load(std::memory_order_acquire)) {
      }
      for (unsigned power = 3; power <= 13; ++power) {
        map.reserve(uint64_t{1} << power);
        std::this_thread::yield();
      }
      done.store(true, std::memory_order_release);
    });
    start.store(true, std::memory_order_release);
    writer.join();
    reader.join();

    TEST_CHECK(!failed.load(std::memory_order_relaxed));
    TEST_CHECK(map.find(0) == 42);
  }
}

TEST_LIST = {
    {"lock_array_publication", test_lock_array_publication},
    {nullptr, nullptr},
};
