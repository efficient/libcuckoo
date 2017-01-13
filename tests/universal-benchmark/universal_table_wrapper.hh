/* For each table to support, we define a wrapper class which holds the table
 * and implements all of the benchmarked operations. Below we list all the
 * methods each wrapper must implement.
 *
 * constructor(size_t n) // n is the initial capacity
 * template <typename K, typename V>
 * bool read(const K& k, V& v) const
 * template <typename K, typename V>
 * bool insert(const K& k, const V& v)
 * template <typename K>
 * bool erase(const K& k)
 * template <typename K, typename V>
 * bool update(const K& k, const V& v)
 * template <typename K, typename V>
 * void upsert(const K& k, Updater fn, const V& v)
 */

#ifndef _UNIVERSAL_TABLE_WRAPPER_HH
#define _UNIVERSAL_TABLE_WRAPPER_HH

#include <atomic>
#include <memory>
#include <utility>

#ifndef KEY
#error Must define KEY symbol as valid key type
#endif

#ifndef VALUE
#error Must define VALUE symbol as valid value type
#endif

std::atomic<size_t>
universal_benchmark_max_bytes_allocated = ATOMIC_VAR_INIT(0);

std::atomic<size_t>
universal_benchmark_current_bytes_allocated = ATOMIC_VAR_INIT(0);

#ifdef TRACKING_ALLOCATOR
template <typename T>
class Allocator : public std::allocator<T> {
private:
    using traits = std::allocator_traits<std::allocator<T> >;

public:
    using value_type = typename traits::value_type;
    using pointer = typename traits::pointer;
    using const_pointer = typename traits::const_pointer;
    using reference = value_type&;
    using const_reference = const value_type&;
    using void_pointer = typename traits::void_pointer;
    using const_void_pointer = typename traits::const_void_pointer;
    using size_type = typename traits::size_type;
    using difference_type = typename traits::difference_type;

    template <typename U>
    struct rebind {
        using other = Allocator<U>;
    };

    Allocator() {}

    template<typename U>
    Allocator(const Allocator<U>&) {}

    pointer address(reference x) const {
        return std::addressof(x);
    }

    const_pointer const_address(const_reference x) const {
        return std::addressof(x);
    }

    pointer allocate(size_type n, const_void_pointer hint = nullptr) {
        universal_benchmark_current_bytes_allocated.fetch_add(
            n * sizeof(value_type), std::memory_order_acq_rel);
        universal_benchmark_max_bytes_allocated.store(
            std::max(universal_benchmark_max_bytes_allocated.load(
                         std::memory_order_acquire),
                     universal_benchmark_current_bytes_allocated.load(
                         std::memory_order_acquire)),
            std::memory_order_release);
        return traits::allocate(allocator_, n, hint);
    }

    void deallocate(pointer p, size_type n) {
        universal_benchmark_current_bytes_allocated.fetch_sub(
            n * sizeof(value_type), std::memory_order_acq_rel);
        traits::deallocate(allocator_, p, n);
    }

    size_type max_size() const {
        return traits::max_size(allocator_);
    }

    template<typename... Args >
    void construct(pointer p, Args&&... args) {
        traits::construct(allocator_, p, std::forward<Args>(args)...);
    }

    void destroy(pointer p) {
        traits::destroy(allocator_, p);
    }

    bool operator==(const Allocator&) {
        return true;
    }

    bool operator!=(const Allocator&) {
        return false;
    }

private:
    typename traits::allocator_type allocator_;
};
#else
template <typename T>
using Allocator = std::allocator<T>;
#endif

#ifdef LIBCUCKOO
#define TABLE "LIBCUCKOO"
#define TABLE_TYPE "cuckoohash_map"
#include <libcuckoo/cuckoohash_map.hh>

class Table {
public:
    Table(size_t n) : tbl(n) {}

    template <typename K, typename V>
    bool read(const K& k, V& v) const {
        return tbl.find(k, v);
    }

    template <typename K, typename V>
    bool insert(const K& k, const V& v) {
        return tbl.insert(k, v);
    }

    template <typename K>
    bool erase(const K& k) {
        return tbl.erase(k);
    }

    template <typename K, typename V>
    bool update(const K& k, const V& v) {
        return tbl.update(k, v);
    }

    template <typename K, typename Updater, typename V>
    void upsert(const K& k, Updater fn, const V& v) {
        tbl.upsert(k, fn, v);
    }

private:
    cuckoohash_map<KEY, VALUE, std::hash<KEY>, std::equal_to<KEY>,
                   Allocator<std::pair<const KEY, VALUE> > > tbl;
};

#else
#error Must define LIBCUCKOO
#endif

#endif // _UNIVERSAL_TABLE_WRAPPER_HH
