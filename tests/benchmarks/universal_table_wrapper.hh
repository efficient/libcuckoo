#ifndef _UNIVERSAL_TABLE_WRAPPER_HH
#define _UNIVERSAL_TABLE_WRAPPER_HH

#include "../../src/cuckoohash_map.hh"

/* A wrapper for all operations being benchmarked that can be specialized for
 * different tables. This un-specialized template implementation lists out all
 * the methods necessary to implement. Each method is static and returns true on
 * successful action and false on failure. */
template <typename T>
class TableWrapper {
    // bool read(T& tbl, const KEY& k)
    // bool insert(const T& tbl, const KEY& k, const VALUE& v)
    // bool erase(T& tbl, const KEY& k)
    // bool update(T& tbl, const KEY& k, const VALUE& v)
    // bool upsert(T& tbl, const KEY& k, Updater fn, const VALUE& v)
};

template <typename Hash, typename Pred, typename Alloc,
          size_t SLOT_PER_BUCKET>
class TableWrapper< cuckoohash_map<KEY, VALUE, Hash, Pred,
                                   Alloc, SLOT_PER_BUCKET> > {
public:
    typedef cuckoohash_map<KEY, VALUE, Hash, Pred,
                           Alloc, SLOT_PER_BUCKET> tbl;
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

#endif // _UNIVERSAL_TABLE_WRAPPER_HH
