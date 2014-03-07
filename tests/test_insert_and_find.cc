#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <utility>

#include <libcuckoo/cuckoohash_map.hh>
#include "test_util.cc"

typedef uint32_t KeyType;
typedef uint32_t ValType;
typedef std::pair<KeyType, ValType> KVPair;

const size_t power = 22;
const size_t numkeys = 1U << power;

class InsertFindEnvironment {
public:
    InsertFindEnvironment() : smalltable(numkeys), bigtable(2*numkeys) {
        // Sets up the random number generator
        uint64_t seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::cout << "seed = " << seed << std::endl;
        std::uniform_int_distribution<ValType> v_dist(std::numeric_limits<ValType>::min(), std::numeric_limits<ValType>::max());
        std::mt19937_64 gen(seed);

        // Inserting elements into the table
        for (size_t i = 0; i < numkeys; i++) {
            keys[i] = i;
            vals[i] = v_dist(gen);
            EXPECT_TRUE(smalltable.insert(keys[i], vals[i]));
            EXPECT_TRUE(bigtable.insert(keys[i], vals[i]));
        }
        // Fills up nonkeys with keys that aren't in the table
        std::uniform_int_distribution<KeyType> k_dist(std::numeric_limits<KeyType>::min(), std::numeric_limits<KeyType>::max());
        for (size_t i = 0; i < numkeys; i++) {
            KeyType k;
            do {
                k = k_dist(gen);
            } while (k < numkeys);
            nonkeys[i] = k;
        }
    }

    cuckoohash_map<KeyType, ValType> smalltable;
    cuckoohash_map<KeyType, ValType> bigtable;
    KeyType keys[numkeys];
    ValType vals[numkeys];
    KeyType nonkeys[numkeys];
};

InsertFindEnvironment* env;

// Makes sure that we can find all the keys with their matching values
// in the small and big tables
void FindKeysInTables() {
    ASSERT_EQ(env->smalltable.size(), numkeys);
    ASSERT_EQ(env->bigtable.size(), numkeys);

    ValType retval;
    for (size_t i = 0; i < numkeys; i++) {
        EXPECT_TRUE(env->smalltable.find(env->keys[i], retval));
        EXPECT_EQ(retval, env->vals[i]);
        EXPECT_TRUE(env->bigtable.find(env->keys[i], retval));
        EXPECT_EQ(retval, env->vals[i]);
    }
}

// Makes sure than none of the nonkeys are in either table
void FindNonkeysInTables() {
    ValType retval;
    for (size_t i = 0; i < numkeys; i++) {
        EXPECT_FALSE(env->smalltable.find(env->nonkeys[i], retval));
        EXPECT_FALSE(env->bigtable.find(env->nonkeys[i], retval));
    }
}

int main(int argc, char **argv) {
    env = new InsertFindEnvironment;
    std::cout << "Running FindKeysInTables" << std::endl;
    FindKeysInTables();
    std::cout << "Running FindNonkeysInTables" << std::endl;
    FindNonkeysInTables();
}
