//
// Created by Iwabuchi, Keita on 2019-05-19.
//

#ifndef METALL_V0_STL_ALLOCATOR_V0_HPP
#define METALL_V0_STL_ALLOCATOR_V0_HPP

#include <memory>
#include <type_traits>
#include <cassert>
#include <limits>

#include <metall/detail/base_stl_allocator.hpp>

namespace metall {
namespace detail {
std::size_t g_max_manager_kernel_id;

template <typename manager_kernel_type>
manager_kernel_type** manager_kernel_table(typename manager_kernel_type::id_type id) {
  static std::array<manager_kernel_type*, 1024> table;
  assert(id < 1024);
  assert(id <= g_max_manager_kernel_id);
  return &(table[id]);
}

} // namespace detail
} // namespace metall

namespace metall::v0::detail {

/// \brief A STL compatible allocator
/// \tparam T The type of the object
/// \tparam manager_kernel_type The type of the manager kernel
template <typename T, typename manager_kernel_type>
struct stl_allocator_type_holder {
  using value_type = T;
  using pointer = typename std::pointer_traits<typename manager_kernel_type::void_pointer>::template rebind<value_type>;
  using const_pointer = typename std::pointer_traits<pointer>::template rebind<const value_type>;
  using void_pointer = typename std::pointer_traits<pointer>::template rebind<void>;
  using const_void_pointer = typename std::pointer_traits<pointer>::template rebind<const void>;
  using difference_type = typename std::pointer_traits<pointer>::difference_type;
  using size_type = typename std::make_unsigned<difference_type>::type;
};

} // namespace metall::v0::detail

namespace metall::v0 {
template <typename T, typename manager_kernel_type>
class stl_allocator_v0
    : public metall::detail::base_stl_allocator<stl_allocator_v0<T, manager_kernel_type>,
                                                detail::stl_allocator_type_holder<T, manager_kernel_type>> {

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  using self_type = stl_allocator_v0<T, manager_kernel_type>;
  using type_holder = detail::stl_allocator_type_holder<T, manager_kernel_type>;
  using base_type = metall::detail::base_stl_allocator<self_type, type_holder>;
  friend base_type;

  using kernel_id_type = typename manager_kernel_type::id_type;

 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using pointer = typename type_holder::pointer;
  using const_pointer = typename type_holder::const_pointer;
  using void_pointer = typename type_holder::void_pointer;
  using const_void_pointer = typename type_holder::const_void_pointer;
  using value_type = typename type_holder::value_type;
  using size_type = typename type_holder::size_type;
  using difference_type = typename type_holder::difference_type;

 public:
  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  // Note: same as manager.hpp in Boost.interprocess,
  // 'explicit' keyword is not used on purpose to enable to call this constructor w/o arguments
  // although this allocator won't work correctly w/o a valid kernel_id
  stl_allocator_v0(kernel_id_type kernel_id)
      : m_kernel_id(kernel_id) {}

  /// \brief Construct a new instance using an instance that has a different T
  template <typename T2>
  stl_allocator_v0(const stl_allocator_v0<T2, manager_kernel_type> &allocator_instance)
      : m_kernel_id(allocator_instance.get_kernel_id()) {}

  stl_allocator_v0(const stl_allocator_v0<T, manager_kernel_type> &other) = default;
  stl_allocator_v0(stl_allocator_v0<T, manager_kernel_type> &&other) noexcept = default;

  stl_allocator_v0 &operator=(const stl_allocator_v0<T, manager_kernel_type> &) = default;
  stl_allocator_v0 &operator=(stl_allocator_v0<T, manager_kernel_type> &&other) noexcept = default;

  /// \brief Copy assign operator for another T
  template <typename T2>
  stl_allocator_v0 &operator=(const stl_allocator_v0<T2, manager_kernel_type> &other) {
    m_kernel_id = other.m_kernel_id;
    return *this;
  }

  /// \brief Move assign operator for another T
  template <typename T2>
  stl_allocator_v0 &operator=(stl_allocator_v0<T2, manager_kernel_type> &&other) noexcept {
    m_kernel_id = other.m_kernel_id;
    return *this;
  }

  // ----------------------------------- This class's unique public functions ----------------------------------- //
  kernel_id_type get_kernel_id() const {
    return m_kernel_id;
  }

 private:
  /// -------------------------------------------------------------------------------- ///
  /// Private methods (required by the base class)
  /// -------------------------------------------------------------------------------- ///
  template <typename T2>
  struct rebind_impl {
    using other = stl_allocator_v0<T2, manager_kernel_type>;
  };

  pointer allocate_impl(const size_type n) const {
    manager_kernel_type *const kernel = *(metall::detail::manager_kernel_table<manager_kernel_type>(m_kernel_id));
    return pointer(static_cast<value_type *>(kernel->allocate(n * sizeof(T))));
  }

  void deallocate_impl(pointer ptr, [[maybe_unused]] const size_type size) const noexcept {
    manager_kernel_type *const kernel = *(metall::detail::manager_kernel_table<manager_kernel_type>(m_kernel_id));
    kernel->deallocate(to_raw_pointer(ptr));
  }

  size_type max_size_impl() const noexcept {
    return std::numeric_limits<size_type>::max();
  }

  template <class... Args>
  void construct_impl(const pointer &ptr, Args &&... args) const {
    ::new((void *)to_raw_pointer(ptr)) value_type(std::forward<Args>(args)...);
  }

  void destroy_impl(const pointer &ptr) const {
    assert(ptr != 0);
    (*ptr).~value_type();
  }

  stl_allocator_v0<T, manager_kernel_type> select_on_container_copy_construction_impl() const {
    return stl_allocator_v0<T, manager_kernel_type>(*this);
  }

  bool propagate_on_container_copy_assignment_impl() const noexcept {
    return std::true_type();
  }

  bool propagate_on_container_move_assignment_impl() const noexcept {
    return std::true_type();
  }

  bool propagate_on_container_swap_impl() const noexcept {
    return std::true_type();
  }

  bool is_always_equal_impl() const noexcept {
    return std::false_type();
  }

  /// -------------------------------------------------------------------------------- ///
  /// Private fields
  /// -------------------------------------------------------------------------------- ///
 private:
  // Initialize with a large value to detect uninitialized situations
  kernel_id_type m_kernel_id{std::numeric_limits<kernel_id_type>::max()};

};

template <typename T, typename kernel>
bool operator==(const stl_allocator_v0<T, kernel> &rhd, const stl_allocator_v0<T, kernel> &lhd) {
  return rhd.get_kernel_id() == lhd.get_kernel_id();
}

template <typename T, typename kernel>
bool operator!=(const stl_allocator_v0<T, kernel> &rhd, const stl_allocator_v0<T, kernel> &lhd) {
  return !(rhd == lhd);
}

}

#endif //METALL_V0_STL_ALLOCATOR_V0_HPP