/* For each table to support, we define a wrapper class which holds the table
 * and implements all of the benchmarked operations. Below we list all the
 * methods each wrapper must implement.
 *
 * constructor(size_t n) // n is the initial capacity
 * bool read(T& tbl, const KEY& k, VALUE& v) const
 * bool insert(const T& tbl, const KEY& k, const VALUE& v)
 * bool erase(T& tbl, const KEY& k)
 * bool update(T& tbl, const KEY& k, const VALUE& v)
 * void upsert(T& tbl, const KEY& k, Updater fn, const VALUE& v)
 *
 */

#ifndef _UNIVERSAL_TABLE_WRAPPER_HH
#define _UNIVERSAL_TABLE_WRAPPER_HH

#include <utility>

#ifndef KEY
#error Must define KEY symbol as valid key type
#endif

#ifndef VALUE
#error Must define VALUE symbol as valid value type
#endif

#ifdef LIBCUCKOO
#define TABLE "LIBCUCKOO"
#define TABLE_TYPE "cuckoohash_map"
#include <libcuckoo/cuckoohash_map.hh>

class Table {
public:
    Table(size_t n) : tbl(n) {}

    bool read(const KEY& k, VALUE& v) const {
        return tbl.find(k, v);
    }

    bool insert(const KEY& k, const VALUE& v) {
        return tbl.insert(k, v);
    }

    bool erase(const KEY& k) {
        return tbl.erase(k);
    }

    bool update(const KEY& k, const VALUE& v) {
        return tbl.update(k, v);
    }

    template <typename Updater>
    void upsert(const KEY& k, Updater fn, const VALUE& v) {
        tbl.upsert(k, fn, v);
    }

private:
    cuckoohash_map<KEY, VALUE, std::hash<KEY> > tbl;
};

#else
#ifdef TBB
#define TABLE "TBB"
#define TABLE_TYPE "tbb::concurrent_hash_map"
#include <tbb/concurrent_hash_map.h>

struct CustomHashCompare {
    static size_t hash(const KEY& k) {
        return hashfn(k);
    }

    static bool equal(const KEY& k1, const KEY& k2) {
        return k1 == k2;
    }
private:
    static std::hash<KEY> hashfn;
};

std::hash<KEY> CustomHashCompare::hashfn;

class Table {
public:
    Table(size_t n): tbl(n) {}

    bool read(const KEY& k, VALUE& v) const {
        typename decltype(tbl)::const_accessor a;
        if (tbl.find(a, k)) {
            v = a->second;
            return true;
        } else {
            return false;
        }
    }

    bool insert(const KEY& k, const VALUE& v) {
        return tbl.insert(std::make_pair(k, v));
    }

    bool erase(const KEY& k) {
        return tbl.erase(k);
    }

    bool update(const KEY& k, const VALUE& v) {
        typename decltype(tbl)::accessor a;
        if (tbl.find(a, k)) {
            a->second = v;
            return true;
        } else {
            return false;
        }
    }

    template <typename Updater>
    void upsert(const KEY& k, Updater fn, const VALUE& v) {
        typename decltype(tbl)::accessor a;
        if (tbl.insert(a, k)) {
            a->second = v;
        } else {
            fn(a->second);
        }
    }

    tbb::concurrent_hash_map<KEY, VALUE, CustomHashCompare> tbl;
};

#else
#error Must define either LIBCUCKOO or TBB
#endif
#endif

#endif // _UNIVERSAL_TABLE_WRAPPER_HH
