//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDmultiplicationHPP
#define BNDmultiplicationHPP

#include "bound/generic.hpp"
#include "bound/grid.hpp"
#include "bound/policy.hpp"

//---------------------------------------------------------------------------
// multiplication — `mul(L, R, policy, action) -> bound<Grid<L> * Grid<R>>`. The
// integer hot path branches on which corner of the four-quadrant product hits
// `Lower<result>`, doing the arithmetic as `umax * umax` (no signed overflow)
// plus integer offset corrections. Rational-result and all-integer-aligned
// cases come first.
//---------------------------------------------------------------------------
namespace bnd::detail
{
  template <boundable L, boundable R = L>
  struct multiplication
  {
    static_assert((Grid<L> * Grid<R>).has_value(),
      "multiplication: result grid's notch/interval exceeds the representable "
      "rational range — coarsen the operand grids");
    static constexpr grid result_grid = (Grid<L> * Grid<R>).value();
    static constexpr bool any_real =
        (BoundPolicy<L> & bnd::real) == bnd::real || (BoundPolicy<R> & bnd::real) == bnd::real;
    // Drop `real` when the product grid (notch = N_L·N_R) outgrows double's
    // 53-bit significand — the double product would round below the result notch.
    static constexpr bool keep_real = any_real && double_exact<result_grid>;
    // Carry both operands' representation flags (widest-wins at storage selection).
    static constexpr policy_flag rep =
        ((BoundPolicy<L> | BoundPolicy<R>) & (bnd::exact | bnd::direct | bnd::indexed))
        | (keep_real ? bnd::real : none);
    using result = bound<result_grid, rep != none ? rep : checked>;

    // The dropped-`real` case (any_real && !keep_real) lands on a rational result
    // when the product grid outgrows uint index space; its product numerator can
    // exceed `umax`, so check it (the result carries `checked`) rather than wrap.
    template <typename P>
    static constexpr bool needs_overflow_check =
        rational_raw<result>
        && (((BoundPolicy<L> | BoundPolicy<R>) & checked) || plain<P>::test(checked)
            || (any_real && !keep_real))
        && !rational_mul_is_safe(Grid<L>, Grid<R>);

    template <typename P>
    using return_type_for = std::conditional_t<needs_overflow_check<P>,
                                               slim::optional<result>,
                                               result>;

    template <typename P, typename A>
    using mul_return_t = std::conditional_t<overflow_action<plain<A>>,
                                            result,
                                            return_type_for<P>>;

    // Defined inline (not out-of-line): MSVC mishandles out-of-line member
    // templates of constrained partial specializations.
    template <typename P, typename A = no_action>
    static constexpr auto mul(L lhs, R rhs, P&& policy, A&& action = {}) -> mul_return_t<P, A>
  {
    if constexpr (real_raw<result>)
    {
      return result::from_raw(Grid<result>.snap_double(as_double(lhs) * as_double(rhs)));
    }
    else if constexpr (rational_raw<result>)
    {
      if constexpr (needs_overflow_check<P>)
      {
        auto prod = as_rational(lhs) * as_rational(rhs);
        if (!prod) [[unlikely]]
          return report_or_nullopt<result>(action, policy, errc::overflow,
                                           "rational overflow in mul");
        return result::from_raw(raw_cast<result>(*prod));
      }
      else
        return result::from_raw(raw_cast<result>(rational::mul_unchecked(
            as_rational(lhs), as_rational(rhs))));
    }
    else if constexpr (IsIntegerAligned<L> && IsIntegerAligned<R> && IsIntegerAligned<result>)
    {
      result res;
      from_value(res, to_value(lhs) * to_value(rhs));
      return res;
    }
    else if constexpr (real_raw<L> || real_raw<R> || rational_raw<L> || rational_raw<R>)
    {
      // An operand whose raw is a double/rational can't feed the integer
      // four-quadrant formula below (it reads the raw as an integer offset).
      // Combine exactly as rationals and convert to the result's storage —
      // mirrors addition's rational-mixed branch. Reached when `real` was
      // dropped from the result (grid not double-exact) but operands stay real.
      auto prod = rational::mul_unchecked(as_rational(lhs), as_rational(rhs));
      return result::from_raw(raw_from_offset<result>(
          ((prod - Lower<result>) / Notch<result>).value().Numerator));
    }
    else
    {
      // Result writes go through raw_from_offset so direct-storage results
      // get Lower<result> added back to recover the value.
      auto to_result = [](auto raw_offset)
      { return result::from_raw(raw_from_offset<result>(static_cast<umax>(raw_offset))); };

      // Normalize lhs.raw() / rhs.raw() to *offsets* regardless of L's / R's
      // storage shape. The formulas below all assume offset arithmetic.
      umax lhs_offset = !index_raw<L>
          ? static_cast<umax>(raw_imax(lhs) - RawLo<L>)
          : static_cast<umax>(lhs.raw());
      umax rhs_offset = !index_raw<R>
          ? static_cast<umax>(raw_imax(rhs) - RawLo<R>)
          : static_cast<umax>(rhs.raw());

      // Absolute notch index of each operand endpoint (Lower/Notch, Upper/Notch).
      constexpr umax idxLoL = (Lower<L>/Notch<L>).value_or(rational{0}).Numerator;
      constexpr umax idxLoR = (Lower<R>/Notch<R>).value_or(rational{0}).Numerator;
      constexpr umax idxHiL = (Upper<L>/Notch<L>).value_or(rational{0}).Numerator;

      // Integral promotion would make `raw * raw` an `int * int` (UB above
      // INT_MAX), so cast to umax to multiply in 64-bit unsigned space. The four
      // branches cover the sign quadrants: Lower<result> is one of the four
      // corner products; sign-flipped helpers (negative<L>/<R>) reduce each to
      // the all-positive formula. The static_assert guards the case analysis.
      if constexpr (Lower<result> == (Lower<L> * Lower<R>).value())
      {
        return to_result(lhs_offset * rhs_offset
                         + lhs_offset * idxLoR
                         + rhs_offset * idxLoL);
      }

      if constexpr (Lower<result> == (Upper<L> * Upper<R>).value())
      { return multiplication<negative<L>, negative<R>>::mul(-lhs, -rhs, std::forward<P>(policy)); }

      if constexpr (Lower<result> == (Upper<L> * Lower<R>).value())
      {
        umax negLhs = NotchCount<L> - lhs_offset;
        return to_result(negLhs * idxLoR
                         + rhs_offset * idxHiL
                         - negLhs * rhs_offset);
      }

      if constexpr (Lower<result> == (Lower<L> * Upper<R>).value())
      { return -multiplication<L, negative<R>>::mul(lhs, -rhs, std::forward<P>(policy)); }

      static_assert(Lower<result> == (Lower<L> * Lower<R>).value()
                 || Lower<result> == (Upper<L> * Upper<R>).value()
                 || Lower<result> == (Upper<L> * Lower<R>).value()
                 || Lower<result> == (Lower<L> * Upper<R>).value(),
                 "multiplication: internal logic error");
    }
  }
  };
} // namespace bnd::detail

#endif // BNDmultiplicationHPP
