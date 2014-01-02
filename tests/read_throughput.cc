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

#include <libcuckoo/cuckoohash_config.h> // for SLOT_PER_BUCKET
#include <libcuckoo/cuckoohash_map.hh>
#include "test_util.cc"

typedef uint32_t KeyType;
typedef std::string KeyType2;
typedef uint32_t ValType;

// The power argument passed to the hashtable constructor. This can be
// set with the command line flag --power.
size_t power = 22;
// The number of threads spawned for inserts. This can be set with the
// command line flag --thread-num
size_t thread_num = sysconf(_SC_NPROCESSORS_ONLN);
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

/* cacheint is a cache-aligned integer type. */
struct cacheint {
    size_t num;
    cacheint() {
        num = 0;
    }
} __attribute__((aligned(64)));

template <class KType>
struct thread_args {
    typename std::vector<KType>::iterator begin;
    typename std::vector<KType>::iterator end;
    cuckoohash_map<KType, ValType>& table;
    cacheint* reads;
    bool in_table;
    std::atomic<bool>* finished;
};

// Repeatedly searches for the keys in the given range until the time
// is up. All the keys in the given range should either be in the
// table or not in the table.
template <class KType>
void read_thread(thread_args<KType> rt_args) {
    auto begin = rt_args.begin;
    auto end = rt_args.end;
    cuckoohash_map<KType, ValType>& table = rt_args.table;
    auto *reads = rt_args.reads;
    auto in_table = rt_args.in_table;
    std::atomic<bool>* finished = rt_args.finished;
    ValType v;
    while (true) {
        for (auto it = begin; it != end; it++) {
            if (finished->load(std::memory_order_acquire)) {
                return;
            }
            ASSERT_EQ(table.find(*begin, v), in_table);
            reads->num++;
        }
        if (finished->load(std::memory_order_acquire)) {
            return;
        }
    }
}

// Inserts the keys in the given range in a random order, avoiding
// inserting duplicates
template <class KType>
void insert_thread(thread_args<KType> it_args) {
    auto begin = it_args.begin;
    auto end = it_args.end;
    cuckoohash_map<KType, ValType>& table = it_args.table;
    for (;begin != end; begin++) {
        if (table.hashpower() > power) {
            print_lock.lock();
            std::cerr << "Expansion triggered" << std::endl;
            print_lock.unlock();
            exit(1);
        }
        ASSERT_TRUE(table.insert(*begin, 0));
    }
}

template <class KType>
class ReadEnvironment {
public:
    // We allocate the vectors with the total amount of space in the
    // table, which is bucket_count() * SLOT_PER_BUCKET
    ReadEnvironment()
        : table(power), numkeys(table.bucket_count()*SLOT_PER_BUCKET), keys(numkeys) {
        // Sets up the random number generator
        if (seed == 0) {
            seed = std::chrono::system_clock::now().time_since_epoch().count();
        }
        std::cout << "seed = " << seed << std::endl;
        gen.seed(seed);

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
            threads.emplace_back(insert_thread<KType>, thread_args<KType>{keys.begin()+i*keys_per_thread,
                        keys.begin()+(i+1)*keys_per_thread, std::ref(table),
                        nullptr, false, nullptr});
        }
        for (size_t i = 0; i < threads.size(); i++) {
            threads[i].join();
        }

        init_size = table.size();
        ASSERT_TRUE(init_size == keys_per_thread * thread_num);

        std::cout << "Table with capacity " << numkeys << " prefilled to a load factor of " << table.load_factor() << std::endl;
    }

    cuckoohash_map<KType, ValType> table;
    size_t numkeys;
    std::vector<KType> keys;
    std::mt19937_64 gen;
    size_t init_size;
};

template <class KType>
void ReadThroughputTest(ReadEnvironment<KType> *env) {
    std::vector<std::thread> threads;
    std::vector<cacheint> counters(thread_num);
    // We use the first half of the threads to read the init_size
    // elements that are in the table and the other half to read the
    // numkeys-init_size elements that aren't in the table.
    const size_t first_threadnum = thread_num / 2;
    const size_t second_threadnum = thread_num - thread_num / 2;
    const size_t in_keys_per_thread = (first_threadnum == 0) ? 0 : env->init_size / first_threadnum;
    const size_t out_keys_per_thread = (env->numkeys - env->init_size) / second_threadnum;
    // When set to true, it signals to the threads to stop running
    std::atomic<bool> finished(false);
    for (size_t i = 0; i < first_threadnum; i++) {
        threads.emplace_back(read_thread<KType>, thread_args<KType>{env->keys.begin() + (i*in_keys_per_thread),
                    env->keys.begin() + ((i+1)*in_keys_per_thread), std::ref(env->table), &counters[i], true, &finished});
    }
    for (size_t i = 0; i < second_threadnum; i++) {
        threads.emplace_back(read_thread<KType>, thread_args<KType>{env->keys.begin() + (i*out_keys_per_thread) + env->init_size,
                    env->keys.begin() + (i+1)*out_keys_per_thread + env->init_size, std::ref(env->table),
                    &counters[first_threadnum+i], false, &finished});
    }
    sleep(test_len);
    finished.store(true, std::memory_order_release);
    for (size_t i = 0; i < threads.size(); i++) {
        threads[i].join();
    }
    size_t total_reads = 0;
    for (size_t i = 0; i < counters.size(); i++) {
        total_reads += counters[i].num;
    }
    // Reports the results
    std::cout << "----------Results----------" << std::endl;
    std::cout << "Number of reads:\t" << total_reads << std::endl;
    std::cout << "Time elapsed:\t" << test_len << " seconds" << std::endl;
    std::cout << "Throughput: " << std::fixed << total_reads / (double)test_len << " reads/sec" << std::endl;
}

int main(int argc, char** argv) {
    const char* args[] = {"--power", "--thread-num", "--load", "--time", "--seed"};
    size_t* arg_vars[] = {&power, &thread_num, &load, &test_len, &seed};
    const char* arg_help[] = {"The power argument given to the hashtable during initialization",
                              "The number of threads to spawn for each type of operation",
                              "The load factor to fill the table up to before testing reads",
                              "The number of seconds to run the test for",
                              "The seed used by the random number generator"};
    const char* flags[] = {"--use-strings"};
    bool* flag_vars[] = {&use_strings};
    const char* flag_help[] = {"If set, the key type of the map will be std::string"};
    parse_flags(argc, argv, "A benchmark for reads", args, arg_vars,
                arg_help, sizeof(args)/sizeof(const char*), flags,
                flag_vars, flag_help, sizeof(flags)/sizeof(const char*));

    if (use_strings) {
        auto *env = new ReadEnvironment<KeyType2>;
        ReadThroughputTest(env);
        delete env;
    } else {
        auto *env = new ReadEnvironment<KeyType>;
        ReadThroughputTest(env);
        delete env;
    }
}
