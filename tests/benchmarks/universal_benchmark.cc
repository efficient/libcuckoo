/* Benchmarks a mix of operations for a compile-time specified key-value pair */

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "../../src/cuckoohash_map.hh"
#include "../test_util.hh"

#include "universal_gen.hh"

/* Compile-time parameters -- key and value type */

#ifndef KEY
#error Must define KEY symbol as valid key type
#endif

#ifndef VALUE
#error Must define VALUE symbol as valid value type
#endif

/* Run-time parameters -- operation mix and table configuration */

// The following specify what percentage of operations should be of each type.
// They must add up to 100, but by default are all 0.
size_t g_read_percentage = 0;
size_t g_insert_percentage = 0;
size_t g_erase_percentage = 0;
size_t g_update_percentage = 0;
size_t g_upsert_percentage = 0;

// The initial capacity of the table, specified as a power of 2.
size_t g_initial_capacity = 25;
// The percentage of the initial table capacity should we fill the table to
// before running the benchmark.
size_t g_prefill_percentage = 0;
// Total number of operations we are running, specified as a percentage of the
// initial capacity. This can exceed 100.
size_t g_total_ops_percentage = 90;

// Number of threads to run with
size_t g_threads = std::thread::hardware_concurrency();

const char* args[] = {
    "--reads",
    "--inserts",
    "--erases",
    "--updates",
    "--upserts",
    "--initial-capacity",
    "--prefill",
    "--total-ops",
    "--num-threads",
};

size_t* arg_vars[] = {
    &g_read_percentage,
    &g_insert_percentage,
    &g_erase_percentage,
    &g_update_percentage,
    &g_upsert_percentage,
    &g_initial_capacity,
    &g_prefill_percentage,
    &g_total_ops_percentage,
    &g_threads,
};

const char* arg_descriptions[] = {
    "Percentage of mix that is reads",
    "Percentage of mix that is inserts",
    "Percentage of mix that is erases",
    "Percentage of mix that is updates",
    "Percentage of mix that is upserts",
    "Initial capacity of table, as a power of 2",
    "Percentage of final size to pre-fill table",
    "Number of operations, as a percentage of the initial capacity. This can exceed 100",
    "Number of threads",
};

#define XSTR(s) STR(s)
#define STR(s) #s

const char* description = "A benchmark that can run an arbitrary mixture of "
    "table operations.\nThe sum of read, insert, erase, update, and upsert "
    "percentages must be 100.\nMap type is cuckoohash_map<" XSTR(KEY) ", " XSTR(VALUE) ">.";

/* A wrapper for all operations being benchmarked that can be specialized for
 * different tables. This un-specialized template implementation lists out all
 * the methods necessary to implement. Each method is static and returns true on
 * successful action and false on failure. */
template <typename T>
class BenchmarkWrapper {
    // bool read(T& tbl, const KEY& k)
    // bool insert(const T& tbl, const KEY& k, const VALUE& v)
    // bool erase(T& tbl, const KEY& k)
    // bool update(T& tbl, const KEY& k, const VALUE& v)
    // bool upsert(T& tbl, const KEY& k, Updater fn, const VALUE& v)
};

template <>
class BenchmarkWrapper<cuckoohash_map<KEY, VALUE> > {
public:
    typedef cuckoohash_map<KEY, VALUE> tbl;
    static bool read(const tbl& tbl, const KEY& k) {
        static VALUE v;
        return tbl.find(k, v);
    }

    static bool insert(tbl& tbl, const KEY& k, const VALUE& v) {
        return tbl.insert(k, v);
    }

    static bool erase(tbl& tbl, const KEY& k) {
        return tbl.erase(k);
    }

    static bool update(tbl& tbl, const KEY& k, const VALUE& v) {
        return tbl.update(k, v);
    }

    template <typename Updater>
    static bool upsert(tbl& tbl, const KEY& k, Updater fn, const VALUE& v) {
        tbl.upsert(k, fn, v);
        return true;
    }
};


void check_percentage(size_t value, const char* name) {
    if (value > 100) {
        std::string msg("Percentage for `");
        msg += name;
        msg += "` cannot exceed 100";
        throw std::runtime_error(msg.c_str());
    }
}

enum Ops {
    READ,
    INSERT,
    ERASE,
    UPDATE,
    UPSERT,
};

template <typename T>
void prefill_thread(const thread_id_t thread_id,
                    T& tbl,
                    const seq_t prefill_elems) {
    for (seq_t i = 0; i < prefill_elems; ++i) {
        ASSERT_TRUE(BenchmarkWrapper<T>::insert(
                        tbl, Gen<KEY>::key(i, thread_id, g_threads),
                        Gen<VALUE>::value()));
    }
}

template <typename T>
void mix_thread(const thread_id_t thread_id,
                T& tbl,
                const seq_t num_ops,
                const std::array<Ops, 100>& op_mix,
                const seq_t prefill_elems) {
    // Invariant: erase_seq <= insert_seq
    seq_t erase_seq = 0;
    seq_t insert_seq = prefill_elems;
    // These variables can be overwritten in the loop iterations, they're
    // declared outside so that we don't initialize new variables in the loop;
    uint64_t x;
    seq_t seq;
    // Run the operation mix for num_ops operations
    for (size_t i = 0; i < num_ops;) {
        for (size_t j = 0; j < 100 && i < num_ops; ++i, ++j) {
            // A large mixed-bits number based on i and j, using constants from
            // MurmurHash
            x = (i * 0x5bd1e995) + (j * 0xc6a4a7935bd1e995);
            switch (op_mix[j]) {
            case READ:
                // Read sequence number `x % num_ops`. If it's in the range
                // [erase_seq, insert_seq), then it's in the table. Assuming `x`
                // is large and `num_ops` is not related to `x` numerically,
                // this should produce a decent mix of numbers across the range
                // [0, `num_ops`], which should approximate the capacity of the
                // table.
                seq = x % num_ops;
                ASSERT_EQ(
                    seq >= erase_seq && seq < insert_seq,
                    BenchmarkWrapper<T>::read(
                        tbl, Gen<KEY>::key(seq, thread_id, g_threads)));
                break;
            case INSERT:
                // Insert sequence number `insert_seq`. This should always
                // succeed and be inserting a new value.
                ASSERT_TRUE(BenchmarkWrapper<T>::insert(
                                tbl, Gen<KEY>::key(insert_seq, thread_id, g_threads),
                                Gen<VALUE>::value()));
                ++insert_seq;
                break;
            case ERASE:
                // Erase sequence number `erase_seq`. Should succeed if
                // `erase_seq < insert_seq`. Since we only increment `erase_seq`
                // if it is less than `insert_seq`, this will keep erasing the
                // same element if we have not inserted anything in a while, but
                // a good mix probably shouldn't be doing that.
                ASSERT_EQ(erase_seq < insert_seq,
                          BenchmarkWrapper<T>::erase(
                              tbl, Gen<KEY>::key(erase_seq, thread_id, g_threads)));
                if (erase_seq < insert_seq) {
                    ++erase_seq;
                }
                break;
            case UPDATE:
                // Same as find, except we update to the same default value
                seq = x % num_ops;
                ASSERT_EQ(
                    seq >= erase_seq && seq < insert_seq,
                    BenchmarkWrapper<T>::update(
                        tbl, Gen<KEY>::key(seq, thread_id, g_threads),
                        Gen<VALUE>::value()));
                break;
            case UPSERT:
                // If insert_seq == 0 or x & 1 == 0, then do an insert,
                // otherwise, update insert_seq - 1. This should obtain an even
                // balance of inserts and updates across a changing set of
                // numbers, regardless of the mix.
                if ((x & 1) == 0 || insert_seq == 0) {
                    ASSERT_TRUE(BenchmarkWrapper<T>::insert(
                                    tbl, Gen<KEY>::key(insert_seq, thread_id, g_threads),
                                    Gen<VALUE>::value()));
                    ++insert_seq;
                } else {
                    ASSERT_TRUE(
                        BenchmarkWrapper<T>::update(
                            tbl, Gen<KEY>::key(insert_seq - 1, thread_id, g_threads),
                            Gen<VALUE>::value()));
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    // Parse parameters and check them.
    parse_flags(argc, argv, description, args, arg_vars, arg_descriptions,
                sizeof(args)/sizeof(const char*), nullptr, nullptr, nullptr, 0);
    check_percentage(g_read_percentage, "reads");
    check_percentage(g_insert_percentage, "inserts");
    check_percentage(g_erase_percentage, "erases");
    check_percentage(g_update_percentage, "updates");
    check_percentage(g_upsert_percentage, "upserts");
    check_percentage(g_prefill_percentage, "prefill");
    if (g_read_percentage + g_insert_percentage + g_erase_percentage +
        g_update_percentage + g_upsert_percentage != 100) {
        throw std::runtime_error("Operation mix percentages must sum to 100");
    }

    const size_t initial_capacity = 1UL << g_initial_capacity;
    const size_t total_ops = initial_capacity * g_total_ops_percentage / 100;

    // Create and size the table
    cuckoohash_map<KEY, VALUE> tbl(initial_capacity);

    // Pre-generate an operation mix based on our percentages.
    std::array<Ops, 100> op_mix;
    auto *op_mix_p = &op_mix[0];
    for (size_t i = 0; i < g_read_percentage; ++i) {
        *op_mix_p++ = READ;
    }
    for (size_t i = 0; i < g_insert_percentage; ++i) {
        *op_mix_p++ = INSERT;
    }
    for (size_t i = 0; i < g_erase_percentage; ++i) {
        *op_mix_p++ = ERASE;
    }
    for (size_t i = 0; i < g_update_percentage; ++i) {
        *op_mix_p++ = UPDATE;
    }
    for (size_t i = 0; i < g_upsert_percentage; ++i) {
        *op_mix_p++ = UPSERT;
    }
    std::shuffle(op_mix.begin(), op_mix.end(),
                 std::mt19937(std::random_device()()));

    // Pre-fill the table
    const size_t prefill_elems = (initial_capacity * g_prefill_percentage / 100);
    std::vector<std::thread> prefill_threads(g_threads);
    for (size_t i = 0; i < g_threads; ++i) {
        size_t thread_prefill = prefill_elems / g_threads;
        if (i == g_threads - 1) {
            thread_prefill += prefill_elems % g_threads;
        }
        prefill_threads[i] = std::thread(
            prefill_thread<decltype(tbl)>,
            i,
            std::ref(tbl),
            thread_prefill);
    }
    for (std::thread& t : prefill_threads) {
        t.join();
    }

    // Run the operation mix, timed
    std::vector<std::thread> mix_threads(g_threads);
    const size_t initial_hashpower = tbl.hashpower();
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < g_threads; ++i) {
        size_t thread_prefill = prefill_elems / g_threads;
        size_t thread_ops = total_ops / g_threads;
        if (i == g_threads - 1) {
            thread_prefill += prefill_elems % g_threads;
            thread_ops += total_ops % g_threads;
        }
        mix_threads[i] = std::thread(
            mix_thread<decltype(tbl)>,
            i,
            std::ref(tbl),
            thread_ops,
            std::ref(op_mix),
            thread_prefill);
    }
    for (std::thread& t : mix_threads) {
        t.join();
    }
    auto end = std::chrono::high_resolution_clock::now();
    const size_t final_hashpower = tbl.hashpower();
    double seconds_elapsed = std::chrono::duration_cast<
        std::chrono::duration<double> >(end - start).count();
    std::cout << std::fixed;
    std::cout << "total ops: " << total_ops << std::endl;
    std::cout << "time elapsed (sec): " << seconds_elapsed << std::endl;
    std::cout << "number of expansions: " << final_hashpower - initial_hashpower
              << std::endl;
    std::cout << "throughput (ops/sec): "
              << total_ops / seconds_elapsed << std::endl;
}
