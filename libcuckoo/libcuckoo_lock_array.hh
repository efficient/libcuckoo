/** \file */

#ifndef _LIBCUCKOO_LOCK_ARRAY_HH
#define _LIBCUCKOO_LOCK_ARRAY_HH

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>

#include "cuckoohash_util.hh"

/**
 * A fixed-size array of locks, broken up into segments that are dynamically
 * allocated upon request. It is the user's responsibility to make sure they
 * only access allocated parts of the array.
 *
 * @tparam OFFSET_BITS the number of bits of the index used as the offset within
 * a segment
 * @tparam SEGMENT_BITS the number of bits of the index used as the segment
 * index
 * @tparam T the type of stored in the container
 * @tparam Alloc the allocator used to allocate data
 */
template <uint8_t OFFSET_BITS, uint8_t SEGMENT_BITS, class Alloc>
class libcuckoo_lock_array {
public:
  // Locking modes
  using locking_active = std::integral_constant<bool, true>;
  using locking_inactive = std::integral_constant<bool, false>;

  // Counter type
  using counter_type = int64_t;

  // A fast, lightweight spinlock
  LIBCUCKOO_SQUELCH_PADDING_WARNING
  class LIBCUCKOO_ALIGNAS(64) spinlock {
  public:
    void lock(locking_active) noexcept {
      while (lock_.test_and_set(std::memory_order_acq_rel))
        ;
    }

    void lock(locking_inactive) noexcept {}

    void unlock(locking_active) noexcept {
      lock_.clear(std::memory_order_release);
    }

    void unlock(locking_inactive) noexcept {}

    bool try_lock(locking_active) noexcept {
      return !lock_.test_and_set(std::memory_order_acq_rel);
    }

    bool try_lock(locking_inactive) noexcept { return true; }

    counter_type &elem_counter() noexcept { return elem_counter_; }

    counter_type elem_counter() const noexcept { return elem_counter_; }

  private:
    std::atomic_flag lock_;
    counter_type elem_counter_;
  };

private:
  using traits_ =
      typename std::allocator_traits<Alloc>::template rebind_traits<spinlock>;

public:
  using value_type = typename traits_::value_type;
  using allocator_type = typename traits_::allocator_type;
  using size_type = std::size_t;
  using reference = value_type &;
  using const_reference = const value_type &;

  static_assert(SEGMENT_BITS + OFFSET_BITS <= sizeof(size_type) * 8,
                "The number of segment and offset bits cannot exceed "
                " the number of bits in a size_type");

  libcuckoo_lock_array(size_type target,
                       const allocator_type &allocator = allocator_type())
      : allocator_(allocator), segment_allocator_(allocator_),
        segments_(nullptr), allocated_segments_(0) {
    segments_ = segment_allocator_.allocate(NUM_SEGMENTS);
    std::fill(segments_, segments_ + NUM_SEGMENTS, nullptr);
    resize(target);
  }

  libcuckoo_lock_array(const libcuckoo_lock_array &arr)
      : libcuckoo_lock_array(0, traits_::select_on_container_copy_construction(
                                    arr.get_allocator())) {
    copy_container(arr);
  }

  libcuckoo_lock_array(const libcuckoo_lock_array &arr,
                       const allocator_type &allocator)
      : libcuckoo_lock_array(0, allocator) {
    copy_container(arr);
  }

  libcuckoo_lock_array(libcuckoo_lock_array &&arr) noexcept
      : allocator_(std::move(arr.allocator_)), segment_allocator_(allocator_),
        segments_(arr.segments_), allocated_segments_(arr.allocated_segments_) {
    arr.segments_ = nullptr;
  }

  libcuckoo_lock_array(libcuckoo_lock_array &&arr,
                       const allocator_type &allocator)
      : libcuckoo_lock_array(0, allocator) {
    move_assign(arr, std::false_type());
  }

  libcuckoo_lock_array &operator=(const libcuckoo_lock_array &arr) {
    destroy_container();
    libcuckoo_copy_allocator(
        allocator_, arr.allocator_,
        typename traits_::propagate_on_container_copy_assignment());
    segment_allocator_ = allocator_;
    copy_container(arr);
    return *this;
  }

  libcuckoo_lock_array &operator=(libcuckoo_lock_array &&arr) {
    destroy_container();
    move_assign(arr,
                typename traits_::propagate_on_container_move_assignment());
    return *this;
  }

  ~libcuckoo_lock_array() noexcept {
    if (segments_ != nullptr) {
      destroy_container();
      segment_allocator_.deallocate(segments_, NUM_SEGMENTS);
    }
  }

  void swap(libcuckoo_lock_array &arr) noexcept {
    libcuckoo_swap_allocator(allocator_, arr.allocator_,
                             typename traits_::propagate_on_container_swap());
    segment_allocator_ = allocator_;
    std::swap(segments_, arr.segments_);
    std::swap(allocated_segments_, arr.allocated_segments_);
  }

  // Copies the size and data of `arr`
  void emulate(const libcuckoo_lock_array &arr) noexcept {
    resize(arr.size());
    for (size_type i = 0; i < allocated_segments_; ++i) {
      emulate_segment(segments_[i], arr.segments_[i]);
    }
  }

  reference operator[](size_type i) {
    assert(get_segment(i) < allocated_segments_);
    return segments_[get_segment(i)][get_offset(i)];
  }

  const_reference operator[](size_type i) const {
    assert(get_segment(i) < allocated_segments_);
    return segments_[get_segment(i)][get_offset(i)];
  }

  // Returns the number of elements the array has allocated space for
  size_type size() const { return allocated_segments_ * SEGMENT_SIZE; }

  // Returns the maximum number of elements the array can hold
  static constexpr size_type max_size() {
    return 1UL << (OFFSET_BITS + SEGMENT_BITS);
  }

  // Allocate enough space for `target` elements, not exceeding the capacity of
  // the array. Under no circumstance will the array be shrunk.
  void resize(size_type target) {
    target = std::min(target, max_size());
    const size_type last_segment = get_segment(target - 1);
    for (size_type i = allocated_segments_; i <= last_segment; ++i) {
      segments_[i] = create_segment();
    }
    allocated_segments_ = last_segment + 1;
  }

private:
  static constexpr size_type SEGMENT_SIZE = 1UL << OFFSET_BITS;
  static constexpr size_type NUM_SEGMENTS = 1UL << SEGMENT_BITS;
  static constexpr size_type OFFSET_MASK = SEGMENT_SIZE - 1;

  // Allocates a SEGMENT_SIZE-sized locks array and initializes each element
  spinlock *create_segment() {
    spinlock *arr = traits_::allocate(allocator_, SEGMENT_SIZE);
    for (size_type i = 0; i < SEGMENT_SIZE; ++i) {
      arr[i].unlock(locking_active());
      arr[i].elem_counter() = 0;
    }
    return arr;
  }

  // Destroys a SEGMENT_SIZE-sized locks array
  void destroy_segment(spinlock *arr) {
    traits_::deallocate(allocator_, arr, SEGMENT_SIZE);
  }

  // Emulates the data in `src` into `dst`
  static void emulate_segment(spinlock *dst, spinlock *src) {
    for (size_type i = 0; i < SEGMENT_SIZE; ++i) {
      dst[i].elem_counter() = src[i].elem_counter();
    }
  }

  // Creates a copy of the given SEGMENT_SIZE-sized array
  spinlock *copy_segment(spinlock *src) {
    spinlock *dst = create_segment();
    emulate_segment(dst, src);
    return dst;
  }

  void copy_container(const libcuckoo_lock_array &arr) {
    std::fill(segments_, segments_ + NUM_SEGMENTS, nullptr);
    allocated_segments_ = arr.allocated_segments_;
    for (size_type i = 0; i < allocated_segments_; ++i) {
      segments_[i] = copy_segment(arr.segments_[i]);
    }
  }

  // true here means the allocator should be propagated on move
  void move_assign(libcuckoo_lock_array &&arr, std::true_type) {
    allocator_ = std::move(arr.allocator_);
    segment_allocator_ = allocator_;
    segments_ = arr.segments_;
    allocated_segments_ = arr.allocated_segments_;
    arr.segments_ = nullptr;
  }

  void move_container(libcuckoo_lock_array &arr, std::false_type) {
    if (allocator_ == arr.allocator_) {
      segments_ = arr.segments_;
      allocated_segments_ = arr.allocated_segments_;
      arr.segments_ = nullptr;
    } else {
      copy_container(arr);
    }
  }

  // Destroys all allocated segments, but leaves the outer array intact
  void destroy_container() {
    for (size_type i = 0; i < allocated_segments_; ++i) {
      destroy_segment(segments_[i]);
    }
    std::fill(segments_, segments_ + NUM_SEGMENTS, nullptr);
    allocated_segments_ = 0;
  }

  // Gets the segment corresponding to the given index
  static size_type get_segment(size_type i) { return i >> OFFSET_BITS; }

  // Gets the segment offset corresponding to the given index
  static size_type get_offset(size_type i) { return i & OFFSET_MASK; }

  // Member variables
  allocator_type allocator_;
  // This allocator is simply a copy of `allocator_`, that exist to allocate
  // and deallocate segments. It will always be a simple copy of allocator_,
  // even when it changes.
  typename traits_::template rebind_alloc<spinlock *> segment_allocator_;
  spinlock **segments_;
  size_type allocated_segments_;
};

#endif // _LIBCUCKOO_LOCK_ARRAY_HH
