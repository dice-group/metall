// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_KERNEL_OBJECT_ATTRIBUTE_ACCESSOR_HPP
#define METALL_KERNEL_OBJECT_ATTRIBUTE_ACCESSOR_HPP

#include <type_traits>
#include <memory>

#include <metall/tags.hpp>
#include <metall/kernel/attributed_object_directory.hpp>

namespace metall {

/// \namespace metall::attraccs_detail
/// \brief Namespace for the details of attribute accessor
namespace attraccs_detail {

/// \brief
/// \tparam _offset_type
/// \tparam _size_type
template <typename _offset_type, typename _size_type>
class general_named_object_attr_accessor {
 private:
  using object_directory_type =
      kernel::attributed_object_directory<_offset_type, _size_type>;

  struct core_data {
    object_directory_type object_directory{};
    std::filesystem::path object_attribute_file_path{};
  };

 public:
  // -------------------- //
  // Public types and static values
  // -------------------- //
  using size_type = typename object_directory_type::size_type;
  using name_type = typename object_directory_type::name_type;
  using offset_type = typename object_directory_type::offset_type;
  using length_type = typename object_directory_type::length_type;
  using description_type = typename object_directory_type::description_type;

  using const_iterator = typename object_directory_type::const_iterator;

  general_named_object_attr_accessor() noexcept = default;

  explicit general_named_object_attr_accessor(
      const std::filesystem::path &object_attribute_file_path) : m_core_data{std::make_unique<core_data>()} {
    m_core_data->object_attribute_file_path = object_attribute_file_path;
    m_core_data->object_directory.deserialize(m_core_data->object_attribute_file_path);
  }

  general_named_object_attr_accessor(
      const general_named_object_attr_accessor &other) : m_core_data{std::make_unique<core_data>(*other.m_core_data)} {
  }

  general_named_object_attr_accessor(
      general_named_object_attr_accessor &&) noexcept = default;

  general_named_object_attr_accessor &operator=(
      const general_named_object_attr_accessor &other) {
    *m_core_data = *other.m_core_data;
    return *this;
  }

  general_named_object_attr_accessor &operator=(
      general_named_object_attr_accessor &&) noexcept = default;

  /// \brief Returns the number of objects in the directory.
  /// \return Returns the number of objects in the directory.
  size_type num_objects() const noexcept {
    return m_core_data->object_directory.size();
  }

  /// \brief Counts the number of objects with the name.
  /// As object name must be unique, only 1 or 0 is returned.
  /// \param name A name of an object to count.
  /// \return Returns 1 if the object exist; otherwise, 0.
  size_type count(std::string const &name) const noexcept {
    return m_core_data->object_directory.count(name);
  }

  /// \brief Finds the position of the object attribute with 'name'.
  /// \param name A name of an object to find.
  /// \return Returns a const iterator that points the found object attribute.
  /// If not found, a returned iterator is equal to that of end().
  const_iterator find(std::string const &name) const noexcept {
    return m_core_data->object_directory.find(name);
  }

  /// \brief Returns a const iterator that points the beginning of stored object
  /// attribute. \return Returns a const iterator that points the beginning of
  /// stored object attribute.
  const_iterator begin() const noexcept {
    return m_core_data->object_directory.begin();
  }

  /// \brief Returns a const iterator that points the end of stored object
  /// attribute. \return Returns a const iterator that points the end of stored
  /// object attribute.
  const_iterator end() const noexcept {
    return m_core_data->object_directory.end();
  }

  /// \brief Sets an description.
  /// An existing one is overwrite.
  /// \param position An iterator to an object.
  /// \param description A description string in description_type.
  /// \return Returns true if a description is set (stored) successfully.
  /// Otherwise, false.
  void set_description(const_iterator position,
                       const description_type &description) {
    if (position == end()) {
      throw std::logic_error{"Cannot set description: position is end"};
    }

    m_core_data->object_directory.set_description(position, description);
    m_core_data->object_directory.serialize(m_core_data->object_attribute_file_path);
  }

  /// \brief Sets an description.
  /// An existing one is overwrite.
  /// \param name A name of an object.
  /// \param description A description string in description_type.
  /// \return Returns true if a description is set (stored) successfully.
  /// Otherwise, false.
  void set_description(std::string const &name,
                       const description_type &description) {
    set_description(find(name), description);
  }

 private:
  std::unique_ptr<core_data> m_core_data;
};
}  // namespace attraccs_detail

/// \brief Objet attribute accessor for named object.
/// \tparam _offset_type Offset type.
/// \tparam _size_type Size type.
template <typename _offset_type, typename _size_type>
class named_object_attr_accessor
    : public attraccs_detail::general_named_object_attr_accessor<_offset_type,
                                                                 _size_type> {
 private:
  using base_type =
      attraccs_detail::general_named_object_attr_accessor<_offset_type,
                                                          _size_type>;

 public:
  using size_type = typename base_type::size_type;
  using name_type = typename base_type::name_type;
  using offset_type = typename base_type::offset_type;
  using length_type = typename base_type::length_type;
  using description_type = typename base_type::description_type;
  using const_iterator = typename base_type::const_iterator;

  named_object_attr_accessor() = default;

  explicit named_object_attr_accessor(
      const std::filesystem::path &object_attribute_file_path) noexcept
      : base_type(object_attribute_file_path) {}
};

/// \brief Objet attribute accessor for unique object.
/// \tparam _offset_type Offset type.
/// \tparam _size_type Size type.
template <typename _offset_type, typename _size_type>
class unique_object_attr_accessor
    : public attraccs_detail::general_named_object_attr_accessor<_offset_type,
                                                                 _size_type> {
 private:
  using base_type =
      attraccs_detail::general_named_object_attr_accessor<_offset_type,
                                                          _size_type>;

 public:
  using size_type = typename base_type::size_type;
  using name_type = typename base_type::name_type;
  using offset_type = typename base_type::offset_type;
  using length_type = typename base_type::length_type;
  using description_type = typename base_type::description_type;
  using const_iterator = typename base_type::const_iterator;

  unique_object_attr_accessor() = default;

  explicit unique_object_attr_accessor(
      const std::filesystem::path &object_attribute_file_path)
      : base_type(object_attribute_file_path) {}

  /// \brief Counts the number of objects with the name.
  /// As object name must be unique, only 1 or 0 is returned.
  /// \param name A name of an object to count.
  /// \return Returns 1 if the object exist; otherwise, 0.
  size_type count(const char *name) const noexcept {
    return base_type::count(name);
  }

  /// \brief Counts the number of the unique object of type T with the name,
  /// i.e., 1 or 0 is returned. \tparam T A type of an object. \return Returns 1
  /// if the object exist; otherwise, 0.
  template <typename T>
  size_type count(const decltype(unique_instance) &) const noexcept {
    return base_type::count(typeid(T).name());
  }

  /// \brief Finds the position of the object attribute with 'name'.
  /// \param name A name of an object to find.
  /// \return Returns a const iterator that points the found object attribute.
  /// If not found, a returned iterator is equal to that of end().
  const_iterator find(const char *name) const noexcept {
    return base_type::find(name);
  }

  /// \brief Finds the position of the attribute of the unique object of type T.
  /// \tparam T A type of an object.
  /// \return Returns a const iterator that points the found object attribute.
  /// If not found, a returned iterator is equal to that of end().
  template <typename T>
  const_iterator find(const decltype(unique_instance) &) const noexcept {
    return base_type::find(typeid(T).name());
  }

  /// \brief Sets an description.
  /// An existing one is overwrite.
  /// \param position An iterator to an object.
  /// \param description A description string in description_type.
  /// \return Returns true if a description is set (stored) successfully.
  /// Otherwise, false.
  void set_description(const_iterator position,
                  const description_type &description) {
    base_type::set_description(position, description);
  }

  /// \brief Sets an description.
  /// An existing one is overwrite.
  /// \param name A name of an object.
  /// \param description A description string in description_type.
  /// \return Returns true if a description is set (stored) successfully.
  /// Otherwise, false.
  void set_description(const char *name,
                       const description_type &description) {
    base_type::set_description(name, description);
  }

  /// \brief Sets an description to the unique object of type T.
  /// An existing one is overwrite.
  /// \tparam T A type of an object.
  /// \param description A description string in description_type.
  /// \return Returns true if a description is set (stored) successfully.
  /// Otherwise, false.
  template <typename T>
  void set_description(const decltype(unique_instance) &,
                       const description_type &description) {
    base_type::set_description(typeid(T).name(), description);
  }
};

/// \brief
/// \tparam _offset_type
/// \tparam _size_type
template <typename _offset_type, typename _size_type>
class anonymous_object_attr_accessor {
 private:
  using object_directory_type =
      kernel::attributed_object_directory<_offset_type, _size_type>;

  struct core_data {
    object_directory_type object_directory{};
    std::filesystem::path object_attribute_file_path{};
  };

 public:
  // -------------------- //
  // Public types and static values
  // -------------------- //
  using size_type = typename object_directory_type::size_type;
  using name_type = typename object_directory_type::name_type;
  using offset_type = typename object_directory_type::offset_type;
  using length_type = typename object_directory_type::length_type;
  using description_type = typename object_directory_type::description_type;

  using const_iterator = typename object_directory_type::const_iterator;

  anonymous_object_attr_accessor() noexcept = default;

  explicit anonymous_object_attr_accessor(
      const std::filesystem::path &object_attribute_file_path) : m_core_data{std::make_unique<core_data>()} {
    m_core_data->object_attribute_file_path = object_attribute_file_path;
    m_core_data->object_directory.deserialize(m_core_data->object_attribute_file_path);
  }

  anonymous_object_attr_accessor(
      const anonymous_object_attr_accessor &other) : m_core_data{std::make_unique<core_data>(*other.m_core_data)} {
  }

  anonymous_object_attr_accessor(anonymous_object_attr_accessor &&) noexcept =
      default;

  anonymous_object_attr_accessor &operator=(
      const anonymous_object_attr_accessor &other) {
    *m_core_data = *other.m_core_data;
    return *this;
  }

  anonymous_object_attr_accessor &operator=(
      anonymous_object_attr_accessor &&) noexcept = default;

  /// \brief Returns the number of objects in the directory.
  /// \return Returns the number of objects in the directory.
  size_type num_objects() const noexcept {
    return m_core_data->object_directory.size();
  }

  /// \brief Returns a const iterator that points the beginning of stored object
  /// attribute. \return Returns a const iterator that points the beginning of
  /// stored object attribute.
  const_iterator begin() const noexcept {
    return m_core_data->object_directory.begin();
  }

  /// \brief Returns a const iterator that points the end of stored object
  /// attribute. \return Returns a const iterator that points the end of stored
  /// object attribute.
  const_iterator end() const noexcept {
    return m_core_data->object_directory.end();
  }

  /// \brief Sets an description.
  /// An existing one is overwrite.
  /// \param position An iterator to an object.
  /// \param description A description string in description_type.
  /// \return Returns true if a description is set (stored) successfully.
  /// Otherwise, false.
  void set_description(const_iterator position,
                       const description_type &description) {
    if (position == end()) {
      throw std::logic_error{"Cannot set description: position is end"};
    }

    m_core_data->object_directory.set_description(position, description);
    m_core_data->object_directory.serialize(m_core_data->object_attribute_file_path);
  }

 private:
  std::unique_ptr<core_data> m_core_data;
};
}  // namespace metall

#endif  // METALL_KERNEL_OBJECT_ATTRIBUTE_ACCESSOR_HPP
