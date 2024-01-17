#ifndef DICE_METALLFFI_METALLINTERNAL_HPP
#define DICE_METALLFFI_METALLINTERNAL_HPP

#include <dice/metall/metall.hpp>

namespace dice::copperr::ffi::internal {
/**
 * @brief The metall manager type used internally.
 *      This object type is whats actually behind the opaque ::copperr_manager *
 * in the interface
 */
using copperr_manager = copperr::manager;
}  // namespace dice::copperr::ffi::internal

#endif  // DICE_METALLFFI_METALLINTERNAL_HPP
