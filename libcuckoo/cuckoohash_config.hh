/** \file */

#ifndef _CUCKOOHASH_CONFIG_HH
#define _CUCKOOHASH_CONFIG_HH

#include <cstddef>

//! The default maximum number of keys per bucket
constexpr size_t LIBCUCKOO_DEFAULT_SLOT_PER_BUCKET = 4;

//! The default number of elements in an empty hash table
constexpr size_t LIBCUCKOO_DEFAULT_SIZE =
    (1U << 16) * LIBCUCKOO_DEFAULT_SLOT_PER_BUCKET;

//! On a scale of 0 to 16, the memory granularity of the locks array. 0 is the
//! least granular, meaning the array is a contiguous array and thus offers the
//! best performance but the greatest memory overhead. 16 is the most granular,
//! offering the least memory overhead but worse performance.
constexpr size_t LIBCUCKOO_LOCK_ARRAY_GRANULARITY = 0;

//! The default minimum load factor that the table allows for automatic
//! expansion. It must be a number between 0.0 and 1.0. The table will throw
//! libcuckoo_load_factor_too_low if the load factor falls below this value
//! during an automatic expansion.
constexpr double LIBCUCKOO_DEFAULT_MINIMUM_LOAD_FACTOR = 0.05;

//! An alias for the value that sets no limit on the maximum hashpower. If this
//! value is set as the maximum hashpower limit, there will be no limit. Since 0
//! is the only hashpower that can never occur, it should stay at 0. This is
//! also the default initial value for the maximum hashpower in a table.
constexpr size_t LIBCUCKOO_NO_MAXIMUM_HASHPOWER = 0;

//! set LIBCUCKOO_DEBUG to 1 to enable debug output
#define LIBCUCKOO_DEBUG 0

#endif // _CUCKOOHASH_CONFIG_HH
