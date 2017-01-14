/** \file */

#ifndef _LIBCUCKOO_LAZY_ARRAY_HH
#define _LIBCUCKOO_LAZY_ARRAY_HH

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>

#include "cuckoohash_util.hh"

/**
 * A fixed-size array, broken up into segments that are dynamically allocated
 * upon request. It is the user's responsibility to make sure they only access
 * allocated parts of the array.
 *
 * @tparam OFFSET_BITS the number of bits of the index used as the offset within
 * a segment
 * @tparam SEGMENT_BITS the number of bits of the index used as the segment
 * index
 * @tparam T the type of stored in the container
 * @tparam Alloc the allocator used to allocate data
 */
template <uint8_t OFFSET_BITS, uint8_t SEGMENT_BITS,
          class T, class Alloc = std::allocator<T>
          >
class libcuckoo_lazy_array {
public:
    using value_type = T;
    using allocator_type = Alloc;
private:
    using traits_ = std::allocator_traits<allocator_type>;
public:
    using size_type = std::size_t;
    using reference = value_type&;
    using const_reference = const value_type&;

    static_assert(SEGMENT_BITS + OFFSET_BITS <= sizeof(size_type)*8,
                  "The number of segment and offset bits cannot exceed "
                  " the number of bits in a size_type");

    /**
     * Default constructor. Creates an empty array with no allocated segments.
     */
    libcuckoo_lazy_array(const allocator_type& allocator = Alloc())
        noexcept(noexcept(Alloc()))
        : segments_{{nullptr}}, allocated_segments_(0), allocator_(allocator) {}

    /**
     * Constructs an array with enough segments allocated to fit @p target
     * elements. Each allocated element is default-constructed.
     *
     * @param target the number of elements to allocate space for
     */
    libcuckoo_lazy_array(size_type target,
                         const allocator_type& allocator = Alloc())
        noexcept(noexcept(Alloc()))
        : libcuckoo_lazy_array(allocator) {
        segments_.fill(nullptr);
        resize(target);
    }

    libcuckoo_lazy_array(const libcuckoo_lazy_array&) = delete;
    libcuckoo_lazy_array& operator=(const libcuckoo_lazy_array&) = delete;

    /**
     * Move constructor
     *
     * @param arr the array being moved
     */
    libcuckoo_lazy_array(libcuckoo_lazy_array&& arr) noexcept
        : segments_(arr.segments_),
          allocated_segments_(arr.allocated_segments_),
          allocator_(std::move(arr.allocator_)) {
        // Deactivate the array by setting its allocated segment count to 0
        arr.allocated_segments_ = 0;
    }

    /**
     * Destructor. Destroys all elements allocated in the array.
     */
    ~libcuckoo_lazy_array()
        noexcept(std::is_nothrow_destructible<T>::value) {
        clear();
    }

    /**
     * Destroys all elements allocated in the array.
     */
    void clear() {
        for (size_type i = 0; i < allocated_segments_; ++i) {
            destroy_array(segments_[i]);
            segments_[i] = nullptr;
        }
    }

    /**
     * Index operator
     *
     * @return a reference to the data at the given index
     */
    reference operator[](size_type i) {
        assert(get_segment(i) < allocated_segments_);
        return segments_[get_segment(i)][get_offset(i)];
    }

    /**
     * Const index operator
     *
     * @return a const reference to the data at the given index
     */
    const_reference operator[](size_type i) const {
        assert(get_segment(i) < allocated_segments_);
        return segments_[get_segment(i)][get_offset(i)];
    }

    /**
     * Returns the number of elements the array has allocated space for
     *
     * @return current size of the array
     */
    size_type size() const {
        return allocated_segments_ * SEGMENT_SIZE;
    }

    /**
     * Returns the maximum number of elements the array can hold
     *
     * @return maximum size of the array
     */
    static constexpr size_type max_size() {
        return 1UL << (OFFSET_BITS + SEGMENT_BITS);
    }

    /**
     * Allocate enough space for @p target elements, not exceeding the capacity
     * of the array. Under no circumstance will the array be shrunk.
     *
     * @param target the number of elements to ensure space is allocated for
     */
    void resize(size_type target) {
        target = std::min(target, max_size());
        if (target == 0) {
            return;
        }
        const size_type last_segment = get_segment(target - 1);
        for (size_type i = allocated_segments_; i <= last_segment; ++i) {
            segments_[i] = create_array();
        }
        allocated_segments_ = last_segment + 1;
    }

private:
    static constexpr size_type SEGMENT_SIZE = 1UL << OFFSET_BITS;
    static constexpr size_type NUM_SEGMENTS = 1UL << SEGMENT_BITS;
    static constexpr size_type OFFSET_MASK = SEGMENT_SIZE - 1;

    std::array<T*, NUM_SEGMENTS> segments_;
    size_type allocated_segments_;
    allocator_type allocator_;

    static size_type get_segment(size_type i) {
        return i >> OFFSET_BITS;
    }

    static size_type get_offset(size_type i) {
        return i & OFFSET_MASK;
    }

    // Allocates a SEGMENT_SIZE-sized array and default-initializes each element
    typename traits_::pointer create_array() {
        typename traits_::pointer arr = traits_::allocate(
            allocator_, SEGMENT_SIZE);
        // Initialize all the elements, safely deallocating and destroying
        // everything in case of error.
        size_type i;
        try {
            for (i = 0; i < SEGMENT_SIZE; ++i) {
                traits_::construct(allocator_, &arr[i]);
            }
        } catch (...) {
            for (size_type j = 0; j < i; ++j) {
                traits_::destroy(allocator_, &arr[j]);
            }
            traits_::deallocate(allocator_, arr, SEGMENT_SIZE);
            throw;
        }
        return arr;
    }

    // Destroys every element of a SEGMENT_SIZE-sized array and then deallocates
    // the memory.
    void destroy_array(typename traits_::pointer arr) {
        for (size_type i = 0; i < SEGMENT_SIZE; ++i) {
            traits_::destroy(allocator_, &arr[i]);
        }
        traits_::deallocate(allocator_, arr, SEGMENT_SIZE);
    }
};

#endif // _LIBCUCKOO_LAZY_ARRAY_HH
