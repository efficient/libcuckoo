// Tests the throughput (queries/sec) of reads and inserts inserts between a
// specific load range in a partially-filled table

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <random>
#include <stdint.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

#include "../../src/cuckoohash_map.hh"
#include "../test_util.hh"
#include "../pcg/pcg_random.hpp"

typedef uint32_t KeyType;
typedef std::string KeyType2;
typedef uint32_t ValType;

// The number of keys to size the table with, expressed as a power of
// 2. This can be set with the command line flag --power
size_t power = 25;
// The initial capacity of the table, expressed as a power of 2. If 0, the table
// is initialized to the number of keys. This can be set with the command line
// flag --table-capacity
size_t table_capacity = 0;
// The number of threads spawned for inserts. This can be set with the
// command line flag --thread-num
size_t thread_num = std::thread::hardware_concurrency();
// The load factor to fill the table up to before testing throughput.
// This can be set with the command line flag --begin-load.
size_t begin_load = 0;
// The maximum load factor to fill the table up to when testing
// throughput. This can be set with the command line flag
// --end-load.
size_t end_load = 90;
// The seed which the random number generator uses. This can be set
// with the command line flag --seed
size_t seed = 0;
// The percentage of operations that should be inserts. This should be
// at least 10. This can be set with the command line flag
// --insert-percent
size_t insert_percent = 10;
// Whether to use strings as the key
bool use_strings = false;

template <class T>
class ReadInsertEnvironment {
    typedef typename T::key_type KType;
public:
    ReadInsertEnvironment()
        : numkeys(1U << power),
          table(table_capacity ? table_capacity : numkeys), keys(numkeys),
	  gen(seed_source) {
        // Sets up the random number generator
        if (seed == 0) {
	    std::cout << "seed = random" << std::endl;
	} else {
	    std::cout << "seed = " << seed << std::endl;
	    gen.seed(seed);
	}

        // We fill the keys array with integers between numkeys and
        // 2*numkeys, shuffled randomly
        keys[0] = numkeys;
        for (size_t i = 1; i < numkeys; i++) {
            const size_t swapind = gen() % i;
            keys[i] = keys[swapind];
            keys[swapind] = generateKey<KType>(i+numkeys);
        }

        // We prefill the table to begin_load with thread_num threads,
        // giving each thread enough keys to insert
        std::vector<std::thread> threads;
        size_t keys_per_thread = numkeys * (begin_load / 100.0) / thread_num;
        for (size_t i = 0; i < thread_num; i++) {
            threads.emplace_back(insert_thread<T>::func, std::ref(table),
                                 keys.begin()+i*keys_per_thread,
                                 keys.begin()+(i+1)*keys_per_thread);
        }
        for (size_t i = 0; i < threads.size(); i++) {
            threads[i].join();
        }

        init_size = table.size();
        ASSERT_TRUE(init_size == keys_per_thread * thread_num);

        std::cout << "Table with capacity " << numkeys <<
            " prefilled to a load factor of " << begin_load << "%" << std::endl;
    }

    size_t numkeys;
    T table;
    std::vector<KType> keys;
    pcg_extras::seed_seq_from<std::random_device> seed_source;
    pcg64_fast gen;
    size_t init_size;
};

template <class T>
void ReadInsertThroughputTest(ReadInsertEnvironment<T> *env) {
    const size_t start_seed = (std::chrono::system_clock::now()
                               .time_since_epoch().count());
    std::atomic<size_t> counter(0);
    std::vector<std::thread> threads;
    size_t keys_per_thread = env->numkeys * ((end_load-begin_load) / 100.0) /
        thread_num;
    timeval t1, t2;
    gettimeofday(&t1, NULL);
    for (size_t i = 0; i < thread_num; i++) {
        threads.emplace_back(
            read_insert_thread<T>::func, std::ref(env->table),
            env->keys.begin()+(i*keys_per_thread)+env->init_size,
            env->keys.begin()+((i+1)*keys_per_thread)+env->init_size,
            std::ref(counter), (double)insert_percent / 100.0, start_seed +i);
    }
    for (size_t i = 0; i < threads.size(); i++) {
        threads[i].join();
    }
    gettimeofday(&t2, NULL);
    double elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000.0; // sec to ms
    elapsed_time += (t2.tv_usec - t1.tv_usec) / 1000.0; // us to ms
    // Reports the results
    std::cout << "----------Results----------" << std::endl;
    std::cout << "Final load factor:\t" << end_load << "%" << std::endl;
    std::cout << "Number of operations:\t" << counter.load() << std::endl;
    std::cout << "Time elapsed:\t" << elapsed_time/1000 << " seconds"
              << std::endl;
    std::cout << "Throughput: " << std::fixed
              << (double)counter.load() / (elapsed_time/1000)
              << " ops/sec" << std::endl;
}

int main(int argc, char** argv) {
    const char* args[] = {"--power", "--table-capacity", "--thread-num",
                          "--begin-load", "--end-load", "--seed",
                          "--insert-percent"};
    size_t* arg_vars[] = {&power, &table_capacity, &thread_num, &begin_load,
                          &end_load, &seed, &insert_percent};
    const char* arg_help[] = {
        "The number of keys to size the table with, expressed as a power of 2",
        "The initial capacity of the table, expressed as a power of 2. "
        "If 0, the table is initialized to the number of keys",
        "The number of threads to spawn for each type of operation",
        "The load factor to fill the table up to before testing throughput",
        "The maximum load factor to fill the table up to when testing "
        "throughput",
        "The seed used by the random number generator",
        "The percentage of operations that should be inserts"
    };
    const char* flags[] = {"--use-strings"};
    bool* flag_vars[] = {&use_strings};
    const char* flag_help[] = {
        "If set, the key type of the map will be std::string"
    };
    parse_flags(argc, argv, "A benchmark for inserts", args, arg_vars, arg_help,
                sizeof(args)/sizeof(const char*), flags, flag_vars, flag_help,
                sizeof(flags)/sizeof(const char*));

    if (begin_load >= 100) {
        std::cerr << "--begin-load must be between 0 and 99" << std::endl;
        exit(1);
    } else if (begin_load >= end_load) {
        std::cerr << "--end-load must be greater than --begin-load"
                  << std::endl;
        exit(1);
    } else if (insert_percent < 1 || insert_percent > 99) {
        std::cerr << "--insert-percent must be between 1 and 99, inclusive"
                  << std::endl;
        exit(1);
    }

    if (use_strings) {
        auto *env = new ReadInsertEnvironment<cuckoohash_map<
                                                  KeyType2, ValType>>;
        ReadInsertThroughputTest(env);
        delete env;
    } else {
        auto *env = new ReadInsertEnvironment<cuckoohash_map<
                                                  KeyType, ValType>>;
        ReadInsertThroughputTest(env);
        delete env;
    }
    return main_return_value;
}
