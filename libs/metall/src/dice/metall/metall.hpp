// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_METALL_HPP
#define METALL_METALL_HPP

#include <dice/metall/defs.hpp>
#include <dice/metall/basic_manager.hpp>
#include <dice/metall/logger.hpp>
#include <dice/metall/version.hpp>

/// \namespace dice::metall
/// \brief The top level of namespace of Metall
namespace dice::metall {

/// \brief Default Metall manager class which is an alias of basic_manager with
/// the default template parameters.
using manager = basic_manager<>;

}  // namespace dice::metall

/// \example complex_map.cpp
/// This is an example of how to use a complex STL map container with Metall.

/// \example multilevel_containers.cpp
/// This is an example of how to use a multi-level STL container with Metall.

/// \example allocator_aware_type.cpp
/// This example shows how to create an allocator-aware type, which takes an
/// allocator and can be stored inside an STL container.

/// \example simple.cpp
/// This is a simple example of how to use Metall.

/// \example snapshot.cpp
/// This is an example of how to use the snapshot capabilities in Metall.

/// \example string.cpp
/// This is an example of how to use the STL string with Metall.

/// \example string_map.cpp
/// This is an example of how to use a STL map container of which key is STL
/// string.

/// \example vector_of_vectors.cpp
/// This is an example of how to use a multi-level STL container, a vector of
/// vectors, with Metall.

/// \example adjacency_list_graph.cpp
/// This is an example of how to use a adjacency-list graph data structure with
/// Metall.

/// \example csr_graph.cpp
/// This is an example of how to use a CSR graph data structure with Metall.

/// \example logger.cpp
/// This is an example of how to use Metall's logger.

/// \example datastore_description.cpp
/// This is an example of how to set and get datastore description.

/// \example object_attribute.cpp
/// This is an example of how to access object attributes.

/// \example object_attribute_api_list.cpp
/// This is an example of how to use all API related to object attributes.

/// \example metall_containers.cpp
/// This is an example of how to use Metall containers.

#endif  // METALL_METALL_HPP