// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_REP_HPP
#define BEMAN_INSIDE_DETAIL_REP_HPP

#include <beman/inside/generic.hpp>
#include <beman/inside/grid.hpp>
#include <beman/inside/policy_flag.hpp>

//---------------------------------------------------------------------------
// result_rep — representation-flag propagation for binary arithmetic results,
// shared by addition/multiplication/division. The result carries `exact`, and
// `direct` / `indexed` where its grid can hold them; otherwise storage_pick
// deduces it.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
template <insidable Lhs, insidable Rhs, grid ResultGrid>
struct result_rep {
    // `direct` needs notch 1 and `indexed` a non-zero notch; a result grid
    // that cannot hold them drops them (storage is then deduced).
    static constexpr policy_flag carried = (policy_of<Lhs> | policy_of<Rhs>)&(
        exact | (ResultGrid.Notch == 1 ? direct : none) | (ResultGrid.Notch != 0 ? indexed : none));
    // The result inside's policy: the propagated representation, checked when
    // either operand is (a plain result is always checked); a representation
    // carried from two `unsafe` operands stays unchecked.
    static constexpr policy_flag result_policy =
        carried |
        ((carried == none || is_checked(policy_of<Lhs>) || is_checked(policy_of<Rhs>)) ? checked
                                                                                       : detail::unsafe_marker);
};

// Whether a + or × into Result runs its overflow check. The result grid
// holds every result, so only a rational or fraction raw can overflow: a
// fraction raw always may (its denominator grows); a rational raw under a
// checked or `exact` policy, unless the grids prove the op fits
// (RationalSafe: rational_add_is_safe / rational_mul_is_safe).
template <insidable Result, insidable L, insidable R, policy_flag F, bool RationalSafe>
inline constexpr bool lattice_op_checked =
    fraction_storage<Result> || (rational_storage<Result> &&
                                 (has_any_flag(F, checked) || is_checked(policy_of<L>) || is_checked(policy_of<R>) ||
                                  has_any_flag(F | policy_of<L> | policy_of<R>, exact)) &&
                                 !RationalSafe);
} // namespace beman::inside::detail
#endif
