#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <map>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <utility>

#include <libcuckoo/cuckoohash_map.hh>
#include "test_util.cc"

typedef uint32_t KeyType;
typedef uint32_t ValType;
typedef cuckoohash_map<KeyType, ValType> Table;

const size_t power = 4;
const size_t size = 1L << power;

class IteratorEnvironment {
public:
    IteratorEnvironment()
        : emptytable(size), table(size), items_end(items+size) {
        // Fills up table and items with random values
        uint64_t seed =
            std::chrono::system_clock::now().time_since_epoch().count();
        std::cout << "seed = " << seed << std::endl;
        std::uniform_int_distribution<ValType> v_dist(
            std::numeric_limits<ValType>::min(),
            std::numeric_limits<ValType>::max());
        std::mt19937_64 gen(seed);
        for (size_t i = 0; i < size; i++) {
            items[i].first = i;
            items[i].second = v_dist(gen);
            EXPECT_TRUE(table.insert(items[i].first, items[i].second));
        }
    }

    Table emptytable;
    Table table;
    std::pair<KeyType, ValType> items[size];
    std::pair<KeyType, ValType>* items_end;
};

IteratorEnvironment* iter_env;

void EmptyTableBeginEndIterator() {
    Table emptytable(size);
    Table::const_iterator t = iter_env->emptytable.cbegin();
    ASSERT_TRUE(t.is_begin() && t.is_end());
    t.release();
    t = iter_env->emptytable.cend();
    ASSERT_TRUE(t.is_begin() && t.is_end());
}

bool check_table_snapshot() {
    std::vector<Table::value_type> snapshot_items =
        iter_env->table.snapshot_table();
    for (size_t i = 0; i < snapshot_items.size(); i++) {
        if (std::find(iter_env->items, iter_env->items_end, snapshot_items[i])
            == iter_env->items_end) {
            return false;
        }
    }
    return true;
}

void FilledTableIterForwards() {
    bool visited[size] = {};
    for (auto t = iter_env->table.cbegin(); !t.is_end(); ++t) {
        auto itemiter = std::find(iter_env->items, iter_env->items_end, *t);
        EXPECT_NE(itemiter, iter_env->items_end);
        visited[iter_env->items_end-itemiter-1] = true;
    }
    // Checks that all the items were visited
    for (size_t i = 0; i < size; i++) {
        EXPECT_TRUE(visited[i]);
    }
    EXPECT_TRUE(check_table_snapshot());
}

void FilledTableIterBackwards() {
    Table::const_iterator t = iter_env->table.cend();
    bool visited[size] = {};
    do {
        t--;
        auto itemiter = std::find(iter_env->items, iter_env->items_end, *t);
        EXPECT_NE(itemiter, iter_env->items_end);
        visited[iter_env->items_end-itemiter-1] = true;
    } while (!t.is_begin());
    // Checks that all the items were visited
    for (size_t i = 0; i < size; i++) {
        EXPECT_TRUE(visited[i]);
    }
    t.release();
    EXPECT_TRUE(check_table_snapshot());
}

void FilledTableIncrementItems() {
    for (size_t i = 0; i < size; i++) {
        iter_env->items[i].second++;
    }
    // Also tests casting from a const iterator to a mutable one
    for (auto t_mut = static_cast<Table::iterator>(iter_env->table.cbegin());
           !t_mut.is_end(); ++t_mut) {
        t_mut.set_value(t_mut->second+1);
        EXPECT_NE(std::find(iter_env->items, iter_env->items_end, *t_mut),
                  iter_env->items_end);
    }
}

ValType ConstRead(const Table& t, KeyType key) {
  return t[key];
}

void IndexOperatorTest() {
  uint64_t seed =
      std::chrono::system_clock::now().time_since_epoch().count();
  std::cout << "seed = " << seed << std::endl;
  std::uniform_int_distribution<ValType> v_dist(
      std::numeric_limits<ValType>::min(),
      std::numeric_limits<ValType>::max());
  std::mt19937_64 gen(seed);
  std::map<KeyType, ValType> reference_map;
  Table cuckoo_map;
  Table cuckoo_map2;
  // loop over keys in [0, size) a few times verify that insertions
  // into the cuckoo map match insertions into the reference map
  for (size_t i = 0; i < size * size; i++) {
    KeyType index = i % size;
    ValType val = v_dist(gen);
    // store
    reference_map[index] = val;
    cuckoo_map[index] = val;

    // load
    ValType cuckoo_read = cuckoo_map[index];
    EXPECT_EQ(reference_map[index], cuckoo_read);
    // const load
    cuckoo_read = ConstRead(cuckoo_map, index);
    EXPECT_EQ(reference_map[index], cuckoo_read);

    // store from reference
    cuckoo_map2[index] = (ValType)cuckoo_map[index]; 
    EXPECT_EQ((ValType)cuckoo_map[index], (ValType)cuckoo_map2[index]);

    // aliased store
    cuckoo_map2[index] = (ValType)cuckoo_map2[index];
    EXPECT_EQ((ValType)cuckoo_map[index], (ValType)cuckoo_map2[index]);
  }
}

int main() {
    iter_env = new IteratorEnvironment;
    std::cout << "Running EmptyTableBeginEndIterator" << std::endl;
    EmptyTableBeginEndIterator();
    std::cout << "Running FilledTableIterBackwards" << std::endl;
    FilledTableIterBackwards();
    std::cout << "Running FilledTableIterForwards" << std::endl;
    FilledTableIterForwards();
    std::cout << "Running FilledTableIncrementItems" << std::endl;
    FilledTableIncrementItems();
    std::cout << "Running IndexOperatorTest" << std::endl;
    IndexOperatorTest();
}
