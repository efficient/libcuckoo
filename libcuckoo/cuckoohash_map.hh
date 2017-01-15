/** \file */

#ifndef _CUCKOOHASH_MAP_HH
#define _CUCKOOHASH_MAP_HH

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "cuckoohash_config.hh"
#include "cuckoohash_util.hh"
#include "libcuckoo_lazy_array.hh"

/**
 * A concurrent hash table
 *
 * @tparam Key type of keys in the table
 * @tparam T type of values in the table
 * @tparam Pred type of equality comparison functor
 * @tparam Alloc type of key-value pair allocator
 * @tparam SLOT_PER_BUCKET number of slots for each bucket in the table
 */
template < class Key,
           class T,
           class Hash = std::hash<Key>,
           class Pred = std::equal_to<Key>,
           class Alloc = std::allocator<std::pair<const Key, T>>,
           std::size_t SLOT_PER_BUCKET = LIBCUCKOO_DEFAULT_SLOT_PER_BUCKET
           >
class cuckoohash_map {
public:
    /** @name Type Declarations */
    /**@{*/

    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<const Key, T>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using hasher = Hash;
    using key_equal = Pred;
    using allocator_type = Alloc;

private:
    using allocator_traits_ = std::allocator_traits<allocator_type>;

public:
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = typename allocator_traits_::pointer;
    using const_pointer = typename allocator_traits_::const_pointer;
    class locked_table;

    /**@}*/

    /** @name Table Parameters */
    /**@{*/

    /**
     * The number of slots per hash bucket
     */
    static constexpr size_type slot_per_bucket() {
        return SLOT_PER_BUCKET;
    }

    /**@}*/

    /** @name Constructors and Destructors */
    /**@{*/

    /**
     * Creates a new cuckohash_map instance
     *
     * @param n the number of elements to reserve space for initially
     * @param hf hash function instance to use
     * @param eql equality function instance to use
     * @param alloc allocator instance to use
     */
    cuckoohash_map(size_type n = LIBCUCKOO_DEFAULT_SIZE,
                   const hasher& hf = hasher(),
                   const key_equal& eql = key_equal(),
                   const allocator_type& alloc = allocator_type())
        : hashpower_(reserve_calc(n)),
          hash_fn_(hf),
          eq_fn_(eql),
          allocator_(alloc),
          buckets_(hashsize(hashpower()), alloc),
          locks_(hashsize(hashpower()), alloc),
          expansion_lock_(),
          minimum_load_factor_(LIBCUCKOO_DEFAULT_MINIMUM_LOAD_FACTOR),
          maximum_hashpower_(LIBCUCKOO_NO_MAXIMUM_HASHPOWER) {}

    /**
     * Destroys the table. The destructors of all elements stored in the table
     * are destroyed, and then the table storage is deallocated.
     */
    ~cuckoohash_map() {
        cuckoo_clear();
    }

    /**@}*/

    /** @name Table Details
     *
     * Methods for getting information about the table. Methods that query
     * changing properties of the table are not synchronized with concurrent
     * operations, and may return out-of-date information if the table is being
     * concurrently modified.
     *
     */
    /**@{*/

    /**
     * Returns the function that hashes the keys
     *
     * @return the hash function
     */
    hasher hash_function() const {
        return hash_fn_;
    }

    /**
     * Returns the function that compares keys for equality
     *
     * @return the key comparison function
     */
    key_equal key_eq() const {
        return eq_fn_;
    }

    /**
     * Returns the allocator associated with the container
     *
     * @return the associated allocator
     */
    allocator_type get_allocator() const {
        return allocator_;
    }

    /**
     * Returns the hashpower of the table, which is log<SUB>2</SUB>(@ref
     * bucket_count()).
     *
     * @return the hashpower
     */
    size_type hashpower() const {
        return hashpower_.load(std::memory_order_acquire);
    }

    /**
     * Returns the number of buckets in the table.
     *
     * @return the bucket count
     */
    size_type bucket_count() const {
        return buckets_.size();
    }

    /**
     * Returns whether the table is empty or not.
     *
     * @return true if the table is empty, false otherwise
     */
    bool empty() const {
        for (size_type i = 0; i < locks_.size(); ++i) {
            if (locks_[i].elem_counter() > 0) {
                return false;
            }
        }
        return true;
    }

    /**
     * Returns the number of elements in the table.
     *
     * @return number of elements in the table
     */
    size_type size() const {
        size_type s = 0;
        for (size_type i = 0; i < locks_.size(); ++i) {
            s += locks_[i].elem_counter();
        }
        return s;
    }

    /** Returns the current capacity of the table, that is, @ref bucket_count()
     * &times; @ref slot_per_bucket().
     *
     * @return capacity of table
     */
    size_type capacity() const {
        return bucket_count() * slot_per_bucket();
    }

    /**
     * Returns the percentage the table is filled, that is, @ref size() &divide;
     * @ref capacity().
     *
     * @return load factor of the table
     */
    double load_factor() const {
        return static_cast<double>(size()) / static_cast<double>(capacity());
    }

    /**
     * Sets the minimum load factor allowed for automatic expansions. If an
     * expansion is needed when the load factor of the table is lower than this
     * threshold, @ref libcuckoo_load_factor_too_low is thrown. It will not be
     * thrown for an explicitly-triggered expansion.
     *
     * @param mlf the load factor to set the minimum to
     * @throw std::invalid_argument if the given load factor is less than 0.0
     * or greater than 1.0
     */
    void minimum_load_factor(const double mlf) {
        if (mlf < 0.0) {
            throw std::invalid_argument(
                "load factor " + std::to_string(mlf) + " cannot be "
                "less than 0");
        } else if (mlf > 1.0) {
            throw std::invalid_argument(
                "load factor " + std::to_string(mlf) + " cannot be "
                "greater than 1");
        }
        minimum_load_factor_.store(mlf, std::memory_order_release);
    }

    /**
     * Returns the minimum load factor of the table
     *
     * @return the minimum load factor
     */
    double minimum_load_factor() {
        return minimum_load_factor_.load(std::memory_order_acquire);
    }

    /**
     * Sets the maximum hashpower the table can be. If set to @ref
     * LIBCUCKOO_NO_MAXIMUM_HASHPOWER, there will be no limit on the hashpower.
     * Otherwise, the table will not be able to expand beyond the given
     * hashpower, either by an explicit or an automatic expansion.
     *
     * @param mhp the hashpower to set the maximum to
     * @throw std::invalid_argument if the current hashpower exceeds the limit
     */
    void maximum_hashpower(size_type mhp) {
        if (mhp != LIBCUCKOO_NO_MAXIMUM_HASHPOWER && hashpower() > mhp) {
            throw std::invalid_argument(
                "maximum hashpower " + std::to_string(mhp) + " is less than "
                "current hashpower");

        }
        maximum_hashpower_.store(mhp, std::memory_order_release);
    }

    /**
     * Returns the maximum hashpower of the table
     *
     * @return the maximum hashpower
     */
    size_type maximum_hashpower() {
        return maximum_hashpower_.load(std::memory_order_acquire);
    }

    /**@}*/

    /** @name Table Operations
     *
     * These are operations that affect the data in the table. They are safe to
     * call concurrently with each other.
     *
     */
    /**@{*/

    /**
     * Searches the table for @p key, and invokes @p fn on the value. @p fn is
     * not allowed to modify the contents of the value if found.
     *
     * @tparam K type of the key. This can be any type comparable with @c key_type
     * @tparam F type of the functor. It should implement the method
     * <tt>void operator()(const mapped_type&)</tt>.
     * @param key the key to search for
     * @param fn the functor to invoke if the element is found
     * @return true if the key was found and functor invoked, false otherwise
     */
    template <typename K, typename F>
    bool find_fn(const K& key, F fn) const {
        const hash_value hv = hashed_key(key);
        const auto b = snapshot_and_lock_two<locking_active>(hv);
        const table_position pos = cuckoo_find(
            key, hv.partial, b.first(), b.second());
        if (pos.status == ok) {
            fn(buckets_[pos.index].val(pos.slot));
            return true;
        } else {
            return false;
        }
    }

    /**
     * Searches the table for @p key, and invokes @p fn on the value. @p fn is
     * allow to modify the contents of the value if found.
     *
     * @tparam K type of the key. This can be any type comparable with @c key_type
     * @tparam F type of the functor. It should implement the method
     * <tt>void operator()(mapped_type&)</tt>.
     * @param key the key to search for
     * @param fn the functor to invoke if the element is found
     * @return true if the key was found and functor invoked, false otherwise
     */
    template <typename K, typename F>
    bool update_fn(const K& key, F fn) {
        const hash_value hv = hashed_key(key);
        const auto b = snapshot_and_lock_two<locking_active>(hv);
        const table_position pos = cuckoo_find(
            key, hv.partial, b.first(), b.second());
        if (pos.status == ok) {
            fn(buckets_[pos.index].val(pos.slot));
            return true;
        } else {
            return false;
        }
    }

    /**
     * Searches for @p key in the table. If the key is not there, it is inserted
     * with @p val. If the key is there, then @p fn is called on the value. The
     * key will be immediately constructed as @c key_type(std::forward<K>(key)).
     * If the insertion succeeds, this constructed key will be moved into the
     * table and the value constructed from the @p val parameters. If the
     * insertion fails, the constructed key will be destroyed, and the @p val
     * parameters will remain valid. If there is no room left in the table, it
     * will be automatically expanded. Expansion may throw exceptions.
     *
     * @tparam K type of the key
     * @tparam F type of the functor. It should implement the method
     * <tt>void operator()(mapped_type&)</tt>.
     * @tparam Args list of types for the value constructor arguments
     * @param key the key to insert into the table
     * @param fn the functor to invoke if the element is found
     * @param val a list of constructor arguments with which to create the value
     * @return true if a new key was inserted, false if the key was already in
     * the table
     */
    template <typename K, typename F, typename... Args>
    bool upsert(K&& key, F fn, Args&&... val) {
        K k(std::forward<K>(key));
        hash_value hv = hashed_key(k);
        auto b = snapshot_and_lock_two<locking_active>(hv);
        table_position pos = cuckoo_insert_loop(hv, b, k);
        if (pos.status == ok) {
            add_to_bucket(pos.index, pos.slot, hv.partial, k,
                          std::forward<Args>(val)...);
        } else {
            fn(buckets_[pos.index].val(pos.slot));
        }
        return pos.status == ok;
    }

    /**
     * Searches for @p key in the table, and invokes @p fn on the value if the
     * key is found. The functor can mutate the value, and should return @c true
     * in order to erase the element, and @c false otherwise.
     *
     * @tparam K type of the key
     * @tparam F type of the functor. It should implement the method
     * <tt>bool operator()(mapped_type&)</tt>.
     * @param key the key to possibly erase from the table
     * @param fn the functor to invoke if the element is found
     * @return true if @p key was found and @p fn invoked, false otherwise
     */
    template <typename K, typename F>
    bool erase_fn(const K& key, F fn) {
        const hash_value hv = hashed_key(key);
        const auto b = snapshot_and_lock_two<locking_active>(hv);
        const table_position pos = cuckoo_find(
            key, hv.partial, b.first(), b.second());
        if (pos.status == ok) {
            if (fn(buckets_[pos.index].val(pos.slot))) {
                del_from_bucket(buckets_[pos.index], pos.index, pos.slot);
            }
            return true;
        } else {
            return false;
        }
    }

    /**
     * Copies the value associated with @p key into @p val. Equivalent to
     * calling @ref find_fn with a functor that copies the value into @p val. @c
     * mapped_type must be @c CopyAssignable.
     */
    template <typename K>
    bool find(const K& key, mapped_type& val) const {
        return find_fn(key, [&val](const mapped_type& v) mutable {
                val = v;
            });
    }

    /** Searches the table for @p key, and returns the associated value it
     * finds. @c mapped_type must be @c CopyConstructible.
     *
     * @tparam K type of the key
     * @param key the key to search for
     * @return the value associated with the given key
     * @throw std::out_of_range if the key is not found
     */
    template <typename K>
    mapped_type find(const K& key) const {
        const hash_value hv = hashed_key(key);
        const auto b = snapshot_and_lock_two<locking_active>(hv);
        const table_position pos = cuckoo_find(
            key, hv.partial, b.first(), b.second());
        if (pos.status == ok) {
            return buckets_[pos.index].val(pos.slot);
        } else {
            throw std::out_of_range("key not found in table");
        }
    }

    /** Returns whether or not @p key is in the table. Equivalent to @ref
     * find_fn with a functor that does nothing.
     */
    template <typename K>
    bool contains(const K& key) const {
        return find_fn(key, [](const mapped_type&) {});
    }

    /**
     * Updates the value associated with @p key to @p val. Equivalent to calling
     * @ref update_fn with a functor that copies @p val into the associated
     * value. @c mapped_type must be @c MoveAssignable or @c CopyAssignable.
     */
    template <typename K, typename V>
    bool update(const K& key, V&& val) {
        return update_fn(key, [&val](mapped_type& v) {
                v = std::forward<V>(val);
            });
    }

    /**
     * Inserts the key-value pair into the table. Equivalent to calling @ref
     * upsert with a functor that does nothing.
     */
    template <typename K, typename... Args>
    bool insert(K&& key, Args&&... val) {
        return upsert(std::forward<K>(key), [](mapped_type&) {},
                      std::forward<Args>(val)...);
    }

    /**
     * Erases the key from the table. Equivalent to calling @ref erase_fn with a
     * functor that just returns true.
     */
    template <typename K>
    bool erase(const K& key) {
        return erase_fn(key, [](mapped_type&) { return true; });
    }

    /**
     * Resizes the table to the given hashpower. If this hashpower is not larger
     * than the current hashpower, then it decreases the hashpower to the
     * maximum of the specified value and the smallest hashpower that can hold
     * all the elements currently in the table.
     *
     * @param n the hashpower to set for the table
     * @return true if the table changed size, false otherwise
     */
    bool rehash(size_type n) {
        return cuckoo_rehash<locking_active>(n);
    }

    /**
     * Reserve enough space in the table for the given number of elements. If
     * the table can already hold that many elements, the function will shrink
     * the table to the smallest hashpower that can hold the maximum of the
     * specified amount and the current table size.
     *
     * @param n the number of elements to reserve space for
     * @return true if the size of the table changed, false otherwise
     */
    bool reserve(size_type n) {
        return cuckoo_reserve<locking_active>(n);
    }

    /**
     * Removes all elements in the table, calling their destructors.
     */
    void clear() {
        auto unlocker = snapshot_and_lock_all<locking_active>();
        cuckoo_clear();
    }

    /**
     * Construct a @ref locked_table object that owns all the locks in the
     * table.
     *
     * @return a \ref locked_table instance
     */
    locked_table lock_table() {
        return locked_table(*this);
    }

    /**@}*/

private:
    // Hashing types and functions

    // Type of the partial key
    using partial_t = uint8_t;

    // true if the key is small and simple, which means using partial keys for
    // lookup would probably slow us down
    static constexpr bool is_simple =
        std::is_pod<key_type>::value && sizeof(key_type) <= 8;

    // Contains a hash and partial for a given key. The partial key is used for
    // partial-key cuckoohashing, and for finding the alternate bucket of that a
    // key hashes to.
    struct hash_value {
        size_type hash;
        partial_t partial;
    };

    template <typename K>
    hash_value hashed_key(const K& key) const {
        const size_type hash = hash_function()(key);
        return { hash, partial_key(hash) };
    }

    template <typename K>
    size_type hashed_key_only_hash(const K& key) const {
        return hash_function()(key);
    }

    // hashsize returns the number of buckets corresponding to a given
    // hashpower.
    static inline size_type hashsize(const size_type hp) {
        return size_type(1) << hp;
    }

    // hashmask returns the bitmask for the buckets array corresponding to a
    // given hashpower.
    static inline size_type hashmask(const size_type hp) {
        return hashsize(hp) - 1;
    }

    // The partial key must only depend on the hash value. It cannot change with
    // the hashpower, because, in order for `cuckoo_fast_double` to work
    // properly, the alt_index must only grow by one bit at the top each time we
    // expand the table.
    static partial_t partial_key(const size_type hash) {
        const uint64_t hash_64bit = hash;
        const uint32_t hash_32bit = (
            static_cast<uint32_t>(hash_64bit) ^
            static_cast<uint32_t>(hash_64bit >> 32));
        const uint16_t hash_16bit = (
            static_cast<uint16_t>(hash_32bit) ^
            static_cast<uint16_t>(hash_32bit >> 16));
        const uint16_t hash_8bit = (
            static_cast<uint8_t>(hash_16bit) ^
            static_cast<uint8_t>(hash_16bit >> 8));
        return hash_8bit;
    }

    // index_hash returns the first possible bucket that the given hashed key
    // could be.
    static inline size_type index_hash(const size_type hp, const size_type hv) {
        return hv & hashmask(hp);
    }

    // alt_index returns the other possible bucket that the given hashed key
    // could be. It takes the first possible bucket as a parameter. Note that
    // this function will return the first possible bucket if index is the
    // second possible bucket, so alt_index(ti, partial, alt_index(ti, partial,
    // index_hash(ti, hv))) == index_hash(ti, hv).
    static inline size_type alt_index(const size_type hp, const partial_t partial,
                                   const size_type index) {
        // ensure tag is nonzero for the multiply. 0xc6a4a7935bd1e995 is the
        // hash constant from 64-bit MurmurHash2
        const size_type nonzero_tag = static_cast<size_type>(partial) + 1;
        return (index ^ (nonzero_tag * 0xc6a4a7935bd1e995)) & hashmask(hp);
    }

    // Locking types and functions

    using locking_active = std::integral_constant<bool, true>;
    using locking_inactive = std::integral_constant<bool, false>;

    // A fast, lightweight spinlock
    LIBCUCKOO_SQUELCH_PADDING_WARNING
    class LIBCUCKOO_ALIGNAS(64) spinlock {
    public:
        spinlock() noexcept : elem_counter_(0) {
            lock_.clear();
        }

        void lock(locking_active) {
            while (lock_.test_and_set(std::memory_order_acq_rel));
        }

        void lock(locking_inactive) {}

        void unlock(locking_active) {
            lock_.clear(std::memory_order_release);
        }

        void unlock(locking_inactive) {}

        bool try_lock(locking_active) {
            return !lock_.test_and_set(std::memory_order_acq_rel);
        }

        bool try_lock(locking_inactive) {
            return true;
        }

        size_type& elem_counter() {
            return elem_counter_;
        }

    private:
        std::atomic_flag lock_;
        size_type elem_counter_;
    };

    // The type of the locks container
    static_assert(LIBCUCKOO_LOCK_ARRAY_GRANULARITY >= 0 &&
                  LIBCUCKOO_LOCK_ARRAY_GRANULARITY <= 16,
                  "LIBCUCKOO_LOCK_ARRAY_GRANULARITY constant must be between "
                  "0 and 16, inclusive");
    using locks_t = libcuckoo_lazy_array<
        16 - LIBCUCKOO_LOCK_ARRAY_GRANULARITY, LIBCUCKOO_LOCK_ARRAY_GRANULARITY,
        spinlock,
        typename allocator_traits_::template rebind_alloc<spinlock>
        >;

    // The type of the expansion lock
    using expansion_lock_t = std::mutex;

    // Classes for managing locked buckets. By storing and moving around sets of
    // locked buckets in these classes, we can ensure that they are unlocked
    // properly.

    template <typename LOCK_T>
    class OneBucket {
    public:
        OneBucket() {}
        OneBucket(locks_t* locks, size_type i)
            : locks_(locks, OneUnlocker{i}) {}

    private:
        struct OneUnlocker {
            size_type i;
            void operator()(locks_t* p) const {
                (*p)[lock_ind(i)].unlock(LOCK_T());
            }
        };

        std::unique_ptr<locks_t, OneUnlocker> locks_;
    };

    template <typename LOCK_T>
    class TwoBuckets {
    public:
        TwoBuckets() {}
        TwoBuckets(locks_t* locks, size_type i1, size_type i2)
            : locks_(locks, TwoUnlocker{i1, i2}) {}

        size_type first() const {
            return locks_.get_deleter().i1;
        }

        size_type second() const {
            return locks_.get_deleter().i2;
        }

        bool is_active() const {
            return static_cast<bool>(locks_);
        }

        void unlock() {
            locks_.reset(nullptr);
        }

    private:
        struct TwoUnlocker {
            size_type i1, i2;
            void operator()(locks_t* p) const {
                const size_type l1 = lock_ind(i1);
                const size_type l2 = lock_ind(i2);
                (*p)[l1].unlock(LOCK_T());
                if (l1 != l2) {
                    (*p)[l2].unlock(LOCK_T());
                }
            }
        };

        std::unique_ptr<locks_t, TwoUnlocker> locks_;
    };

    template <typename LOCK_T>
    class AllBuckets {
    public:
        AllBuckets(locks_t* locks) : locks_(locks) {}

        bool is_active() const {
            return static_cast<bool>(locks_);
        }

        void unlock() {
            locks_.reset(nullptr);
        }

        void release() {
            (void)locks_.release();
        }

    private:
        struct AllUnlocker {
            void operator()(locks_t* p) const {
                for (size_type i = 0; i < p->size(); ++i) {
                    (*p)[i].unlock(LOCK_T());
                }
            }
        };

        std::unique_ptr<locks_t, AllUnlocker> locks_;
    };

    // This exception is thrown whenever we try to lock a bucket, but the
    // hashpower is not what was expected
    class hashpower_changed {};

    // After taking a lock on the table for the given bucket, this function will
    // check the hashpower to make sure it is the same as what it was before the
    // lock was taken. If it isn't unlock the bucket and throw a
    // hashpower_changed exception.
    template <typename LOCK_T>
    inline void check_hashpower(const size_type hp, const size_type lock) const {
        if (hashpower() != hp) {
            locks_[lock].unlock(LOCK_T());
            LIBCUCKOO_DBG("%s", "hashpower changed\n");
            throw hashpower_changed();
        }
    }

    // locks the given bucket index.
    //
    // throws hashpower_changed if it changed after taking the lock.
    template <typename LOCK_T>
    inline OneBucket<LOCK_T> lock_one(const size_type hp, const size_type i) const {
        const size_type l = lock_ind(i);
        locks_[l].lock(LOCK_T());
        check_hashpower<LOCK_T>(hp, l);
        return OneBucket<LOCK_T>(&locks_, i);
    }

    // locks the two bucket indexes, always locking the earlier index first to
    // avoid deadlock. If the two indexes are the same, it just locks one.
    //
    // throws hashpower_changed if it changed after taking the lock.
    template <typename LOCK_T>
    TwoBuckets<LOCK_T> lock_two(const size_type hp, const size_type i1,
                        const size_type i2) const {
        size_type l1 = lock_ind(i1);
        size_type l2 = lock_ind(i2);
        if (l2 < l1) {
            std::swap(l1, l2);
        }
        locks_[l1].lock(LOCK_T());
        check_hashpower<LOCK_T>(hp, l1);
        if (l2 != l1) {
            locks_[l2].lock(LOCK_T());
        }
        return TwoBuckets<LOCK_T>(&locks_, i1, i2);
    }

    // lock_two_one locks the three bucket indexes in numerical order, returning
    // the containers as a two (i1 and i2) and a one (i3). The one will not be
    // active if i3 shares a lock index with i1 or i2.
    //
    // throws hashpower_changed if it changed after taking the lock.
    template <typename LOCK_T>
    std::pair<TwoBuckets<LOCK_T>, OneBucket<LOCK_T>>
    lock_three(const size_type hp, const size_type i1,
               const size_type i2, const size_type i3) const {
        std::array<size_type, 3> l{{lock_ind(i1), lock_ind(i2), lock_ind(i3)}};
	// Lock in order.
	if (l[2] < l[1]) std::swap(l[2], l[1]);
	if (l[2] < l[0]) std::swap(l[2], l[0]);
	if (l[1] < l[0]) std::swap(l[1], l[0]);
        locks_[l[0]].lock(LOCK_T());
        check_hashpower<LOCK_T>(hp, l[0]);
        if (l[1] != l[0]) {
            locks_[l[1]].lock(LOCK_T());
        }
        if (l[2] != l[1]) {
            locks_[l[2]].lock(LOCK_T());
        }
        return std::make_pair(
            TwoBuckets<LOCK_T>(&locks_, i1, i2),
            OneBucket<LOCK_T>(
                (lock_ind(i3) == lock_ind(i1) || lock_ind(i3) == lock_ind(i2)) ?
                nullptr : &locks_, i3)
            );
    }

    // snapshot_and_lock_two loads locks the buckets associated with the given
    // hash value, making sure the hashpower doesn't change before the locks are
    // taken. Thus it ensures that the buckets and locks corresponding to the
    // hash value will stay correct as long as the locks are held. It returns
    // the bucket indices associated with the hash value and the current
    // hashpower.
    template <typename LOCK_T>
    TwoBuckets<LOCK_T> snapshot_and_lock_two(const hash_value& hv) const {
        while (true) {
            // Store the current hashpower we're using to compute the buckets
            const size_type hp = hashpower();
            const size_type i1 = index_hash(hp, hv.hash);
            const size_type i2 = alt_index(hp, hv.partial, i1);
            try {
                return lock_two<LOCK_T>(hp, i1, i2);
            } catch (hashpower_changed&) {
                // The hashpower changed while taking the locks. Try again.
                continue;
            }
        }
    }

    // snapshot_and_lock_all takes all the locks, and returns a deleter object
    // that releases the locks upon destruction. Note that after taking all the
    // locks, it is okay to change the buckets_ vector and the hashpower_, since
    // no other threads should be accessing the buckets.
    template <typename LOCK_T>
    AllBuckets<LOCK_T> snapshot_and_lock_all() const {
        for (size_type i = 0; i < locks_.size(); ++i) {
            locks_[i].lock(LOCK_T());
        }
        return AllBuckets<LOCK_T>(&locks_);
    }

    // lock_ind converts an index into buckets to an index into locks.
    static inline size_type lock_ind(const size_type bucket_ind) {
        return bucket_ind & (locks_t::max_size() - 1);
    }

    // Data storage types and functions

    // Value type without const Key, used for storage
    using storage_value_type = std::pair<key_type, mapped_type>;

    // The Bucket type holds slot_per_bucket() partial keys, key-value pairs,
    // and a occupied bitset, which indicates whether the slot at the given bit
    // index is in the table or not. It uses aligned_storage arrays to store the
    // keys and values to allow constructing and destroying key-value pairs in
    // place. Internally, the values are stored without the const qualifier in
    // the key, to enable modifying bucket memory.
    class Bucket {
    public:
        Bucket() noexcept {}
        // The destructor does nothing to the key-value pairs, since we'd need
        // an allocator to properly destroy the elements.
        ~Bucket() noexcept {}

        // No move or copy constructors, since we'd need an
        // instance of the allocator to do any constructions or destructions
        Bucket(const Bucket&) = delete;
        Bucket(Bucket&&) = delete;
        Bucket& operator=(const Bucket&) = delete;
        Bucket& operator=(Bucket&&) = delete;

        partial_t partial(size_type ind) const {
            return partials_[ind];
        }

        const value_type& kvpair(size_type ind) const {
            return *static_cast<const value_type*>(
                static_cast<const void*>(std::addressof(kvpairs_[ind])));
        }

        value_type& kvpair(size_type ind) {
            return *static_cast<value_type*>(
                static_cast<void*>(std::addressof(kvpairs_[ind])));
        }

        storage_value_type& storage_kvpair(size_type ind) {
            return *static_cast<storage_value_type*>(
                static_cast<void*>(std::addressof(kvpairs_[ind])));
        }

        bool occupied(size_type ind) const {
            return occupied_[ind];
        }

        const key_type& key(size_type ind) const {
            return kvpair(ind).first;
        }

        const mapped_type& val(size_type ind) const {
            return kvpair(ind).second;
        }

        mapped_type& val(size_type ind) {
            return kvpair(ind).second;
        }

        template <typename K, typename... Args>
        void setKV(allocator_type& allocator, size_type ind, partial_t p,
                   K& k, Args&&... args) {
            partials_[ind] = p;
            occupied_[ind] = true;
            allocator_traits_::construct(
                allocator, &storage_kvpair(ind), std::piecewise_construct,
                std::forward_as_tuple(std::move(k)),
                std::forward_as_tuple(std::forward<Args>(args)...));
        }

        void eraseKV(allocator_type& allocator, size_type ind) {
            occupied_[ind] = false;
            allocator_traits_::destroy(
                allocator, std::addressof(storage_kvpair(ind)));
        }

        void clear(allocator_type& allocator) {
            for (size_type i = 0; i < slot_per_bucket(); ++i) {
                if (occupied(i)) {
                    eraseKV(allocator, i);
                }
            }
        }

        // Moves the item in b1[slot1] into b2[slot2] without copying
        static void move_to_bucket(allocator_type& allocator,
                                   Bucket& b1, size_type slot1,
                                   Bucket& b2, size_type slot2) {
            assert(b1.occupied(slot1));
            assert(!b2.occupied(slot2));
            storage_value_type& tomove = b1.storage_kvpair(slot1);
            b2.setKV(allocator, slot2, b1.partial(slot1),
                     tomove.first, std::move(tomove.second));
            b1.eraseKV(allocator, slot1);
        }

        // Moves the contents of b1 to b2
        static void move_bucket(allocator_type& allocator, Bucket& b1,
                                Bucket& b2) {
            for (size_type i = 0; i < slot_per_bucket(); ++i) {
                if (b1.occupied(i)) {
                    move_to_bucket(allocator, b1, i, b2, i);
                }
            }
        }

    private:
        std::array<partial_t, slot_per_bucket()> partials_;
        std::bitset<slot_per_bucket()> occupied_;
        std::array<typename std::aligned_storage<
                       sizeof(storage_value_type),
                       alignof(storage_value_type)>::type,
                   slot_per_bucket()> kvpairs_;
    };

    class BucketContainer {
        using traits_ = typename allocator_traits_::
            template rebind_traits<Bucket>;
    public:
        BucketContainer(size_type n, typename traits_::allocator_type alloc)
            : buckets_(traits_::allocate(allocator_, n)),
              allocator_(alloc), size_(n) {
            // The Bucket default constructor is nothrow, so we don't have to
            // worry about dealing with exceptions when constructing all the
            // elements.
            static_assert(
                std::is_nothrow_constructible<Bucket>::value,
                "BucketContainer requires Bucket to be nothrow constructible");
            for (size_type i = 0; i < size_; ++i) {
                traits_::construct(allocator_, &buckets_[i]);
            }
        }

        BucketContainer(const BucketContainer&) = delete;
        BucketContainer(BucketContainer&&) = delete;
        BucketContainer& operator=(const BucketContainer&) = delete;
        BucketContainer& operator=(BucketContainer&&) = delete;

        ~BucketContainer() noexcept {
            static_assert(
                std::is_nothrow_destructible<Bucket>::value,
                "BucketContainer requires Bucket to be nothrow destructible");
            for (size_type i = 0; i < size_; ++i) {
                traits_::destroy(allocator_, &buckets_[i]);
            }
            traits_::deallocate(allocator_, buckets_, size());
        }

        size_type size() const {
            return size_;
        }

        void swap(BucketContainer& other) noexcept {
            std::swap(buckets_, other.buckets_);
            // If propagate_container_on_swap is false, we do nothing if the
            // allocators are equal. If they're not equal, behavior is
            // undefined, so we can still do nothing.
            if (traits_::propagate_on_container_swap::value) {
                std::swap(allocator_, other.allocator_);
            }
            std::swap(size_, other.size_);
        }

        Bucket& operator[](size_type i) {
            return buckets_[i];
        }

        const Bucket& operator[](size_type i) const {
            return buckets_[i];
        }

    private:
        typename traits_::pointer buckets_;
        typename allocator_traits_::template rebind_alloc<Bucket> allocator_;
        size_type size_;
    };

    // The type of the buckets container
    using buckets_t = BucketContainer;

    // Status codes for internal functions

    enum cuckoo_status {
        ok,
        failure,
        failure_key_not_found,
        failure_key_duplicated,
        failure_table_full,
        failure_under_expansion,
    };


    // A composite type for functions that need to return a table position, and
    // a status code.
    struct table_position {
        size_type index;
        size_type slot;
        cuckoo_status status;
    };

    // Searching types and functions

    // cuckoo_find searches the table for the given key, returning the position
    // of the element found, or a failure status code if the key wasn't found.
    // It expects the locks to be taken and released outside the function.
    template <typename K>
    table_position cuckoo_find(const K &key, const partial_t partial,
                               const size_type i1, const size_type i2) const {
        int slot = try_read_from_bucket(buckets_[i1], partial, key);
        if (slot != -1) {
            return table_position{i1, static_cast<size_type>(slot), ok};
        }
        slot = try_read_from_bucket(buckets_[i2], partial, key);
        if (slot != -1) {
            return table_position{i2, static_cast<size_type>(slot), ok};
        }
        return table_position{0, 0, failure_key_not_found};
    }

    // try_read_from_bucket will search the bucket for the given key and return
    // the index of the slot if found, or -1 if not found.
    template <typename K>
    int try_read_from_bucket(const Bucket& b, const partial_t partial,
                             const K &key) const {
        // Silence a warning from MSVC about partial being unused if is_simple.
        (void)partial;
        for (size_type i = 0; i < slot_per_bucket(); ++i) {
            if (!b.occupied(i) || (!is_simple && partial != b.partial(i))) {
                continue;
            } else if (key_eq()(b.key(i), key)) {
                return i;
            }
        }
        return -1;
    }

    // Insertion types and function

    /**
     * Runs cuckoo_insert in a loop until it succeeds in insert and upsert, so
     * we pulled out the loop to avoid duplicating logic.
     *
     * @param hv the hash value of the key
     * @param b bucket locks
     * @param key the key to insert
     * @return table_position of the location to insert the new element, or the
     * site of the duplicate element with a status code if there was a duplicate.
     * In either case, the locks will still be held after the function ends.
     * @throw libcuckoo_load_factor_too_low if expansion is necessary, but the
     * load factor of the table is below the threshold
     */
    template <typename K, typename LOCK_T>
    table_position cuckoo_insert_loop(hash_value hv, TwoBuckets<LOCK_T>& b,
                                      K& key) {
        table_position pos;
        while (true) {
            assert(b.is_active());
            const size_type hp = hashpower();
            pos = cuckoo_insert(hv, b, key);
            switch (pos.status) {
            case ok:
            case failure_key_duplicated:
                return pos;
            case failure_table_full:
                // Expand the table and try again, re-grabbing the locks
                cuckoo_fast_double<LOCK_T, automatic_resize>(hp);
            case failure_under_expansion:
                b = snapshot_and_lock_two<LOCK_T>(hv);
                break;
            default:
                assert(false);
            }
        }
    }

    // cuckoo_insert tries to find an empty slot in either of the buckets to
    // insert the given key into, performing cuckoo hashing if necessary. It
    // expects the locks to be taken outside the function. Before inserting, it
    // checks that the key isn't already in the table. cuckoo hashing presents
    // multiple concurrency issues, which are explained in the function. The
    // following return states are possible:
    //
    // ok -- Found an empty slot, locks will be held on both buckets after the
    // function ends, and the position of the empty slot is returned
    //
    // failure_key_duplicated -- Found a duplicate key, locks will be held, and
    // the position of the duplicate key will be returned
    //
    // failure_under_expansion -- Failed due to a concurrent expansion
    // operation. Locks are released. No meaningful position is returned.
    //
    // failure_table_full -- Failed to find an empty slot for the table. Locks
    // are released. No meaningful position is returned.
    template <typename K, typename LOCK_T>
    table_position cuckoo_insert(const hash_value hv, TwoBuckets<LOCK_T>& b,
                                 K& key) {
        int res1, res2;
        Bucket& b1 = buckets_[b.first()];
        if (!try_find_insert_bucket(b1, res1, hv.partial, key)) {
            return table_position{b.first(), static_cast<size_type>(res1),
                    failure_key_duplicated};
        }
        Bucket& b2 = buckets_[b.second()];
        if (!try_find_insert_bucket(b2, res2, hv.partial, key)) {
            return table_position{b.second(), static_cast<size_type>(res2),
                    failure_key_duplicated};
        }
        if (res1 != -1) {
            return table_position{b.first(), static_cast<size_type>(res1), ok};
        }
        if (res2 != -1) {
            return table_position{b.second(), static_cast<size_type>(res2), ok};
        }

        // We are unlucky, so let's perform cuckoo hashing.
        size_type insert_bucket = 0;
        size_type insert_slot = 0;
        cuckoo_status st = run_cuckoo<LOCK_T>(b, insert_bucket, insert_slot);
        if (st == failure_under_expansion) {
            // The run_cuckoo operation operated on an old version of the table,
            // so we have to try again. We signal to the calling insert method
            // to try again by returning failure_under_expansion.
            return table_position{0, 0, failure_under_expansion};
        } else if (st == ok) {
            assert(!locks_[lock_ind(b.first())].try_lock(LOCK_T()));
            assert(!locks_[lock_ind(b.second())].try_lock(LOCK_T()));
            assert(!buckets_[insert_bucket].occupied(insert_slot));
            assert(insert_bucket == index_hash(hashpower(), hv.hash) ||
                   insert_bucket == alt_index(
                       hashpower(), hv.partial,
                       index_hash(hashpower(), hv.hash)));
            // Since we unlocked the buckets during run_cuckoo, another insert
            // could have inserted the same key into either b.first() or
            // b.second(), so we check for that before doing the insert.
            table_position pos = cuckoo_find(
                key, hv.partial, b.first(), b.second());
            if (pos.status == ok) {
                pos.status = failure_key_duplicated;
                return pos;
            }
            return table_position{insert_bucket, insert_slot, ok};
        }
        assert(st == failure);
        LIBCUCKOO_DBG("hash table is full (hashpower = %zu, hash_items = %zu,"
                      "load factor = %.2f), need to increase hashpower\n",
                      hashpower(), size(), load_factor());
        return table_position{0, 0, failure_table_full};
    }

    // add_to_bucket will insert the given key-value pair into the slot. The key
    // and value will be move-constructed into the table, so they are not valid
    // for use afterwards.
    template <typename K, typename... Args>
    void add_to_bucket(const size_type bucket_ind, const size_type slot,
                       const partial_t partial, K& key, Args&&... val) {
        Bucket& b = buckets_[bucket_ind];
        assert(!b.occupied(slot));
        b.setKV(allocator_, slot, partial,
                key, std::forward<Args>(val)...);
        ++locks_[lock_ind(bucket_ind)].elem_counter();
    }

    // try_find_insert_bucket will search the bucket for the given key, and for
    // an empty slot. If the key is found, we store the slot of the key in
    // `slot` and return false. If we find an empty slot, we store its position
    // in `slot` and return true. If no duplicate key is found and no empty slot
    // is found, we store -1 in `slot` and return true.
    template <typename K>
    bool try_find_insert_bucket(const Bucket& b, int& slot,
                                const partial_t partial, const K &key) const {
        // Silence a warning from MSVC about partial being unused if is_simple.
        (void)partial;
        slot = -1;
        for (size_type i = 0; i < slot_per_bucket(); ++i) {
            if (b.occupied(i)) {
                if (!is_simple && partial != b.partial(i)) {
                    continue;
                }
                if (key_eq()(b.key(i), key)) {
                    slot = i;
                    return false;
                }
            } else {
                slot = i;
            }
        }
        return true;
    }

    // CuckooRecord holds one position in a cuckoo path. Since cuckoopath
    // elements only define a sequence of alternate hashings for different hash
    // values, we only need to keep track of the hash values being moved, rather
    // than the keys themselves.
    typedef struct {
        size_type bucket;
        size_type slot;
        hash_value hv;
    } CuckooRecord;

    // The maximum number of items in a cuckoo BFS path.
    static constexpr uint8_t MAX_BFS_PATH_LEN = 5;

    // An array of CuckooRecords
    using CuckooRecords = std::array<CuckooRecord, MAX_BFS_PATH_LEN>;

    // run_cuckoo performs cuckoo hashing on the table in an attempt to free up
    // a slot on either of the insert buckets, which are assumed to be locked
    // before the start. On success, the bucket and slot that was freed up is
    // stored in insert_bucket and insert_slot. In order to perform the search
    // and the swaps, it has to release the locks, which can lead to certain
    // concurrency issues, the details of which are explained in the function.
    // If run_cuckoo returns ok (success), then `b` will be active, otherwise it
    // will not.
    template <typename LOCK_T>
    cuckoo_status run_cuckoo(TwoBuckets<LOCK_T>& b, size_type &insert_bucket,
                             size_type &insert_slot) {
        // We must unlock the buckets here, so that cuckoopath_search and
        // cuckoopath_move can lock buckets as desired without deadlock.
        // cuckoopath_move has to move something out of one of the original
        // buckets as its last operation, and it will lock both buckets and
        // leave them locked after finishing. This way, we know that if
        // cuckoopath_move succeeds, then the buckets needed for insertion are
        // still locked. If cuckoopath_move fails, the buckets are unlocked and
        // we try again. This unlocking does present two problems. The first is
        // that another insert on the same key runs and, finding that the key
        // isn't in the table, inserts the key into the table. Then we insert
        // the key into the table, causing a duplication. To check for this, we
        // search the buckets for the key we are trying to insert before doing
        // so (this is done in cuckoo_insert, and requires that both buckets are
        // locked). Another problem is that an expansion runs and changes the
        // hashpower, meaning the buckets may not be valid anymore. In this
        // case, the cuckoopath functions will have thrown a hashpower_changed
        // exception, which we catch and handle here.
        size_type hp = hashpower();
        b.unlock();
        CuckooRecords cuckoo_path;
        bool done = false;
        try {
            while (!done) {
                const int depth = cuckoopath_search<LOCK_T>(
                    hp, cuckoo_path, b.first(), b.second());
                if (depth < 0) {
                    break;
                }

                if (cuckoopath_move(hp, cuckoo_path, depth, b)) {
                    insert_bucket = cuckoo_path[0].bucket;
                    insert_slot = cuckoo_path[0].slot;
                    assert(insert_bucket == b.first() || insert_bucket == b.second());
                    assert(!locks_[lock_ind(b.first())].try_lock(LOCK_T()));
                    assert(!locks_[lock_ind(b.second())].try_lock(LOCK_T()));
                    assert(!buckets_[insert_bucket].occupied(insert_slot));
                    done = true;
                    break;
                }
            }
        } catch (hashpower_changed&) {
            // The hashpower changed while we were trying to cuckoo, which means
            // we want to retry. b.first() and b.second() should not be locked
            // in this case.
            return failure_under_expansion;
        }
        return done ? ok : failure;
    }

    // cuckoopath_search finds a cuckoo path from one of the starting buckets to
    // an empty slot in another bucket. It returns the depth of the discovered
    // cuckoo path on success, and -1 on failure. Since it doesn't take locks on
    // the buckets it searches, the data can change between this function and
    // cuckoopath_move. Thus cuckoopath_move checks that the data matches the
    // cuckoo path before changing it.
    //
    // throws hashpower_changed if it changed during the search.
    template <typename LOCK_T>
    int cuckoopath_search(const size_type hp,
                          CuckooRecords& cuckoo_path,
                          const size_type i1, const size_type i2) {
        b_slot x = slot_search<LOCK_T>(hp, i1, i2);
        if (x.depth == -1) {
            return -1;
        }
        // Fill in the cuckoo path slots from the end to the beginning.
        for (int i = x.depth; i >= 0; i--) {
            cuckoo_path[i].slot = x.pathcode % slot_per_bucket();
            x.pathcode /= slot_per_bucket();
        }
        // Fill in the cuckoo_path buckets and keys from the beginning to the
        // end, using the final pathcode to figure out which bucket the path
        // starts on. Since data could have been modified between slot_search
        // and the computation of the cuckoo path, this could be an invalid
        // cuckoo_path.
        CuckooRecord& first = cuckoo_path[0];
        if (x.pathcode == 0) {
            first.bucket = i1;
        } else {
            assert(x.pathcode == 1);
            first.bucket = i2;
        }
        {
            const auto ob = lock_one<LOCK_T>(hp, first.bucket);
            const Bucket& b = buckets_[first.bucket];
            if (!b.occupied(first.slot)) {
                // We can terminate here
                return 0;
            }
            first.hv = hashed_key(b.key(first.slot));
        }
        for (int i = 1; i <= x.depth; ++i) {
            CuckooRecord& curr = cuckoo_path[i];
            const CuckooRecord& prev = cuckoo_path[i-1];
            assert(prev.bucket == index_hash(hp, prev.hv.hash) ||
                   prev.bucket == alt_index(hp, prev.hv.partial,
                                            index_hash(hp, prev.hv.hash)));
            // We get the bucket that this slot is on by computing the alternate
            // index of the previous bucket
            curr.bucket = alt_index(hp, prev.hv.partial, prev.bucket);
            const auto ob = lock_one<LOCK_T>(hp, curr.bucket);
            const Bucket& b = buckets_[curr.bucket];
            if (!b.occupied(curr.slot)) {
                // We can terminate here
                return i;
            }
            curr.hv = hashed_key(b.key(curr.slot));
        }
        return x.depth;
    }

    // cuckoopath_move moves keys along the given cuckoo path in order to make
    // an empty slot in one of the buckets in cuckoo_insert. Before the start of
    // this function, the two insert-locked buckets were unlocked in run_cuckoo.
    // At the end of the function, if the function returns true (success), then
    // both insert-locked buckets remain locked. If the function is
    // unsuccessful, then both insert-locked buckets will be unlocked.
    //
    // throws hashpower_changed if it changed during the move.
    template <typename LOCK_T>
    bool cuckoopath_move(const size_type hp, CuckooRecords& cuckoo_path,
                         size_type depth, TwoBuckets<LOCK_T>& b) {
        assert(!b.is_active());
        if (depth == 0) {
            // There is a chance that depth == 0, when try_add_to_bucket sees
            // both buckets as full and cuckoopath_search finds one empty. In
            // this case, we lock both buckets. If the slot that
            // cuckoopath_search found empty isn't empty anymore, we unlock them
            // and return false. Otherwise, the bucket is empty and insertable,
            // so we hold the locks and return true.
            const size_type bucket = cuckoo_path[0].bucket;
            assert(bucket == b.first() || bucket == b.second());
            b = lock_two<LOCK_T>(hp, b.first(), b.second());
            if (!buckets_[bucket].occupied(cuckoo_path[0].slot)) {
                return true;
            } else {
                b.unlock();
                return false;
            }
        }

        while (depth > 0) {
            CuckooRecord& from = cuckoo_path[depth-1];
            CuckooRecord& to   = cuckoo_path[depth];
            const size_type fs = from.slot;
            const size_type ts = to.slot;
            TwoBuckets<LOCK_T> twob;
            OneBucket<LOCK_T> extrab;
            if (depth == 1) {
                // Even though we are only swapping out of one of the original
                // buckets, we have to lock both of them along with the slot we
                // are swapping to, since at the end of this function, they both
                // must be locked. We store tb inside the extrab container so it
                // is unlocked at the end of the loop.
                std::tie(twob, extrab) = lock_three<LOCK_T>(
                    hp, b.first(), b.second(), to.bucket);
            } else {
                twob = lock_two<LOCK_T>(hp, from.bucket, to.bucket);
            }

            Bucket& fb = buckets_[from.bucket];
            Bucket& tb = buckets_[to.bucket];

            // We plan to kick out fs, but let's check if it is still there;
            // there's a small chance we've gotten scooped by a later cuckoo. If
            // that happened, just... try again. Also the slot we are filling in
            // may have already been filled in by another thread, or the slot we
            // are moving from may be empty, both of which invalidate the swap.
            // We only need to check that the hash value is the same, because,
            // even if the keys are different and have the same hash value, then
            // the cuckoopath is still valid.
            if (hashed_key_only_hash(fb.key(fs)) != from.hv.hash ||
                tb.occupied(ts) || !fb.occupied(fs)) {
                return false;
            }

            Bucket::move_to_bucket(allocator_, fb, fs, tb, ts);
            if (depth == 1) {
                // Hold onto the locks contained in twob
                b = std::move(twob);
            }
            depth--;
        }
        return true;
    }

    // A constexpr version of pow that we can use for static_asserts
    static constexpr size_type const_pow(size_type a, size_type b) {
        return (b == 0) ? 1 : a * const_pow(a, b - 1);
    }

    // b_slot holds the information for a BFS path through the table.
    #pragma pack(push, 1)
    struct b_slot {
        // The bucket of the last item in the path.
        size_type bucket;
        // a compressed representation of the slots for each of the buckets in
        // the path. pathcode is sort of like a base-slot_per_bucket number, and
        // we need to hold at most MAX_BFS_PATH_LEN slots. Thus we need the
        // maximum pathcode to be at least slot_per_bucket()^(MAX_BFS_PATH_LEN).
        size_type pathcode;
        static_assert(const_pow(slot_per_bucket(), MAX_BFS_PATH_LEN) <
                      std::numeric_limits<decltype(pathcode)>::max(),
                      "pathcode may not be large enough to encode a cuckoo "
                      "path");
        // The 0-indexed position in the cuckoo path this slot occupies. It must
        // be less than MAX_BFS_PATH_LEN, and also able to hold negative values.
        int_fast8_t depth;
        static_assert(MAX_BFS_PATH_LEN - 1 <=
                      std::numeric_limits<decltype(depth)>::max(),
                      "The depth type must able to hold a value of"
                      " MAX_BFS_PATH_LEN - 1");
        static_assert(-1 >= std::numeric_limits<decltype(depth)>::min(),
                      "The depth type must be able to hold a value of -1");
        b_slot() {}
        b_slot(const size_type b, const size_type p, const decltype(depth) d)
            : bucket(b), pathcode(p), depth(d) {
            assert(d < MAX_BFS_PATH_LEN);
        }
    };
    #pragma pack(pop)

    // b_queue is the queue used to store b_slots for BFS cuckoo hashing.
    #pragma pack(push, 1)
    class b_queue {
    public:
        b_queue() noexcept : first_(0), last_(0) {}

        void enqueue(b_slot x) {
            assert(!full());
            slots_[last_] = x;
            last_ = increment(last_);
        }

        b_slot dequeue() {
            assert(!empty());
            b_slot& x = slots_[first_];
            first_ = increment(first_);
            return x;
        }

        bool empty() const {
            return first_ == last_;
        }

        bool full() const {
            return increment(last_) == first_;
        }

    private:
        // The maximum size of the BFS queue. Note that unless it's less than
        // slot_per_bucket()^MAX_BFS_PATH_LEN, it won't really mean anything.
        static constexpr size_type MAX_CUCKOO_COUNT = 256;
        static_assert((MAX_CUCKOO_COUNT & (MAX_CUCKOO_COUNT - 1)) == 0,
                      "MAX_CUCKOO_COUNT should be a power of 2");
        // A circular array of b_slots
        b_slot slots_[MAX_CUCKOO_COUNT];
        // The index of the head of the queue in the array
        size_type first_;
        // One past the index of the last_ item of the queue in the array.
        size_type last_;

        // returns the index in the queue after ind, wrapping around if
        // necessary.
        size_type increment(size_type ind) const {
            return (ind + 1) & (MAX_CUCKOO_COUNT - 1);
        }
    };
    #pragma pack(pop)

    // slot_search searches for a cuckoo path using breadth-first search. It
    // starts with the i1 and i2 buckets, and, until it finds a bucket with an
    // empty slot, adds each slot of the bucket in the b_slot. If the queue runs
    // out of space, it fails.
    //
    // throws hashpower_changed if it changed during the search
    template <typename LOCK_T>
    b_slot slot_search(const size_type hp, const size_type i1,
                       const size_type i2) {
        b_queue q;
        // The initial pathcode informs cuckoopath_search which bucket the path
        // starts on
        q.enqueue(b_slot(i1, 0, 0));
        q.enqueue(b_slot(i2, 1, 0));
        while (!q.full() && !q.empty()) {
            b_slot x = q.dequeue();
            // Picks a (sort-of) random slot to start from
            size_type starting_slot = x.pathcode % slot_per_bucket();
            for (size_type i = 0; i < slot_per_bucket() && !q.full();
                 ++i) {
                size_type slot = (starting_slot + i) % slot_per_bucket();
                auto ob = lock_one<LOCK_T>(hp, x.bucket);
                Bucket& b = buckets_[x.bucket];
                if (!b.occupied(slot)) {
                    // We can terminate the search here
                    x.pathcode = x.pathcode * slot_per_bucket() + slot;
                    return x;
                }

                // If x has less than the maximum number of path components,
                // create a new b_slot item, that represents the bucket we would
                // have come from if we kicked out the item at this slot.
                const partial_t partial = b.partial(slot);
                if (x.depth < MAX_BFS_PATH_LEN - 1) {
                    b_slot y(alt_index(hp, partial, x.bucket),
                             x.pathcode * slot_per_bucket() + slot, x.depth+1);
                    q.enqueue(y);
                }
            }
        }
        // We didn't find a short-enough cuckoo path, so the queue ran out of
        // space. Return a failure value.
        return b_slot(0, 0, -1);
    }

    // cuckoo_fast_double will double the size of the table by taking advantage
    // of the properties of index_hash and alt_index. If the key's move
    // constructor is not noexcept, we use cuckoo_expand_simple, since that
    // provides a strong exception guarantee.
    template <typename LOCK_T, typename AUTO_RESIZE>
    cuckoo_status cuckoo_fast_double(size_type current_hp) {
        if (!std::is_nothrow_move_constructible<storage_value_type>::value) {
            LIBCUCKOO_DBG("%s", "cannot run cuckoo_fast_double because kv-pair "
                          "is not nothrow move constructible");
            return cuckoo_expand_simple<LOCK_T, AUTO_RESIZE>(current_hp + 1);
        }
        const size_type new_hp = current_hp + 1;
        std::lock_guard<expansion_lock_t> l(expansion_lock_);
        cuckoo_status st = check_resize_validity<AUTO_RESIZE>(current_hp, new_hp);
        if (st != ok) {
            return st;
        }

        locks_.resize(hashsize(new_hp));
        auto unlocker = snapshot_and_lock_all<LOCK_T>();
        // We can't just resize, since the Bucket is non-copyable and
        // non-movable. Instead, we allocate a new array of buckets, and move
        // the contents of each bucket manually.
        {
            buckets_t new_buckets(buckets_.size() * 2, get_allocator());
            for (size_type i = 0; i < buckets_.size(); ++i) {
                Bucket::move_bucket(allocator_, buckets_[i], new_buckets[i]);
            }
            buckets_.swap(new_buckets);
        }
        set_hashpower(new_hp);

        // We gradually unlock the new table, by processing each of the buckets
        // corresponding to each lock we took. For each slot in an old bucket,
        // we either leave it in the old bucket, or move it to the corresponding
        // new bucket. After we're done with the bucket, we release the lock on
        // it and the new bucket, letting other threads using the new map
        // gradually. We only unlock the locks being used by the old table,
        // because unlocking new locks would enable operations on the table
        // before we want them. We also re-evaluate the partial key stored at
        // each slot, since it depends on the hashpower.
        const size_type locks_to_move = std::min(
            locks_.size(), hashsize(current_hp));
        parallel_exec(0, locks_to_move,
                      [this, current_hp, new_hp]
                      (size_type start, size_type end, std::exception_ptr& eptr) {
                          try {
                              move_buckets<LOCK_T>(current_hp, new_hp, start, end);
                          } catch (...) {
                              eptr = std::current_exception();
                          }
                      });
        parallel_exec(locks_to_move, locks_.size(),
                      [this](size_type i, size_type end, std::exception_ptr&) {
                          for (; i < end; ++i) {
                              locks_[i].unlock(LOCK_T());
                          }
                      });
        // Since we've unlocked the buckets ourselves, we don't need the
        // unlocker to do it for us.
        unlocker.release();
        return ok;
    }

    template <typename LOCK_T>
    void move_buckets(size_type current_hp, size_type new_hp,
                      size_type start_lock_ind, size_type end_lock_ind) {
        for (; start_lock_ind < end_lock_ind; ++start_lock_ind) {
            for (size_type bucket_i = start_lock_ind;
                 bucket_i < hashsize(current_hp);
                 bucket_i += locks_t::max_size()) {
                // By doubling the table size, the index_hash and alt_index of
                // each key got one bit added to the top, at position
                // current_hp, which means anything we have to move will either
                // be at the same bucket position, or exactly
                // hashsize(current_hp) later than the current bucket
                Bucket& old_bucket = buckets_[bucket_i];
                const size_type new_bucket_i = bucket_i + hashsize(current_hp);
                Bucket& new_bucket = buckets_[new_bucket_i];
                size_type new_bucket_slot = 0;

                // Move each item from the old bucket that needs moving into the
                // new bucket
                for (size_type slot = 0; slot < slot_per_bucket(); ++slot) {
                    if (!old_bucket.occupied(slot)) {
                        continue;
                    }
                    const hash_value hv = hashed_key(old_bucket.key(slot));
                    const size_type old_ihash = index_hash(current_hp, hv.hash);
                    const size_type old_ahash = alt_index(
                        current_hp, hv.partial, old_ihash);
                    const size_type new_ihash = index_hash(new_hp, hv.hash);
                    const size_type new_ahash = alt_index(
                        new_hp, hv.partial, new_ihash);
                    if ((bucket_i == old_ihash && new_ihash == new_bucket_i) ||
                        (bucket_i == old_ahash && new_ahash == new_bucket_i)) {
                        // We're moving the key from the old bucket to the new
                        // one
                        Bucket::move_to_bucket(
                            allocator_,
                            old_bucket, slot, new_bucket, new_bucket_slot++);
                        // Also update the lock counts, in case we're moving to
                        // a different lock.
                        --locks_[lock_ind(bucket_i)].elem_counter();
                        ++locks_[lock_ind(new_bucket_i)].elem_counter();
                    } else {
                        // Check that we don't want to move the new key
                        assert(
                            (bucket_i == old_ihash && new_ihash == old_ihash) ||
                            (bucket_i == old_ahash && new_ahash == old_ahash));
                    }
                }
            }
            // Now we can unlock the lock, because all the buckets corresponding
            // to it have been unlocked
            locks_[start_lock_ind].unlock(LOCK_T());
        }
    }

    // Checks whether the resize is okay to proceed. Returns a status code, or
    // throws an exception, depending on the error type.
    using automatic_resize = std::integral_constant<bool, true>;
    using manual_resize = std::integral_constant<bool, false>;

    template <typename AUTO_RESIZE>
    cuckoo_status check_resize_validity(const size_type orig_hp,
                                        const size_type new_hp) {
        const size_type mhp = maximum_hashpower();
        if (mhp != LIBCUCKOO_NO_MAXIMUM_HASHPOWER && new_hp > mhp) {
            throw libcuckoo_maximum_hashpower_exceeded(new_hp);
        }
        if (AUTO_RESIZE::value && load_factor() < minimum_load_factor()) {
            throw libcuckoo_load_factor_too_low(minimum_load_factor());
        }
        if (hashpower() != orig_hp) {
            // Most likely another expansion ran before this one could grab the
            // locks
            LIBCUCKOO_DBG("%s", "another expansion is on-going\n");
            return failure_under_expansion;
        }
        return ok;
    }

    // cuckoo_expand_simple will resize the table to at least the given
    // new_hashpower. When we're shrinking the table, if the current table
    // contains more elements than can be held by new_hashpower, the resulting
    // hashpower will be greater than new_hashpower. It needs to take all the
    // bucket locks, since no other operations can change the table during
    // expansion. Throws libcuckoo_maximum_hashpower_exceeded if we're expanding
    // beyond the maximum hashpower, and we have an actual limit.
    template <typename LOCK_T, typename AUTO_RESIZE>
    cuckoo_status cuckoo_expand_simple(size_type new_hp) {
        const auto unlocker = snapshot_and_lock_all<LOCK_T>();
        const size_type hp = hashpower();
        cuckoo_status st = check_resize_validity<AUTO_RESIZE>(hp, new_hp);
        if (st != ok) {
            return st;
        }
        // Creates a new hash table with hashpower new_hp and adds all
        // the elements from the old buckets.
        cuckoohash_map new_map(
            hashsize(new_hp) * slot_per_bucket(),
            hash_function(),
            key_eq(),
            get_allocator());

        parallel_exec(
            0, hashsize(hp),
            [this, &new_map]
            (size_type i, size_type end, std::exception_ptr& eptr) {
                try {
                    for (; i < end; ++i) {
                        for (size_type j = 0; j < slot_per_bucket(); ++j) {
                            if (buckets_[i].occupied(j)) {
                                storage_value_type& kvpair = (
                                    buckets_[i].storage_kvpair(j));
                                new_map.insert(kvpair.first,
                                               std::move(kvpair.second));
                            }
                        }
                    }
                } catch (...) {
                    eptr = std::current_exception();
                }
            });

        // Swap the current buckets vector with new_map's and set the hashpower.
        // This is okay, because we have all the locks, so nobody else should be
        // reading from the buckets array. Then the old buckets array will be
        // deleted when new_map is deleted. All the locks should be released by
        // the unlocker as well.
        buckets_.swap(new_map.buckets_);
        set_hashpower(new_map.hashpower_);
        return ok;
    }

    // Executes the function over the given range split over num_threads threads
    template <typename F>
    static void parallel_exec(size_type start, size_type end, F func) {
        static const size_type num_threads = (
            std::thread::hardware_concurrency() == 0 ?
            1 : std::thread::hardware_concurrency());
        size_type work_per_thread = (end - start) / num_threads;
        std::vector<std::thread, typename allocator_traits_::
        template rebind_alloc<std::thread> > threads(num_threads);
        std::vector<std::exception_ptr, typename allocator_traits_::
        template rebind_alloc<std::exception_ptr>> eptrs(num_threads, nullptr);
        for (size_type i = 0; i < num_threads - 1; ++i) {
            threads[i] = std::thread(func, start, start + work_per_thread,
                                     std::ref(eptrs[i]));
            start += work_per_thread;
        }
        threads.back() = std::thread(func, start, end, std::ref(eptrs.back()));
        for (std::thread& t : threads) {
            t.join();
        }
        for (std::exception_ptr& eptr : eptrs) {
            if (eptr) {
                std::rethrow_exception(eptr);
            }
        }
    }

    // Deletion functions

    // Removes an item from a bucket, decrementing the associated counter as
    // well.
    void del_from_bucket(Bucket& b, const size_type bucket_ind,
                         const size_type slot) {
        b.eraseKV(allocator_, slot);
        --locks_[lock_ind(bucket_ind)].elem_counter();
    }

    // Empties the table, calling the destructors of all the elements it removes
    // from the table. It assumes the locks are taken as necessary.
    cuckoo_status cuckoo_clear() {
        for (size_type i = 0; i < buckets_.size(); ++i) {
            buckets_[i].clear(allocator_);
        }
        for (size_type i = 0; i < locks_.size(); ++i) {
            locks_[i].elem_counter() = 0;
        }
        return ok;
    }

    // Rehashing functions

    template <typename LOCK_T>
    bool cuckoo_rehash(size_type n) {
        const size_type hp = hashpower();
        if (n == hp) {
            return false;
        }
        return cuckoo_expand_simple<LOCK_T, manual_resize>(n) == ok;
    }

    template <typename LOCK_T>
    bool cuckoo_reserve(size_type n) {
        const size_type hp = hashpower();
        const size_type new_hp = reserve_calc(n);
        if (new_hp == hp) {
            return false;
        }
        return cuckoo_expand_simple<LOCK_T, manual_resize>(new_hp) == ok;
    }

    // Miscellaneous functions

    void set_hashpower(size_type val) {
        hashpower_.store(val, std::memory_order_release);
    }

    // reserve_calc takes in a parameter specifying a certain number of slots
    // for a table and returns the smallest hashpower that will hold n elements.
    static size_type reserve_calc(const size_type n) {
        const size_type buckets = (n + slot_per_bucket() - 1) / slot_per_bucket();
        size_type blog2;
        for (blog2 = 1; (1UL << blog2) < buckets; ++blog2);
        assert(n <= hashsize(blog2) * slot_per_bucket());
        return blog2;
    }

    // This class is a friend for unit testing
    friend class UnitTestInternalAccess;

    // Member variables

    // 2**hashpower is the number of buckets. This cannot be changed unless all
    // the locks are taken on the table. Since it is still read and written by
    // multiple threads not necessarily synchronized by a lock, we keep it
    // atomic
    std::atomic<size_type> hashpower_;

    // The hash function
    hasher hash_fn_;

    // The equality function
    key_equal eq_fn_;

    // The allocator
    allocator_type allocator_;

    // vector of buckets. The size or memory location of the buckets cannot be
    // changed unless al the locks are taken on the table. Thus, it is only safe
    // to access the buckets_ vector when you have at least one lock held.
    buckets_t buckets_;

    // array of locks. marked mutable, so that const methods can take locks.
    // Even though it's a vector, it should not ever change in size after the
    // initial allocation.
    mutable locks_t locks_;

    // a lock to synchronize expansions
    expansion_lock_t expansion_lock_;

    // stores the minimum load factor allowed for automatic expansions. Whenever
    // an automatic expansion is triggered (during an insertion where cuckoo
    // hashing fails, for example), we check the load factor against this
    // double, and throw an exception if it's lower than this value. It can be
    // used to signal when the hash function is bad or the input adversarial.
    std::atomic<double> minimum_load_factor_;

    // stores the maximum hashpower allowed for any expansions. If set to
    // NO_MAXIMUM_HASHPOWER, this limit will be disregarded.
    std::atomic<size_type> maximum_hashpower_;

public:
    /**
     * An ownership wrapper around a @ref cuckoohash_map table instance. When
     * given a table instance, it takes all the locks on the table, blocking all
     * outside operations on the table. Because the locked_table has unique
     * ownership of the table, it can provide a set of operations on the table
     * that aren't possible in a concurrent context.
     *
     * The locked_table interface is very similar to the STL unordered_map
     * interface, and for functions whose signatures correspond to unordered_map
     * methods, the behavior should be mostly the same.
     */
    class locked_table {
    public:
        /** @name Type Declarations */
        /**@{*/

        using key_type = cuckoohash_map::key_type;
        using mapped_type = cuckoohash_map::mapped_type;
        using value_type = cuckoohash_map::value_type;
        using size_type = cuckoohash_map::size_type;
        using difference_type = cuckoohash_map::difference_type;
        using hasher = cuckoohash_map::hasher;
        using key_equal = cuckoohash_map::key_equal;
        using allocator_type = cuckoohash_map::allocator_type;
        using reference = cuckoohash_map::reference;
        using const_reference = cuckoohash_map::const_reference;
        using pointer = cuckoohash_map::pointer;
        using const_pointer = cuckoohash_map::const_pointer;

        /**
         * A constant iterator over a @ref locked_table, which allows read-only
         * access to the elements of the table. It fulfills the
         * BidirectionalIterator concept.
         */
        class const_iterator {
        public:
            using difference_type = locked_table::difference_type;
            using value_type = locked_table::value_type;
            using pointer = locked_table::const_pointer;
            using reference = locked_table::const_reference;
            using iterator_category = std::bidirectional_iterator_tag;

            const_iterator() {}

            // Return true if the iterators are from the same locked table and
            // location, false otherwise.
            bool operator==(const const_iterator& it) const {
                return buckets_ == it.buckets_ &&
                    index_ == it.index_ && slot_ == it.slot_;
            }

            bool operator!=(const const_iterator& it) const {
                return !(operator==(it));
            }

            reference operator*() const {
                return (*buckets_)[index_].kvpair(slot_);
            }

            pointer operator->() const {
                return &(*buckets_)[index_].kvpair(slot_);
            }

            // Advance the iterator to the next item in the table, or to the end
            // of the table. Returns the iterator at its new position.
            const_iterator& operator++() {
                // Move forward until we get to a slot that is occupied, or we
                // get to the end
                ++slot_;
                for (; index_ < buckets_->size(); ++index_) {
                    for (; slot_ < slot_per_bucket(); ++slot_) {
                        if ((*buckets_)[index_].occupied(slot_)) {
                            return *this;
                        }
                    }
                    slot_ = 0;
                }
                assert(std::make_pair(index_, slot_) == end_pos(*buckets_));
                return *this;
            }

            // Advance the iterator to the next item in the table, or to the end
            // of the table. Returns the iterator at its old position.
            const_iterator operator++(int) {
                const_iterator old(*this);
                ++(*this);
                return old;
            }

            // Move the iterator back to the previous item in the table. Returns
            // the iterator at its new position.
            const_iterator& operator--() {
                // Move backward until we get to the beginning. Behavior is
                // undefined if we are iterating at the first element, so we can
                // assume we'll reach an element. This means we'll never reach
                // index_ == 0 and slot_ == 0.
                if (slot_ == 0) {
                    --index_;
                    slot_ = slot_per_bucket() - 1;
                } else {
                    --slot_;
                }
                while (!(*buckets_)[index_].occupied(slot_)) {
                    if (slot_ == 0) {
                        --index_;
                        slot_ = slot_per_bucket() - 1;
                    } else {
                        --slot_;
                    }
                }
                return *this;
            }

            //! Move the iterator back to the previous item in the table.
            //! Returns the iterator at its old position. Behavior is undefined
            //! if the iterator is at the beginning.
            const_iterator operator--(int) {
                const_iterator old(*this);
                --(*this);
                return old;
            }

        protected:
            // The buckets owned by the locked table being iterated over. Even
            // though const_iterator cannot modify the buckets, we don't mark
            // them const so that the mutable iterator can derive from this
            // class. Also, since iterators should be default constructible,
            // copyable, and movable, we have to make this a raw pointer type.
            buckets_t* buckets_;

            // The bucket index of the item being pointed to. For implementation
            // convenience, we let it take on negative values.
            size_type index_;

            // The slot in the bucket of the item being pointed to. For
            // implementation convenience, we let it take on negative values.
            size_type slot_;

            // Returns the position signifying the end of the table
            static std::pair<size_type, size_type>
            end_pos(const buckets_t& buckets) {
                return std::make_pair(buckets.size(), 0);
            }

            // The private constructor is used by locked_table to create
            // iterators from scratch. If the given index_-slot_ pair is at the
            // end of the table, or the given spot is occupied, stay. Otherwise,
            // step forward to the next data item, or to the end of the table.
            const_iterator(buckets_t& buckets, size_type index,
                           size_type slot) noexcept
                : buckets_(std::addressof(buckets)), index_(index), slot_(slot) {
                if (std::make_pair(index_, slot_) != end_pos(*buckets_) &&
                    !(*buckets_)[index_].occupied(slot_)) {
                    operator++();
                }
            }

            friend class locked_table;
        };

        /**
         * An iterator over a @ref locked_table, which allows read-write access
         * to elements of the table. It fulfills the BidirectionalIterator
         * concept.
         */
        class iterator : public const_iterator {
        public:
            using pointer = cuckoohash_map::pointer;
            using reference = cuckoohash_map::reference;

            iterator() {}

            bool operator==(const iterator& it) const {
                return const_iterator::operator==(it);
            }

            bool operator!=(const iterator& it) const {
                return const_iterator::operator!=(it);
            }

            using const_iterator::operator*;
            reference operator*() {
                return (*const_iterator::buckets_)[
                    const_iterator::index_].kvpair(const_iterator::slot_);
            }

            using const_iterator::operator->;
            pointer operator->() {
                return &(*const_iterator::buckets_)[
                    const_iterator::index_].kvpair(const_iterator::slot_);
            }

            iterator& operator++() {
                const_iterator::operator++();
                return *this;
            }

            iterator operator++(int) {
                iterator old(*this);
                const_iterator::operator++();
                return old;
            }

            iterator& operator--() {
                const_iterator::operator--();
                return *this;
            }

            iterator operator--(int) {
                iterator old(*this);
                const_iterator::operator--();
                return old;
            }

        private:
            iterator(buckets_t& buckets, size_type index, size_type slot) noexcept
                : const_iterator(buckets, index, slot) {}

            friend class locked_table;
        };

        /**@}*/

        /** @name Table Parameters */
        /**@{*/

        static constexpr size_type slot_per_bucket() {
            return cuckoohash_map::slot_per_bucket();
        }

        /**@}*/

        /** @name Constructors, Destructors, and Assignment */
        /**@{*/

        locked_table() = delete;
        locked_table(const locked_table&) = delete;
        locked_table& operator=(const locked_table&) = delete;

        locked_table(locked_table&& lt) noexcept
            : map_(std::move(lt.map_)),
              unlocker_(std::move(lt.unlocker_))
            {}

        locked_table& operator=(locked_table&& lt) noexcept {
            unlock();
            map_ = std::move(lt.map_);
            unlocker_ = std::move(lt.unlocker_);
            return *this;
        }

        /**
         * Unlocks the table, thereby freeing the locks on the table, but also
         * invalidating all iterators and table operations with this object. It
         * is idempotent.
         */
        void unlock() {
            unlocker_.unlock();
        }

        /**@}*/

        /** @name Table Details
         *
         * Methods for getting information about the table. Many are identical
         * to their @ref cuckoohash_map counterparts. Only new functions or
         * those with different behavior are documented.
         *
         */
        /**@{*/

        /**
         * Returns whether the locked table has ownership of the table
         *
         * @return true if it still has ownership, false otherwise
         */
        bool is_active() const {
            return unlocker_.is_active();
        }

        hasher hash_function() const {
            return map_.get().hash_function();
        }

        key_equal key_eq() const {
            return map_.get().key_eq();
        }

        allocator_type get_allocator() const {
            return map_.get().get_allocator();
        }

        size_type hashpower() const {
            return map_.get().hashpower();
        }

        size_type bucket_count() const {
            return map_.get().bucket_count();
        }

        bool empty() const {
            return map_.get().empty();
        }

        size_type size() const {
            return map_.get().size();
        }

        size_type capacity() const {
            return map_.get().capacity();
        }

        double load_factor() const {
            return map_.get().load_factor();
        }

        void minimum_load_factor(const double mlf) {
            map_.get().minimum_load_factor(mlf);
        }

        double minimum_load_factor() {
            return map_.get().minimum_load_factor();
        }

        void maximum_hashpower(size_type mhp) {
            map_.get().maximum_hashpower(mhp);
        }

        size_type maximum_hashpower() {
            return map_.get().maximum_hashpower();
        }

        /**@}*/

        /**@{*/
        /**
         * Returns an iterator to the beginning of the table. If the table is
         * empty, it will point past the end of the table.
         *
         * @return an iterator to the beginning of the table
         */

        iterator begin() {
            return iterator(map_.get().buckets_, 0, 0);
        }

        const_iterator begin() const {
            return const_iterator(map_.get().buckets_, 0, 0);
        }

        const_iterator cbegin() const {
            return begin();
        }

        /**@}*/

        /** @name Iterators */
        /**@{*/

        /**@{*/
        /**
         * Returns an iterator past the end of the table.
         *
         * @return an iterator past the end of the table
         */

        iterator end() {
            const auto end_pos = const_iterator::end_pos(map_.get().buckets_);
            return iterator(map_.get().buckets_,
                            static_cast<size_type>(end_pos.first),
                            static_cast<size_type>(end_pos.second));
        }

        const_iterator end() const {
            const auto end_pos = const_iterator::end_pos(map_.get().buckets_);
            return const_iterator(map_.get().buckets_,
                                  static_cast<size_type>(end_pos.first),
                                  static_cast<size_type>(end_pos.second));
        }

        const_iterator cend() const {
            return end();
        }

        /**@}*/

        /**@}*/

        /** @name Modifiers */
        /**@{*/

        void clear() {
            map_.get().cuckoo_clear();
        }

        /**
         * This behaves like the @c unordered_map::try_emplace method, but with
         * the same argument lifetime properties as @ref cuckoohash_map::insert.
         * It will always invalidate all iterators, due to the possibilities of
         * cuckoo hashing and expansion.
         */
        template <typename K, typename... Args>
        std::pair<iterator, bool> insert(K&& key, Args&&... val) {
            K k(std::forward<K>(key));
            hash_value hv = map_.get().hashed_key(k);
            auto b = map_.get().template snapshot_and_lock_two<locking_inactive>(hv);
            table_position pos = map_.get().cuckoo_insert_loop(hv, b, k);
            if (pos.status == ok) {
                map_.get().add_to_bucket(
                    pos.index, pos.slot, hv.partial, k,
                    std::forward<Args>(val)...);
            } else {
                assert(pos.status == failure_key_duplicated);
            }
            return std::make_pair(
                iterator(map_.get().buckets_, pos.index, pos.slot),
                pos.status == ok);
        }

        iterator erase(const_iterator pos) {
            map_.get().del_from_bucket(map_.get().buckets_[pos.index_],
                                       pos.index_,
                                       pos.slot_);
            return iterator(map_.get().buckets_, pos.index_, pos.slot_);
        }

        iterator erase(iterator pos) {
            map_.get().del_from_bucket(map_.get().buckets_[pos.index_],
                                       pos.index_,
                                       pos.slot_);
            return iterator(map_.get().buckets_, pos.index_, pos.slot_);
        }

        template <typename K>
        size_type erase(const K& key) {
            const hash_value hv = map_.get().hashed_key(key);
            const auto b = map_.get().
                template snapshot_and_lock_two<locking_inactive>(hv);
            const table_position pos = map_.get().cuckoo_find(
                key, hv.partial, b.first(), b.second());
            if (pos.status == ok) {
                map_.get().del_from_bucket(map_.get().buckets_[pos.index],
                                           pos.index, pos.slot);
                return 1;
            } else {
                return 0;
            }
        }

        /**@}*/

        /** @name Lookup */
        /**@{*/

        template <typename K>
        iterator find(const K& key) {
            const hash_value hv = map_.get().hashed_key(key);
            const auto b = map_.get().
                template snapshot_and_lock_two<locking_inactive>(hv);
            const table_position pos = map_.get().cuckoo_find(
                key, hv.partial, b.first(), b.second());
            if (pos.status == ok) {
                return iterator(map_.get().buckets_, pos.index, pos.slot);
            } else {
                return end();
            }
        }

        template <typename K>
        const_iterator find(const K& key) const {
            const hash_value hv = map_.get().hashed_key(key);
            const auto b = map_.get().
                template snapshot_and_lock_two<locking_inactive>(hv);
            const table_position pos = map_.get().cuckoo_find(
                key, hv.partial, b.first(), b.second());
            if (pos.status == ok) {
                return const_iterator(map_.get().buckets_, pos.index, pos.slot);
            } else {
                return end();
            }
        }

        template <typename K>
        mapped_type& at(const K& key) {
            auto it = find(key);
            if (it == end()) {
                throw std::out_of_range("key not found in table");
            } else {
                return it->second;
            }
        }

        template <typename K>
        const mapped_type& at(const K& key) const {
            auto it = find(key);
            if (it == end()) {
                throw std::out_of_range("key not found in table");
            } else {
                return it->second;
            }
        }

        /**
         * This function has the same lifetime properties as @ref
         * cuckoohash_map::insert, except that the value is default-constructed,
         * with no parameters, if it is not already in the table.
         */
        template <typename K>
        T& operator[](K&& key) {
            auto result = insert(std::forward<K>(key));
            return result.first->second;
        }

        template <typename K>
        size_type count(const K& key) const {
            const hash_value hv = map_.get().hashed_key(key);
            const auto b = map_.get().
                template snapshot_and_lock_two<locking_inactive>(hv);
            return map_.get().cuckoo_find(
                key, hv.partial, b.first(), b.second()).status == ok ? 1 : 0;
        }

        template <typename K>
        std::pair<iterator, iterator> equal_range(const K& key) {
            auto it = find(key);
            if (it == end()) {
                return std::make_pair(it, it);
            } else {
                auto start_it = it++;
                return std::make_pair(start_it, it);
            }
        }

        template <typename K>
        std::pair<const_iterator, const_iterator> equal_range(const K& key) const {
            auto it = find(key);
            if (it == end()) {
                return std::make_pair(it, it);
            } else {
                auto start_it = it++;
                return std::make_pair(start_it, it);
            }
        }

        /**@}*/

        /** @name Re-sizing */
        /**@{*/

        /**
         * This has the same behavior as @ref cuckoohash_map::rehash, except
         * that we don't return anything.
         */
        void rehash(size_type n) {
            map_.get().template cuckoo_rehash<locking_inactive>(n);
        }

        /**
         * This has the same behavior as @ref cuckoohash_map::reserve, except
         * that we don't return anything.
         */
        void reserve(size_type n) {
            map_.get().template cuckoo_reserve<locking_inactive>(n);
        }

        /**@}*/

        /** @name Comparison  */
        /**@{*/

        bool operator==(const locked_table& lt) const {
            if (size() != lt.size()) {
                return false;
            }
            for (const auto& elem : lt) {
                auto it = find(elem.first);
                if (it == end() || it->second != elem.second) {
                    return false;
                }
            }
            return true;
        }

        bool operator!=(const locked_table& lt) const {
            if (size() != lt.size()) {
                return true;
            }
            for (const auto& elem : lt) {
                auto it = find(elem.first);
                if (it == end() || it->second != elem.second) {
                    return true;
                }
            }
            return false;
        }

        /**@}*/

    private:
        // The constructor locks the entire table. We keep this constructor
        // private (but expose it to the cuckoohash_map class), since we don't
        // want users calling it.
        locked_table(cuckoohash_map& map) noexcept
            : map_(map), unlocker_(
                map_.get().template snapshot_and_lock_all<locking_active>())
            {}

        // A reference to the map owned by the table
        std::reference_wrapper<cuckoohash_map> map_;
        // A manager for all the locks we took on the table.
        AllBuckets<locking_active> unlocker_;

        friend class cuckoohash_map;
    };
};

#endif // _CUCKOOHASH_MAP_HH
