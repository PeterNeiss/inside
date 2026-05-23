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
  template <boundable L, boundable R = L>
  struct multiplication
  {
    using result = bound<(Grid<L> * Grid<R>).value()>;

    template <typename P>
    static constexpr bool needs_overflow_check =
        is_raw_rational<result>
        && (((BoundPolicy<L> | BoundPolicy<R>) & checked) || plain<P>::test(checked));

    template <typename P>
    using return_type_for = std::conditional_t<needs_overflow_check<P>,
                                               slim::optional<result>,
                                               result>;

    template <typename P, typename A>
    using mul_return_t = std::conditional_t<is_overflow_action<plain<A>>,
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
    if constexpr (is_raw_rational<result>)
    {
      if constexpr (needs_overflow_check<P>)
      {
        auto prod = as_rational(lhs) * as_rational(rhs);
        if (!prod)
        {
          if constexpr (is_overflow_action<plain<A>>)
          { result res; action.fn(res, errc::overflow); return res; }
          else
          {
            if constexpr (uses_error_ref_v<plain<P>>)
              policy.report(errc::overflow, "rational overflow in mul");
            return slim::nullopt;
          }
        }
        result res; res.Raw = raw_cast<result>(*prod); return res;
      }
      else
      {
        result res;
        res.Raw = raw_cast<result>(rational::mul_unchecked(
            as_rational(lhs), as_rational(rhs)));
        return res;
      }
    }
    else if constexpr (is_integer_aligned<L> && is_integer_aligned<R> && is_integer_aligned<result>)
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
      { result res; res.Raw = raw_from_offset<result>(static_cast<umax>(raw_offset)); return res; };

      // Normalize lhs.Raw / rhs.Raw to *offsets* regardless of L's / R's
      // storage shape. The formulas below all assume offset arithmetic.
      umax lhs_offset = is_direct_storage<L>
          ? static_cast<umax>(signed_raw(lhs) - RawLo<L>)
          : static_cast<umax>(lhs.Raw);
      umax rhs_offset = is_direct_storage<R>
          ? static_cast<umax>(signed_raw(rhs) - RawLo<R>)
          : static_cast<umax>(rhs.Raw);

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
