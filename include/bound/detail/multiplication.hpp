//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDmultiplicationHPP
#define BNDmultiplicationHPP

#include "bound/generic.hpp"
#include "bound/grid.hpp"
#include "bound/policy.hpp"

//---------------------------------------------------------------------------
// multiplication — `mul(L, R, policy, action) -> bound<Grid<L> * Grid<R>>`.
//
// The integer hot path branches on which corner of the four-quadrant product
// {LoL*LoR, LoL*HiR, HiL*LoR, HiL*HiR} hits `Lower<result>`. Each branch is
// arranged so the runtime arithmetic is `umax * umax` (no signed overflow)
// followed by integer offset corrections — see the `to_result` lambda and
// the comments around `static_cast<umax>` for the integer-promotion UB note.
//
// Rational-result and direct-storage cases (the "all-integer-aligned" fast
// path) come first.
//---------------------------------------------------------------------------
namespace bnd
{
  //---------------------------------------------------------------------------
  // grid_value_bounds / rational_mul_is_safe
  //
  // Conservative compile-time bound on the (numerator, denominator) any
  // canonical-form value on a grid can have, and a derived "can the rational
  // product of two grid values overflow" predicate.
  //
  // For a grid `{[lo, hi], notch}`, every value v = lo + k*notch (k integer)
  // expressed over the common denominator dC = |lo.den| * |hi.den| * |notch.den|
  // is linear in k, so the maximum scaled numerator is at one of the endpoints
  // — max(lo_scaled, hi_scaled). dC is a conservative bound on the canonical
  // denominator (lcm divides this product).
  //
  // The two helpers return `false` whenever the bound itself can't be computed
  // without overflow — i.e. the safe fallback is "assume the optional wrapper
  // is needed".
  //---------------------------------------------------------------------------
  constexpr bool grid_value_bounds(grid g, umax& max_num, umax& max_den) noexcept
  {
    umax d_lo = abs_den(g.Interval.Lower.Denominator);
    umax d_hi = abs_den(g.Interval.Upper.Denominator);
    umax d_no = (g.Notch.Numerator == 0) ? umax{1} : abs_den(g.Notch.Denominator);

    umax d_common;
    if (mul_overflow(d_lo, d_hi, &d_common)) return false;
    if (mul_overflow(d_common, d_no, &d_common)) return false;

    umax lo_scaled, hi_scaled;
    if (mul_overflow(g.Interval.Lower.Numerator, d_common / d_lo, &lo_scaled)) return false;
    if (mul_overflow(g.Interval.Upper.Numerator, d_common / d_hi, &hi_scaled)) return false;

    max_num = lo_scaled > hi_scaled ? lo_scaled : hi_scaled;
    max_den = d_common;
    return true;
  }

  constexpr bool rational_mul_is_safe(grid g_l, grid g_r) noexcept
  {
    umax n_l, d_l, n_r, d_r;
    if (!grid_value_bounds(g_l, n_l, d_l)) return false;
    if (!grid_value_bounds(g_r, n_r, d_r)) return false;

    umax num_prod, den_prod;
    if (mul_overflow(n_l, n_r, &num_prod)) return false;
    if (mul_overflow(d_l, d_r, &den_prod)) return false;
    if (den_prod > static_cast<umax>(std::numeric_limits<imax>::max())) return false;
    return true;
  }

  template <boundable L, boundable R = L>
  struct multiplication
  {
    using result = bound<(Grid<L> * Grid<R>).value()>;

    template <typename P>
    static constexpr bool needs_overflow_check =
        IsRawRational<result>
        && (((BoundPolicy<L> | BoundPolicy<R>) & checked) || plain<P>::test(checked))
        && !rational_mul_is_safe(Grid<L>, Grid<R>);

    template <typename P>
    using return_type_for = std::conditional_t<needs_overflow_check<P>,
                                               slim::optional<result>,
                                               result>;

    template <typename P, typename A>
    using mul_return_t = std::conditional_t<overflow_action<plain<A>>,
                                            result,
                                            return_type_for<P>>;

    template <typename P, typename A = no_action>
    static constexpr mul_return_t<P, A> mul(L, R, P&&, A&& = {});
  };

  //---------------------------------------------------------------------------
  // mul
  //---------------------------------------------------------------------------
  template <boundable L, boundable R>
  template <typename P, typename A>
  constexpr auto multiplication<L,R>::mul(L lhs, R rhs, P&& policy, A&& action) -> mul_return_t<P, A>
  {
    if constexpr (IsRawRational<result>)
    {
      if constexpr (needs_overflow_check<P>)
      {
        auto prod = detail::as_rational(lhs) * detail::as_rational(rhs);
        if (!prod)
          return report_or_nullopt<result>(action, policy, errc::overflow,
                                           "rational overflow in mul");
        return result::from_raw(raw_cast<result>(*prod));
      }
      else
        return result::from_raw(raw_cast<result>(bnd::detail::rational::mul_unchecked(
            detail::as_rational(lhs), detail::as_rational(rhs))));
    }
    else if constexpr (IsIntegerAligned<L> && IsIntegerAligned<R> && IsIntegerAligned<result>)
    {
      result res;
      from_value(res, to_value(lhs) * to_value(rhs));
      return res;
    }
    else
    {
      // Result writes go through raw_from_offset so direct-storage results
      // get Lower<result> added back to recover the value.
      auto to_result = [](auto raw_offset)
      { return result::from_raw(raw_from_offset<result>(static_cast<umax>(raw_offset))); };

      // Normalize lhs.raw() / rhs.raw() to *offsets* regardless of L's / R's
      // storage shape. The formulas below all assume offset arithmetic.
      umax lhs_offset = IsDirectStorage<L>
          ? static_cast<umax>(raw_imax(lhs) - RawLo<L>)
          : static_cast<umax>(lhs.raw());
      umax rhs_offset = IsDirectStorage<R>
          ? static_cast<umax>(raw_imax(rhs) - RawLo<R>)
          : static_cast<umax>(rhs.raw());

      // Raws are typically small unsigned ints (uint8/16/32). C++ integral
      // promotion turns `raw * raw` into `int * int`, so any product above
      // INT_MAX is signed-overflow UB. Cast both factors to umax to force
      // the multiplication into 64-bit unsigned space before adding offsets.
      //
      // The four `if constexpr` branches below cover the four sign-quadrant
      // cases for the result interval. For any product `[loL,hiL]*[loR,hiR]`
      // the resulting Lower must be one of {loL*loR, loL*hiR, hiL*loR,
      // hiL*hiR}; we pick the matching branch and use sign-flipped helpers
      // (negative<L> / negative<R>) to reduce each case to the all-positive
      // formula. The trailing static_assert exists to catch any future grid
      // arithmetic change that would invalidate this case analysis.
      if constexpr (Lower<result> == (Lower<L> * Lower<R>).value())
      {
        return to_result(lhs_offset * rhs_offset
                         + lhs_offset * LowerIndex<R>
                         + rhs_offset * LowerIndex<L>);
      }

      if constexpr (Lower<result> == (Upper<L> * Upper<R>).value())
      { return multiplication<negative<L>, negative<R>>::mul(-lhs, -rhs, std::forward<P>(policy)); }

      if constexpr (Lower<result> == (Upper<L> * Lower<R>).value())
      {
        umax negLhs = NotchCount<L> - lhs_offset;
        return to_result(negLhs * LowerIndex<R>
                         + rhs_offset * UpperIndex<L>
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
} // namespace bnd

#endif // BNDmultiplicationHPP
