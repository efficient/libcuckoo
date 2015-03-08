#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <thread>
#include <utility>

#include <libcuckoo/cuckoohash_map.hh>
#include "test_util.cc"

typedef uint32_t Type;
typedef cuckoohash_map<Type, Type> Table;

const size_t thread_num = sysconf(_SC_NPROCESSORS_ONLN);
const size_t power = 10;
const size_t size = 1L << power;

Type key_to_val(Type key) {
    return key+1;
}

// Inserts key-value pairs of the form (i, key_to_val(i)) from i = start to end
// in the given table.
void inserter(Table& table, Type start, Type end) {
    for (Type t = start; t < end; ++t) {
        table[t] = key_to_val(t);
    }
}

// Copies the key-value pairs from key start to end from t1 to t2
void copier(Table& t1, Table& t2, Type start, Type end) {
    for (Type t = start; t < end; ++t) {
        t2[t] = t1[t];
    }
}

// Increments the key-value pairs from key start to end
void incrementer(Table& table, Type start, Type end) {
    for (Type t = start; t < end; ++t) {
        table[t] = table[t] + 1;
    }
}

void IndexOperatorTest() {
  Table cuckoo_map;
  Table cuckoo_map2;

  size_t keys_per_thread = size / thread_num;
  std::vector<std::thread> threads;
  // Concurrently insert items into cuckoo_map
  for (size_t i = 0; i < thread_num; ++i) {
      threads.emplace_back(inserter, std::ref(cuckoo_map),
                           i * keys_per_thread, (i+1) * keys_per_thread);
  }
  for (auto& t : threads) {
      t.join();
  }
  // Check that the values are correct
  for (Type t = 0; t < thread_num * keys_per_thread; ++t) {
      EXPECT_EQ(cuckoo_map[t], key_to_val(t));
  }

  // Concurrently copy from cuckoo_map to cuckoo_map2
  threads.clear();
  for (size_t i = 0; i < thread_num; ++i) {
      threads.emplace_back(copier, std::ref(cuckoo_map), std::ref(cuckoo_map2),
                           i * keys_per_thread, (i+1) * keys_per_thread);
  }
  for (auto& t : threads) {
      t.join();
  }
  // Check that the values are correct
  for (Type t = 0; t < thread_num * keys_per_thread; ++t) {
      EXPECT_EQ(cuckoo_map2[t], key_to_val(t));
  }


  // Concurrently increment the items in cuckoo_map2
  threads.clear();
  for (size_t i = 0; i < thread_num; ++i) {
      threads.emplace_back(incrementer, std::ref(cuckoo_map2),
                           i * keys_per_thread, (i+1) * keys_per_thread);
  }
  for (auto& t : threads) {
      t.join();
  }
  // Check that the values are correct
  for (Type t = 0; t < thread_num * keys_per_thread; ++t) {
      EXPECT_EQ(cuckoo_map2[t], key_to_val(t)+1);
  }
}

int main() {
    std::cout << "Running IndexOperatorTest" << std::endl;
    IndexOperatorTest();
    return main_return_value;
}
