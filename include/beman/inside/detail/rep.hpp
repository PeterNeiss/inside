// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_REP_HPP
#define BEMAN_INSIDE_DETAIL_REP_HPP

#include <beman/inside/generic.hpp>
#include <beman/inside/grid.hpp>
#include <beman/inside/policy_flag.hpp>

//---------------------------------------------------------------------------
// rep — representation-flag propagation for binary arithmetic results, shared
// by addition/multiplication/division. Propagate fp storage only when the
// result grid stays exactly representable in the chosen width; otherwise
// demote (f32→f64) or drop it so storage_pick deduces an exact representation
// (the fp result would diverge from the exact result — see grid::double_exact
// / float_exact). Widest-wins: prefer f32 only when both operands are
// f32-only and the result fits float; an f64 operand or a too-fine-for-float
// result widens to f64; too fine for double → exact; a continuous result
// (Notch 0) drops it. fp storage never changes a result: the flags only pick
// the raw, and the result's other flags are computed as without them.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
template <insidable Lhs, insidable Rhs, grid ResultGrid>
struct fp_rep {
    static constexpr bool any_f64  = has_flag(policy_of<Lhs>, f64) || has_flag(policy_of<Rhs>, f64);
    static constexpr bool any_f32  = has_flag(policy_of<Lhs>, f32) || has_flag(policy_of<Rhs>, f32);
    static constexpr bool keep_f32 = any_f32 && !any_f64 && float_exact<ResultGrid>;
    static constexpr bool keep_f64 = !keep_f32 && (any_f64 || any_f32) && double_exact<ResultGrid>;
    // Carry both operands' representation flags (widest-wins at storage selection).
    // `direct` needs notch 1 and `indexed` a non-zero notch; a result grid
    // that cannot hold them drops them (storage is then deduced).
    static constexpr policy_flag carried = (policy_of<Lhs> | policy_of<Rhs>)&(
        exact | (ResultGrid.Notch == 1 ? direct : none) | (ResultGrid.Notch != 0 ? indexed : none));
    static constexpr policy_flag rep = carried | (keep_f64 ? f64 : none) | (keep_f32 ? f32 : none);
    // The result inside's policy: the propagated representation, checked when
    // either operand is (a plain result is always checked); a representation
    // carried from two `unsafe` operands stays unchecked.
    static constexpr policy_flag result_policy =
        rep | ((carried == none || is_checked(policy_of<Lhs>) || is_checked(policy_of<Rhs>)) ? checked
                                                                                             : detail::unsafe_marker);
};

// The fp storage a result on grid G keeps from its operands Ins — fp_rep's
// rule for any number of operands: f32 when only f32 operands and float holds
// G exactly, f64 when double does, else none. Storage only: the result's
// value and policy are the same either way.
template <grid G, insidable... Ins>
inline constexpr policy_flag fp_storage_for = [] {
    constexpr bool any_f64 = (has_flag(policy_of<Ins>, f64) || ...);
    constexpr bool any_f32 = (has_flag(policy_of<Ins>, f32) || ...);
    if constexpr (any_f32 && !any_f64 && float_exact<G>)
        return f32;
    else if constexpr ((any_f64 || any_f32) && double_exact<G>)
        return f64;
    else
        return none;
}();

// A deduced result type: grid G, policy P (without storage flags), plus the
// fp storage its operands' flags and G allow.
template <grid G, policy_flag P, insidable... Ins>
using deduced_inside = inside<G, P | fp_storage_for<G, Ins...>>;
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
