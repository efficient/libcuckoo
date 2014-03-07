#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

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
    IteratorEnvironment(): emptytable(size), table(size), items_end(items+size) {
        // Fills up table and items with random values
        uint64_t seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::cout << "seed = " << seed << std::endl;
        std::uniform_int_distribution<ValType> v_dist(std::numeric_limits<ValType>::min(), std::numeric_limits<ValType>::max());
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
    std::vector<Table::value_type> snapshot_items = iter_env->table.snapshot_table();
    for (size_t i = 0; i < snapshot_items.size(); i++) {
        if (std::find(iter_env->items, iter_env->items_end, snapshot_items[i]) == iter_env->items_end) {
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
    for (Table::iterator t_mut = static_cast<Table::iterator>(iter_env->table.cbegin());
           !t_mut.is_end(); ++t_mut) {
        t_mut.set_value(t_mut->second+1);
        EXPECT_NE(std::find(iter_env->items, iter_env->items_end, *t_mut), iter_env->items_end);
    }
}

int main(int argc, char** argv) {
    iter_env = new IteratorEnvironment;
    std::cout << "Running EmptyTableBeginEndIterator" << std::endl;
    EmptyTableBeginEndIterator();
    std::cout << "Running FilledTableIterBackwards" << std::endl;
    FilledTableIterBackwards();
    std::cout << "Running FilledTableIterForwards" << std::endl;
    FilledTableIterForwards();
    std::cout << "Running FilledTableIncrementItems" << std::endl;
    FilledTableIncrementItems();
}
