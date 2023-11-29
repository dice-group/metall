#ifndef DICE_METALLFFI_METALLINTERNAL_HPP
#define DICE_METALLFFI_METALLINTERNAL_HPP

#include <dice/metall/metall.hpp>

namespace dice::metall_ffi::internal {
/**
 * @brief The metall manager type used internally.
 *      This object type is whats actually behind the opaque ::metall_manager *
 * in the interface
 */
using metall_manager = metall::manager;
}  // namespace dice::metall_ffi::internal

#endif  // DICE_METALLFFI_METALLINTERNAL_HPP
