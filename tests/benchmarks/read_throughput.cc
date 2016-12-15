/* Tests the throughput (queries/sec) of only reads for a specific
 * amount of time in a partially-filled table. */

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
#include <numeric>
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
// The number of threads spawned for inserts. This can be set with the
// command line flag --thread-num
size_t thread_num = std::thread::hardware_concurrency();
// The load factor to fill the table up to before testing throughput.
// This can be set with the --load flag
size_t load = 90;
// The seed which the random number generator uses. This can be set
// with the command line flag --seed
size_t seed = 0;
// How many seconds to run the test for. This can be set with the
// command line flag --time
size_t test_len = 10;
// Whether to use strings as the key
bool use_strings = false;

template <class T>
class ReadEnvironment {
    typedef typename T::key_type KType;
public:
    // We allocate the vectors with 2^power keys.
    ReadEnvironment()
        : numkeys(1U<<power), table(numkeys), keys(numkeys), gen(seed_source) {
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

        // We prefill the table to load with thread_num
        // threads, giving each thread enough keys to insert
        std::vector<std::thread> threads;
        size_t keys_per_thread = numkeys * (load / 100.0) / thread_num;
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

        std::cout << "Table with capacity " << numkeys
                  << " prefilled to a load factor of " << load << "%"
                  << std::endl;
    }

    size_t numkeys;
    T table;
    std::vector<KType> keys;
    pcg_extras::seed_seq_from<std::random_device> seed_source;
    pcg64_fast gen;
    size_t init_size;
};

template <class T>
void ReadThroughputTest(ReadEnvironment<T> *env) {
    std::vector<std::thread> threads;
    // A counter for the number of reads
    std::atomic<size_t> counter(0);
    // When set to true, it signals to the threads to stop running
    std::atomic<bool> finished(false);
    // We use the first chunk of the threads to read the init_size elements that
    // are in the table and the others to read the numkeys-init_size elements
    // that aren't in the table. We proportion the number of threads based on
    // the load factor.
    const size_t first_threadnum = thread_num * (load / 100.0);
    const size_t second_threadnum = thread_num - first_threadnum;
    const size_t in_keys_per_thread = (first_threadnum == 0) ?
        0 : env->init_size / first_threadnum;
    const size_t out_keys_per_thread = (env->numkeys - env->init_size) /
        second_threadnum;
    for (size_t i = 0; i < first_threadnum; i++) {
        threads.emplace_back(read_thread<T>::func, std::ref(env->table),
                             env->keys.begin() + (i*in_keys_per_thread),
                             env->keys.begin() + ((i+1)*in_keys_per_thread),
                             std::ref(counter), true, std::ref(finished));
    }
    for (size_t i = 0; i < second_threadnum; i++) {
        threads.emplace_back(
            read_thread<T>::func, std::ref(env->table),
            env->keys.begin() + (i*out_keys_per_thread) + env->init_size,
            env->keys.begin() + (i+1)*out_keys_per_thread + env->init_size,
            std::ref(counter), false, std::ref(finished));
    }
    sleep(test_len);
    finished.store(true, std::memory_order_release);
    for (size_t i = 0; i < threads.size(); i++) {
        threads[i].join();
    }
    // Reports the results
    std::cout << "----------Results----------" << std::endl;
    std::cout << "Number of reads:\t" << counter.load() << std::endl;
    std::cout << "Time elapsed:\t" << test_len << " seconds" << std::endl;
    std::cout << "Throughput: " << std::fixed
              << counter.load() / (double)test_len << " reads/sec" << std::endl;
}

int main(int argc, char** argv) {
    const char* args[] = {"--power", "--thread-num", "--load",
                          "--time", "--seed"};
    size_t* arg_vars[] = {&power, &thread_num, &load, &test_len, &seed};
    const char* arg_help[] = {
        "The number of keys to size the table with, expressed as a power of 2",
        "The number of threads to spawn for each type of operation",
        "The load factor to fill the table up to before testing reads",
        "The number of seconds to run the test for",
        "The seed used by the random number generator"
    };
    const char* flags[] = {"--use-strings"};
    bool* flag_vars[] = {&use_strings};
    const char* flag_help[] = {
        "If set, the key type of the map will be std::string"
    };
    parse_flags(argc, argv, "A benchmark for reads", args, arg_vars,
                arg_help, sizeof(args)/sizeof(const char*), flags,
                flag_vars, flag_help, sizeof(flags)/sizeof(const char*));

    if (use_strings) {
        auto *env = new ReadEnvironment<cuckoohash_map<KeyType2, ValType>>;
        ReadThroughputTest(env);
        delete env;
    } else {
        auto *env = new ReadEnvironment<cuckoohash_map<KeyType, ValType>>;
        ReadThroughputTest(env);
        delete env;
    }
    return main_return_value;
}
