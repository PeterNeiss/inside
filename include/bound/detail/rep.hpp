//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDrepHPP
#define BNDrepHPP

#include "bound/generic.hpp"
#include "bound/grid.hpp"
#include "bound/policy_flag.hpp"

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
namespace bnd::detail
{
  template <boundable Lhs, boundable Rhs, grid ResultGrid, bool AllowContinuous = false>
  struct fp_rep
  {
    static constexpr bool any_f64 =
        (BoundPolicy<Lhs> & bnd::real) == bnd::real || (BoundPolicy<Rhs> & bnd::real) == bnd::real;
    static constexpr bool any_f32 =
        (BoundPolicy<Lhs> & bnd::f32) == bnd::f32 || (BoundPolicy<Rhs> & bnd::f32) == bnd::f32;
    static constexpr bool continuous_ok = AllowContinuous && ResultGrid.Notch == 0;
    static constexpr bool keep_f32 =
        any_f32 && !any_f64 && (continuous_ok || float_exact<ResultGrid>);
    static constexpr bool keep_f64 =
        !keep_f32 && (any_f64 || any_f32) && (continuous_ok || double_exact<ResultGrid>);
    static constexpr bool dropped_fp = (any_f64 || any_f32) && !keep_f64 && !keep_f32;
    // Carry both operands' representation flags (widest-wins at storage selection).
    static constexpr policy_flag rep =
        ((BoundPolicy<Lhs> | BoundPolicy<Rhs>) & (bnd::exact | bnd::direct | bnd::indexed))
        | (keep_f64 ? bnd::real : none) | (keep_f32 ? bnd::f32 : none);
    // The result bound's policy: the propagated representation, or plain checked.
    static constexpr policy_flag result_policy = rep != none ? rep : checked;
  };
}
#endif
