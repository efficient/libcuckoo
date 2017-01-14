// Utilities for unit testing
#ifndef UNIT_TEST_UTIL_HH_
#define UNIT_TEST_UTIL_HH_

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <utility>

#include <libcuckoo/cuckoohash_map.hh>

// Returns a statically allocated value used to keep track of how many unfreed
// bytes have been allocated. This value is shared across all threads.
std::atomic<int64_t>& get_unfreed_bytes();

// We define a a allocator class that keeps track of how many unfreed bytes have
// been allocated. Users can specify an optional bound for how many bytes can be
// unfreed, and the allocator will fail if asked to allocate above that bound
// (note that behavior with this bound with concurrent allocations will be hard
// to deal with). A bound below 0 is inactive (the default is -1).
template <class T, int64_t BOUND = -1>
class TrackingAllocator {
private:
    using traits_ = std::allocator_traits<std::allocator<T> >;
public:
    using value_type = T;
    template <typename U>
    struct rebind {
        using other = TrackingAllocator<U, BOUND>;
    };

    TrackingAllocator() {}

    template <typename U>
    TrackingAllocator(const TrackingAllocator<U, BOUND>& alloc)
        : allocator_(alloc.allocator_) {}

    typename traits_::pointer
    allocate(typename traits_::size_type n,
             typename traits_::const_void_pointer hint = nullptr) {
        const size_t bytes_to_allocate = sizeof(T) * n;
        if (BOUND >= 0 && get_unfreed_bytes() + bytes_to_allocate > BOUND) {
            throw std::bad_alloc();
        }
        get_unfreed_bytes() += bytes_to_allocate;
        return traits_::allocate(allocator_, n, hint);
    }

    void deallocate(typename traits_::pointer p,
                    typename traits_::size_type n) {
        get_unfreed_bytes() -= (sizeof(T) * n);
        traits_::deallocate(allocator_, p, n);
    }

    std::allocator<T> allocator_;
};

using IntIntTable = cuckoohash_map<
    int,
    int,
    std::hash<int>,
    std::equal_to<int>,
    std::allocator<std::pair<const int, int>>,
    4>;

template <class Alloc>
using IntIntTableWithAlloc = cuckoohash_map<
    int,
    int,
    std::hash<int>,
    std::equal_to<int>,
    Alloc,
    4>;


using StringIntTable = cuckoohash_map<
    std::string,
    int,
    std::hash<std::string>,
    std::equal_to<std::string>,
    std::allocator<std::pair<const std::string, int>>,
    4>;

namespace std {
    template <typename T>
    struct hash<unique_ptr<T> > {
        size_t operator()(const unique_ptr<T>& ptr) const {
            return std::hash<T>()(*ptr);
        }

        size_t operator()(const T* ptr) const {
            return std::hash<T>()(*ptr);
        }
    };

    template <typename T>
    struct equal_to<unique_ptr<T> > {
        bool operator()(const unique_ptr<T>& ptr1,
                        const unique_ptr<T>& ptr2) const {
            return *ptr1 == *ptr2;
        }

        bool operator()(const T* ptr1,
                        const unique_ptr<T>& ptr2) const {
            return *ptr1 == *ptr2;
        }

        bool operator()(const unique_ptr<T>& ptr1,
                        const T* ptr2) const {
            return *ptr1 == *ptr2;
        }
    };
}

template <typename T>
using UniquePtrTable = cuckoohash_map<
    std::unique_ptr<T>,
    std::unique_ptr<T>,
    std::hash<std::unique_ptr<T> >,
    std::equal_to<std::unique_ptr<T> >,
    std::allocator<std::pair<const std::unique_ptr<T>, std::unique_ptr<T> > >,
    4>;

// Some unit tests need access into certain private data members of the table.
// This class is a friend of the table, so it can access those.
class UnitTestInternalAccess {
public:
    static const size_t IntIntBucketSize = sizeof(IntIntTable::Bucket);

    template <class CuckoohashMap>
    static size_t old_table_info_size(const CuckoohashMap& table) {
        // This is not thread-safe
        return table.old_table_infos.size();
    }

    template <class CuckoohashMap>
    static typename CuckoohashMap::SnapshotNoLockResults snapshot_table_nolock(
        const CuckoohashMap& table) {
        return table.snapshot_table_nolock();
    }

    template <class CuckoohashMap>
    static typename CuckoohashMap::partial_t partial_key(const size_t hv) {
        return CuckoohashMap::partial_key(hv);
    }

    template <class CuckoohashMap>
    static size_t index_hash(const size_t hashpower, const size_t hv) {
        return CuckoohashMap::index_hash(hashpower, hv);
    }

    template <class CuckoohashMap>
    static size_t alt_index(const size_t hashpower,
                            const typename CuckoohashMap::partial_t partial,
                            const size_t index) {
        return CuckoohashMap::alt_index(hashpower, partial, index);
    }

    template <class CuckoohashMap>
    static size_t reserve_calc(size_t n) {
        return CuckoohashMap::reserve_calc(n);
    }
};

#endif // UNIT_TEST_UTIL_HH_
