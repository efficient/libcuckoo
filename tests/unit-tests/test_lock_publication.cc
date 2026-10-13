#include "acutest.h"
#include "unit_test_util.hh"

#include <atomic>
#include <thread>
#include <vector>

namespace {

// Only the main thread uses acutest assertions. Workers report through atomics.
void concurrent_growth(bool manual_resize) {
  const int writers = 4;
  const int per_writer = 8192;
  const int stable_keys = 16;
  const int updates = 512;
  for (int round = 0; round < 4; ++round) {
    IntIntTable map(1);
    // Explicit reserve can make the table sparse while an insert needs to grow.
    if (manual_resize) {
      map.minimum_load_factor(0.0);
    }
    for (int key = -stable_keys; key < 0; ++key) {
      TEST_CHECK(map.insert(key, 0));
    }
    std::atomic<bool> start(false);
    std::atomic<int> finished(0), failures(0);
    std::atomic<int> published[writers];
    for (auto &count : published) {
      count.store(0);
    }
    std::vector<std::thread> threads;
    const auto await_start = [&] {
      while (!start.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }
    };
    for (int writer = 0; writer < writers; ++writer) {
      threads.emplace_back([&, writer] {
        await_start();
        try {
          for (int i = 0; i < per_writer; ++i) {
            const int key = writer * per_writer + i;
            if (!map.insert(key, key * 3 + 1)) {
              ++failures;
            }
            published[writer].store(i + 1, std::memory_order_release);
          }
        } catch (...) {
          ++failures;
        }
        ++finished;
      });
    }
    threads.emplace_back([&] {
      await_start();
      try {
        for (int i = 0; i < updates; ++i) {
          for (int key = -stable_keys; key < 0; ++key) {
            if (!map.update_fn(key, [](int &value) { ++value; })) {
              ++failures;
            }
          }
        }
      } catch (...) {
        ++failures;
      }
    });
    threads.emplace_back([&] {
      await_start();
      try {
        do {
          for (int writer = 0; writer < writers; ++writer) {
            const int count = published[writer].load(std::memory_order_acquire);
            if (count > 0) {
              const int key = writer * per_writer + count - 1;
              int value = 0;
              if (!map.find(key, value) || value != key * 3 + 1) {
                ++failures;
              }
            }
          }
        } while (finished.load() != writers);
      } catch (...) {
        ++failures;
      }
    });
    threads.emplace_back([&] {
      await_start();
      try {
        do {
          {
            auto table = map.lock_table();
            for (const auto &entry : table) {
              if ((entry.first >= 0 && entry.second != entry.first * 3 + 1) ||
                  (entry.first < 0 &&
                   (entry.second < 0 || entry.second > updates))) {
                ++failures;
              }
            }
          }
          std::this_thread::yield();
        } while (finished.load() != writers);
      } catch (...) {
        ++failures;
      }
    });
    if (manual_resize) {
      threads.emplace_back([&] {
        await_start();
        try {
          for (int i = 0; i < 8; ++i) {
            map.reserve(65536);
            map.rehash(8);
          }
        } catch (...) {
          ++failures;
        }
      });
    }
    start.store(true, std::memory_order_release);
    for (auto &thread : threads) {
      thread.join();
    }
    TEST_CHECK(failures.load() == 0);
    TEST_CHECK(map.size() == size_t(writers * per_writer + stable_keys));
    for (int key = 0; key < writers * per_writer; ++key) {
      int value = 0;
      TEST_CHECK(map.find(key, value));
      TEST_CHECK(value == key * 3 + 1);
    }
    for (int key = -stable_keys; key < 0; ++key) {
      TEST_CHECK(map.find(key) == updates);
    }
  }
}

void test_concurrent_automatic_growth() { concurrent_growth(false); }
void test_concurrent_reserve_rehash() { concurrent_growth(true); }

void test_retained_lock_array() {
  IntIntTable map(8);
  TEST_CHECK(map.insert(2, 35));
  auto &old_locks = libcuckoo::UnitTestInternalAccess::get_current_locks(map);
  const auto old_size = old_locks.size();
  std::thread resizer([&] {
    map.reserve(4096);
    map.rehash(1);
    map.reserve(16384);
  });
  resizer.join();
  // A reader can retain this reference before a resize, then acquire an old
  // lock and retry. The old array must remain alive and unlocked.
  TEST_CHECK(old_locks.size() == old_size);
  for (auto &lock : old_locks) {
    const bool acquired = lock.try_lock();
    TEST_CHECK(acquired);
    if (acquired) {
      lock.unlock();
    }
  }
  TEST_CHECK(map.find(2) == 35);
}

void grow_and_check(IntIntTable &map) {
  {
    auto table = map.lock_table();
    table.reserve(map.capacity() * 2);
    TEST_CHECK(table.size() == 100);
  }
  for (int key = 0; key < 100; ++key) {
    TEST_CHECK(map.find(key) == key + 1);
  }
}

void test_lock_ownership_after_copy_move_swap() {
  IntIntTable copied(1), moved(1);
  {
    IntIntTable source(1);
    for (int key = 0; key < 100; ++key) {
      TEST_CHECK(source.insert(key, key + 1));
    }
    // Retain several arrays, including arrays added while lock_table owns them.
    grow_and_check(source);
    IntIntTable copy(source);
    IntIntTable move(std::move(copy));
    copied = source;
    moved = std::move(move);
    copied.swap(moved);
  }
  // Destroy the source before using or growing either destination. Incorrect
  // publication/successor pointers would access the source's freed arrays.
  grow_and_check(copied);
  grow_and_check(moved);
}

void test_growth_past_lock_limit() {
  IntIntTable map(1);
  const int count = 300000; // Forces automatic growth beyond 2^16 buckets.
  for (int key = 0; key < count; ++key) {
    TEST_CHECK(map.insert(key, key + 1));
  }
  TEST_CHECK(map.hashpower() > 16);
  TEST_CHECK(map.size() == size_t(count));
  for (int key = 0; key < count; ++key) {
    TEST_CHECK(map.find(key) == key + 1);
  }
  {
    auto table = map.lock_table();
    size_t visited = 0;
    for (const auto &entry : table) {
      TEST_CHECK(entry.second == entry.first + 1);
      ++visited;
    }
    TEST_CHECK(visited == size_t(count));
  }
}

int allocations_before_failure = -1;

template <class T> struct FailingAllocator {
  using value_type = T;
  FailingAllocator() = default;
  template <class U> FailingAllocator(const FailingAllocator<U> &) {}
  T *allocate(size_t n) {
    if (allocations_before_failure == 0) {
      throw std::bad_alloc();
    }
    if (allocations_before_failure > 0) {
      --allocations_before_failure;
    }
    return std::allocator<T>().allocate(n);
  }
  void deallocate(T *p, size_t n) { std::allocator<T>().deallocate(p, n); }
  template <class U> bool operator==(const FailingAllocator<U> &) const {
    return true;
  }
  template <class U> bool operator!=(const FailingAllocator<U> &) const {
    return false;
  }
};

void test_failed_lock_array_allocation() {
  using Map =
      libcuckoo::cuckoohash_map<int, int, std::hash<int>, std::equal_to<int>,
                                FailingAllocator<std::pair<const int, int>>>;
  // Automatic growth allocates the new lock vector, then its owning list node.
  for (int allocation = 0; allocation < 2; ++allocation) {
    Map map(8);
    for (int key = 0; key < 8; ++key) {
      TEST_CHECK(map.insert(key, key + 7));
    }
    allocations_before_failure = allocation;
    bool failed = false;
    try {
      map.insert(8, 15);
    } catch (const std::bad_alloc &) {
      failed = true;
    }
    allocations_before_failure = -1;
    TEST_CHECK(failed);
    TEST_CHECK(map.size() == 8);
    for (int key = 0; key < 8; ++key) {
      TEST_CHECK(map.find(key) == key + 7);
    }
    TEST_CHECK(map.insert(8, 15));
    TEST_CHECK(map.find(8) == 15);
  }
}

} // namespace

TEST_LIST = {
    {"concurrent_automatic_growth", test_concurrent_automatic_growth},
    {"concurrent_reserve_rehash", test_concurrent_reserve_rehash},
    {"retained_lock_array", test_retained_lock_array},
    {"lock_ownership_after_copy_move_swap",
     test_lock_ownership_after_copy_move_swap},
    {"growth_past_lock_limit", test_growth_past_lock_limit},
    {"failed_lock_array_allocation", test_failed_lock_array_allocation},
    {nullptr, nullptr},
};
