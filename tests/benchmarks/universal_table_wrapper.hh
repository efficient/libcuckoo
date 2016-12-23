#ifndef _UNIVERSAL_TABLE_WRAPPER_HH
#define _UNIVERSAL_TABLE_WRAPPER_HH

#include <utility>

/* A wrapper for all operations being benchmarked that can be specialized for
 * different tables. This un-specialized template implementation lists out all
 * the methods necessary to implement. Each method is static and returns true on
 * successful action and false on failure. */
template <typename T>
class TableWrapper {
    // bool read(T& tbl, const KEY& k, VALUE& v)
    // bool insert(const T& tbl, const KEY& k, const VALUE& v)
    // bool erase(T& tbl, const KEY& k)
    // bool update(T& tbl, const KEY& k, const VALUE& v)
    // bool upsert(T& tbl, const KEY& k, Updater fn, const VALUE& v)
};

#ifdef USE_LIBCUCKOO

#include "../../src/cuckoohash_map.hh"

template <typename Hash, typename Pred, typename Alloc,
          size_t SLOT_PER_BUCKET>
class TableWrapper< cuckoohash_map<KEY, VALUE, Hash, Pred,
                                   Alloc, SLOT_PER_BUCKET> > {
public:
    typedef cuckoohash_map<KEY, VALUE, Hash, Pred,
                           Alloc, SLOT_PER_BUCKET> tbl;
    static bool read(const tbl& tbl, const KEY& k, VALUE& v) {
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

#endif

#ifdef USE_TBB

#include <tbb/concurrent_hash_map.h>

template <typename HashCompare, typename A>
class TableWrapper<
    tbb::concurrent_hash_map<KEY, VALUE, HashCompare, A> > {
public:
    typedef tbb::concurrent_hash_map<KEY, VALUE, HashCompare, A> tbl;

    static bool read(const tbl& tbl, const KEY& k, VALUE& v) {
        static typename tbl::const_accessor a;
        if (tbl.find(a, k)) {
            v = a->second;
            return true;
        } else {
            return false;
        }
    }

    static bool insert(tbl& tbl, const KEY& k, const VALUE& v) {
        return tbl.insert(std::make_pair(k, v));
    }

    static bool erase(tbl& tbl, const KEY& k) {
        return tbl.erase(k);
    }

    static bool update(tbl& tbl, const KEY& k, const VALUE& v) {
        static typename tbl::accessor a;
        if (tbl.find(a, k)) {
            a->second = v;
            return true;
        } else {
            return false;
        }
    }

    template <typename Updater>
    static bool upsert(tbl& tbl, const KEY& k, Updater fn, const VALUE& v) {
        static typename tbl::accessor a;
        if (tbl.insert(a, k)) {
            a->second = v;
        } else {
            fn(a->second);
        }
        return true;
    }
};

#endif

#endif // _UNIVERSAL_TABLE_WRAPPER_HH
