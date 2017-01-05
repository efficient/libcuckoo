/** \file */

#ifndef _CUCKOOHASH_UTIL_HH
#define _CUCKOOHASH_UTIL_HH

#include <exception>
#include <thread>
#include <utility>
#include <vector>
#include "cuckoohash_config.hh" // for LIBCUCKOO_DEBUG

#if LIBCUCKOO_DEBUG
//! When \ref LIBCUCKOO_DEBUG is 0, LIBCUCKOO_DBG will printing out status
//! messages in various situations
#  define LIBCUCKOO_DBG(fmt, ...)                                          \
     fprintf(stderr, "\x1b[32m""[libcuckoo:%s:%d:%lu] " fmt"" "\x1b[0m",   \
             __FILE__,__LINE__, std::hash<std::thread::id>()(std::this_thread::get_id()), \
             __VA_ARGS__)
#else
//! When \ref LIBCUCKOO_DEBUG is 0, LIBCUCKOO_DBG does nothing
#  define LIBCUCKOO_DBG(fmt, ...)  do {} while (0)
#endif

/**
 * alignas() requires GCC >= 4.9, so we stick with the alignment attribute for
 * GCC.
 */
#ifdef __GNUC__
#define LIBCUCKOO_ALIGNAS(x) __attribute__((aligned(x)))
#else
#define LIBCUCKOO_ALIGNAS(x) alignas(x)
#endif

/**
 * At higher warning levels, MSVC produces an annoying warning that alignment
 * may cause wasted space: "structure was padded due to __declspec(align())".
 */
#ifdef _MSC_VER
#define LIBCUCKOO_SQUELCH_PADDING_WARNING __pragma(warning(suppress : 4324))
#else
#define LIBCUCKOO_SQUELCH_PADDING_WARNING
#endif

/**
 * thread_local requires GCC >= 4.8 and is not supported in some clang versions,
 * so we use __thread if thread_local is not supported
 */
#define LIBCUCKOO_THREAD_LOCAL thread_local
#if defined(__clang__)
#  if !__has_feature(cxx_thread_local)
#    undef LIBCUCKOO_THREAD_LOCAL
#    define LIBCUCKOO_THREAD_LOCAL __thread
#  endif
#elif defined(__GNUC__)
#  if __GNUC__ == 4 && __GNUC_MINOR__ < 8
#    undef LIBCUCKOO_THREAD_LOCAL
#    define LIBCUCKOO_THREAD_LOCAL __thread
#  endif
#endif

//! For enabling certain methods based on a condition. Here's an example.
//! LIBCUCKOO_ENABLE_IF(sizeof(int) == 4, int) method() {
//!     ...
//! }
#define LIBCUCKOO_ENABLE_IF(condition, return_type)                       \
    template <class Bogus = void*>                                        \
    typename std::enable_if<sizeof(Bogus) && condition, return_type>::type

/**
 * Thrown when an automatic expansion is triggered, but the load factor of the
 * table is below a minimum threshold, which can be set by the \ref
 * cuckoohash_map::minimum_load_factor method. This can happen if the hash
 * function does not properly distribute keys, or for certain adversarial
 * workloads.
 */
class libcuckoo_load_factor_too_low : public std::exception {
public:
    /**
     * Constructor
     *
     * @param lf the load factor of the table when the exception was thrown
     */
    libcuckoo_load_factor_too_low(const double lf)
        : load_factor_(lf) {}

    /**
     * @return a descriptive error message
     */
    virtual const char* what() const noexcept override {
        return "Automatic expansion triggered when load factor was below "
            "minimum threshold";
    }

    /**
     * @return the load factor of the table when the exception was thrown
     */
    double load_factor() const {
        return load_factor_;
    }
private:
    const double load_factor_;
};

/**
 * Thrown when an expansion is triggered, but the hashpower specified is greater
 * than the maximum, which can be set with the \ref
 * cuckoohash_map::maximum_hashpower method.
 */
class libcuckoo_maximum_hashpower_exceeded : public std::exception {
public:
    /**
     * Constructor
     *
     * @param hp the hash power we were trying to expand to
     */
    libcuckoo_maximum_hashpower_exceeded(const size_t hp)
        : hashpower_(hp) {}

    /**
     * @return a descriptive error message
     */
    virtual const char* what() const noexcept override {
        return "Expansion beyond maximum hashpower";
    }

    /**
     * @return the hashpower we were trying to expand to
     */
    size_t hashpower() const {
        return hashpower_;
    }
private:
    const size_t hashpower_;
};

#endif // _CUCKOOHASH_UTIL_HH
