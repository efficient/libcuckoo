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

#include "../../src/cuckoohash_map.hh"

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
public:
    typedef T value_type;
    typedef T* pointer;
    typedef T& reference;
    typedef const T* const_pointer;
    typedef const T& const_reference;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;

    template <class U>
    struct rebind {
        typedef TrackingAllocator<U, BOUND> other;
    };

    TrackingAllocator() {}
    TrackingAllocator(const TrackingAllocator&) {}
    template <class U> TrackingAllocator(
        const TrackingAllocator<U, BOUND>&) {}

    ~TrackingAllocator() {}

    pointer address(reference x) const {
        return allocator_().address(x);
    }
    const_pointer address(const_reference x) const {
        return allocator_().address(x);
    }

    pointer allocate(size_type n, std::allocator<void>::const_pointer hint=0) {
        const size_type bytes_to_allocate = sizeof(T) * n;
        if (BOUND >= 0 && get_unfreed_bytes() + bytes_to_allocate > BOUND) {
            throw std::bad_alloc();
        }
        get_unfreed_bytes() += bytes_to_allocate;
        return allocator_().allocate(n, hint);
    }

    void deallocate(pointer p, size_type n) {
        get_unfreed_bytes() -= (sizeof(T) * n);
        allocator_().deallocate(p, n);
    }

    size_type max_size() const {
        return allocator_().max_size();
    }

    void construct(pointer p, const_reference val) {
        allocator_().construct(p, val);
    }

    template <class U, class... Args>
    void construct(U* p, Args&&... args) {
        allocator_().construct(p, std::forward<Args>(args)...);
    }

    void destroy(pointer p) {
        allocator_().destroy(p);
    }

    template <class U>
    void destroy(U* p) {
        allocator_().destroy(p);
    }


private:
    typedef std::allocator<T> allocator_;
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

// Returns the number of slots the table has to store key-value pairs.
template <class CuckoohashMap>
size_t table_capacity(const CuckoohashMap& table) {
    return CuckoohashMap::slot_per_bucket * (1U << table.hashpower());
}

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
