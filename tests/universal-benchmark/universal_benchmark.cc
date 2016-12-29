/* Benchmarks a mix of operations for a compile-time specified key-value pair */

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <test_util.hh>
#include <pcg/pcg_random.hpp>

#include "universal_gen.hh"
#include "universal_table_wrapper.hh"

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

// Seed for random number generator. If left at the default (0), we'll generate
// a random seed.
size_t g_seed = 0;

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
    "--seed",
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
    &g_seed,
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
    "Seed for random number generator",
};

#define XSTR(s) STR(s)
#define STR(s) #s

const char* description = "A benchmark that can run an arbitrary mixture of "
    "table operations.\nThe sum of read, insert, erase, update, and upsert "
    "percentages must be 100.\nMap type is " TABLE_TYPE "<" XSTR(KEY)
    ", " XSTR(VALUE) ">."
    ;

void check_percentage(size_t value, const char* name) {
    if (value > 100) {
        std::string msg("Percentage for `");
        msg += name;
        msg += "` cannot exceed 100\n";
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

void genkeys_thread(std::vector<uint64_t>& keys,
                    const size_t gen_elems,
                    const size_t base_seed,
                    const size_t thread_id) {
    keys.resize(gen_elems);
    pcg64 rng(base_seed, thread_id);
    for (uint64_t& num : keys) {
        num = rng();
    }
}

void prefill_thread(Table& tbl,
                    const std::vector<uint64_t>& keys,
                    const size_t prefill_elems) {
    for (size_t i = 0; i < prefill_elems; ++i) {
        ASSERT_TRUE(tbl.insert(Gen<KEY>::key(keys[i]),
                               Gen<VALUE>::value()));
    }
}

void mix_thread(Table& tbl,
                const size_t num_ops,
                const std::array<Ops, 100>& op_mix,
                const std::vector<uint64_t>& keys,
                const size_t prefill_elems) {
    // Invariant: erase_seq <= insert_seq
    // Invariant: insert_seq < numkeys (enforced with bounds checking)
    const size_t numkeys = keys.size();
    size_t erase_seq = 0;
    size_t insert_seq = prefill_elems;
    // This rng is used to get random indices in the keys array. The actual
    // numbers don't matter too much so we just seed it with the global seed.
    pcg64_fast rng(g_seed);
    // These variables are initialized out here so we don't create new variables
    // in the switch statement.
    size_t n;
    VALUE v;
    // A convenience function for getting the nth key
    auto key = [&keys](size_t n) {
        return keys.at(n);
    };

    // The upsert function is just the identity
    auto upsert_fn = [](VALUE& v) { return; };

    // Run the operation mix for num_ops operations
    for (size_t i = 0; i < num_ops;) {
        for (size_t j = 0; j < 100 && i < num_ops; ++i, ++j) {
            switch (op_mix[j]) {
            case READ:
                // If `n` is between `erase_seq` and `insert_seq`, then it
                // should be in the table.
                n = rng(numkeys);
                ASSERT_EQ(
                    n >= erase_seq && n < insert_seq,
                    tbl.read(key(n), v));
                break;
            case INSERT:
                // Insert sequence number `insert_seq`. This should always
                // succeed and be inserting a new value.
                ASSERT_TRUE(tbl.insert(key(insert_seq++), Gen<VALUE>::value()));
                break;
            case ERASE:
                // If `erase_seq` == `insert_seq`, the table should be empty, so
                // we pick a random index to unsuccessfully erase. Otherwise we
                // erase `erase_seq`.
                if (erase_seq == insert_seq) {
                    ASSERT_TRUE(!tbl.erase(key(rng(numkeys))));
                } else {
                    ASSERT_TRUE(tbl.erase(key(erase_seq++)));
                }
                break;
            case UPDATE:
                // Same as find, except we update to the same default value
                n = rng(numkeys);
                ASSERT_EQ(
                    n >= erase_seq && n < insert_seq,
                    tbl.update(key(n), Gen<VALUE>::value()));
                break;
            case UPSERT:
                // Pick a number from the full distribution, but cap it to the
                // insert_seq, so we don't insert a number greater than
                // insert_seq.
                n = std::max(rng(numkeys), static_cast<uint64_t>(insert_seq));
                tbl.upsert(key(n), upsert_fn, Gen<VALUE>::value());
                if (n == insert_seq) {
                    ++insert_seq;
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    try {
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
            throw std::runtime_error("Operation mix percentages must sum to 100\n");
        }
        if (g_seed == 0) {
            g_seed = std::random_device()();
        }

        pcg32 base_rng(g_seed);

        const size_t initial_capacity = 1UL << g_initial_capacity;
        const size_t total_ops = initial_capacity * g_total_ops_percentage / 100;

        // Create and size the table
        Table tbl(initial_capacity);

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
        std::shuffle(op_mix.begin(), op_mix.end(), base_rng);

        // Pre-generate all the keys we'd want to insert. In case the insert +
        // upsert percentage is too low, lower bound by the table capacity.
        // Also, to prevent rounding errors with the percentages, add 1000 to
        // the final number.
        const size_t prefill_elems = (initial_capacity * g_prefill_percentage / 100);
        const size_t max_insert_ops =
            total_ops *
            (g_insert_percentage + g_upsert_percentage) /
            100;
        const size_t insert_keys =
            std::min(initial_capacity, max_insert_ops) +
            prefill_elems +
            1000;
        std::vector<std::vector<uint64_t> > keys(g_threads);
        std::vector<std::thread> genkeys_threads(g_threads);
        for (size_t i = 0; i < g_threads; ++i) {
            genkeys_threads[i] = std::thread(
                genkeys_thread, std::ref(keys[i]),
                insert_keys / g_threads, g_seed, i);
        }
        for (auto& t : genkeys_threads) {
            t.join();
        }

        // Pre-fill the table
        std::vector<std::thread> prefill_threads(g_threads);
        for (size_t i = 0; i < g_threads; ++i) {
            prefill_threads[i] = std::thread(
                prefill_thread, std::ref(tbl), std::ref(keys[i]),
                prefill_elems / g_threads);
        }
        for (auto& t : prefill_threads) {
            t.join();
        }

        // Run the operation mix, timed
        std::vector<std::thread> mix_threads(g_threads);
        auto start_rss = max_rss();
        auto start_time = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < g_threads; ++i) {
            mix_threads[i] = std::thread(
                mix_thread, std::ref(tbl), total_ops / g_threads,
                std::ref(op_mix), std::ref(keys[i]), prefill_elems / g_threads);
        }
        for (auto& t : mix_threads) {
            t.join();
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        auto end_rss = max_rss();
        double seconds_elapsed = std::chrono::duration_cast<
            std::chrono::duration<double> >(end_time - start_time).count();

        // Print out args, preprocessor constants, and results in JSON format
        std::stringstream argstr;
        argstr << args[0] << " " << *arg_vars[0];
        for (size_t i = 1; i < sizeof(args)/sizeof(args[0]); ++i) {
            argstr << " " << args[i] << " " << *arg_vars[i];
        }
        const char* json_format = R"({
    "args": "%s",
    "key": "%s",
    "value": "%s",
    "table": "%s",
    "output": {
        "total_ops": {
            "name": "Total Operations",
            "units": "count",
            "value": %zu
        },
        "time_elapsed": {
            "name": "Time Elapsed",
            "units": "seconds",
            "value": %.4f
        },
        "throughput": {
            "name": "Throughput",
            "units": "count/seconds",
            "value": %.4f
        },
        "max_rss_change": {
            "name": "Change in Maximum RSS",
            "units": "bytes",
            "value": %ld
        }
    }
}
)";
        printf(json_format, argstr.str().c_str(), XSTR(KEY), XSTR(VALUE),
               TABLE, total_ops, seconds_elapsed,
               total_ops / seconds_elapsed, end_rss - start_rss);
    } catch (const std::exception& e) {
        std::cerr << e.what();
        std::exit(1);
    }
}
