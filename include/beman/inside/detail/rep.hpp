// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
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
// result widens to f64; too fine for double → exact. Division sets
// AllowContinuous: a continuous result (Notch 0) keeps fp regardless — the
// raw stores the quotient verbatim, so there is no grid to land on.
//---------------------------------------------------------------------------
namespace beman::inside::detail
{
  template <insidable Lhs, insidable Rhs, grid ResultGrid, bool AllowContinuous = false>
  struct fp_rep
  {
    static constexpr bool any_f64 =
        has_flag(policy_of<Lhs>, f64) || has_flag(policy_of<Rhs>, f64);
    static constexpr bool any_f32 =
        has_flag(policy_of<Lhs>, f32) || has_flag(policy_of<Rhs>, f32);
    static constexpr bool continuous_ok = AllowContinuous && ResultGrid.Notch == 0;
    static constexpr bool keep_f32 =
        any_f32 && !any_f64 && (continuous_ok || float_exact<ResultGrid>);
    static constexpr bool keep_f64 =
        !keep_f32 && (any_f64 || any_f32) && (continuous_ok || double_exact<ResultGrid>);
    static constexpr bool dropped_fp = (any_f64 || any_f32) && !keep_f64 && !keep_f32;
    // Carry both operands' representation flags (widest-wins at storage selection).
    // `direct` needs notch 1 and `indexed` a non-zero notch; a result grid
    // that cannot hold them drops them (storage is then deduced).
    static constexpr policy_flag carried =
        (policy_of<Lhs> | policy_of<Rhs>)
        & (exact | (ResultGrid.Notch == 1 ? direct : none) | (ResultGrid.Notch != 0 ? indexed : none));
    static constexpr policy_flag rep =
        carried
        | (keep_f64 ? f64 : none) | (keep_f32 ? f32 : none);
    // The result inside's policy: the propagated representation, checked when
    // either operand is (a plain result is always checked); a representation
    // carried from two `unsafe` operands stays unchecked.
    static constexpr policy_flag result_policy =
        rep | ((rep == none || is_checked(policy_of<Lhs>) || is_checked(policy_of<Rhs>))
               ? checked : detail::unsafe_marker);
  };
}
#endif
