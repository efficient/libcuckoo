#ifndef _UNIVERSAL_GEN_HH
#define _UNIVERSAL_GEN_HH

#include <bitset>
#include <cstdint>
#include <memory>
#include <string>

/* A specialized functor for generating unique keys and values from a sequence
 * number and thread id. Must define one for each type we want to use. */
using seq_t = uint32_t;
using thread_id_t = uint16_t;

template <typename T>
class Gen {
    // static T key(seq_t seq, thread_id_t thread_id, thread_id_t num_threads)
    // static T value()
};

template <>
class Gen<uint64_t> {
public:
    // Per-thread, the seq will be an incrementing number and the thread_id will
    // be constant. We assume thread_id < num_threads.
    static uint64_t key(seq_t seq, thread_id_t thread_id,
                        thread_id_t num_threads) {
        return static_cast<uint64_t>(seq) * num_threads + thread_id;
    }

    static uint64_t value() {
        return 0;
    }
};

template <>
class Gen<std::string> {
    static constexpr size_t STRING_SIZE = 100;
public:
    static std::string key(seq_t seq, thread_id_t thread_id,
                           thread_id_t num_threads) {
        return std::to_string(Gen<uint64_t>::key(seq, thread_id, num_threads));
    }

    static std::string value() {
        return std::string(STRING_SIZE, '0');
    }
};

// Should be 1KB. Bitset is nice since it already has std::hash specialized.
using MediumBlob = std::bitset<8192>;

template <>
class Gen<MediumBlob> {
public:
    static MediumBlob key(seq_t seq, thread_id_t thread_id,
                          thread_id_t num_threads) {
        return MediumBlob(Gen<uint64_t>::key(seq, thread_id, num_threads));
    }

    static MediumBlob value() {
        return MediumBlob();
    }
};

// Should be 1MB
using BigBlob = std::bitset<8388608>;

template <>
class Gen<BigBlob> {
public:
    static BigBlob key(seq_t seq, thread_id_t thread_id,
                       thread_id_t num_threads) {
        return BigBlob(Gen<uint64_t>::key(seq, thread_id, num_threads));
    }

    static BigBlob value() {
        return BigBlob();
    }
};

template <typename T>
class Gen<std::shared_ptr<T> > {
public:
    static std::shared_ptr<T> key(seq_t seq, thread_id_t thread_id,
                                  thread_id_t num_threads) {
        return std::make_shared<T>(Gen<T>::key(seq, thread_id, num_threads));
    }

    static std::shared_ptr<T> value() {
        return std::make_shared<T>(Gen<T>::value());
    }
};

#endif // _UNIVERSAL_GEN_HH
