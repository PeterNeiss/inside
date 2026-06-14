//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDadditionHPP
#define BNDadditionHPP

#include "bound/generic.hpp"
#include "bound/grid.hpp"
#include "bound/policy.hpp"

//---------------------------------------------------------------------------
// addition — `add(L, R, policy, action) -> bound<G>`, G = Grid<L> + Grid<R>.
// The grid arithmetic is sound by construction (the result interval contains
// every runtime sum), so overflow can only happen on rational-raw results.
// Specialises on the storage shapes: rational result, mixed rational/integer,
// direct integer-space add, or both notch-offset (scale via lhs/rhs_widen).
//---------------------------------------------------------------------------
namespace bnd::detail
{
  template <boundable L, boundable R = L>
  struct addition
  {
    // Propagate `real`: an op on math operands yields a math operand (sum of
    // dyadic grids is dyadic, so the result stays double-backed).
    static constexpr bool any_real =
        (BoundPolicy<L> & bnd::real) == bnd::real || (BoundPolicy<R> & bnd::real) == bnd::real;
    // Carry both operands' representation flags (widest-wins at storage selection).
    static constexpr policy_flag rep =
        ((BoundPolicy<L> | BoundPolicy<R>) & (bnd::exact | bnd::direct | bnd::indexed))
        | (any_real ? bnd::real : none);
    using result = bound<(Grid<L> + Grid<R>).value(), rep != none ? rep : checked>;

    template <policy_flag F>
    static constexpr bool needs_overflow_check =
        rational_raw<result>
        && ((F | BoundPolicy<L> | BoundPolicy<R>) & checked)
        && !rational_add_is_safe(Grid<L>, Grid<R>);

    template <policy_flag F = none>
    using return_type_for = std::conditional_t<needs_overflow_check<F>,
                                               slim::optional<result>,
                                               result>;

    template <policy_flag F, typename A>
    using add_return_t = std::conditional_t<overflow_action<plain<A>>,
                                            result,
                                            return_type_for<F>>;

    // Result notch is gcd(NL, NR); scale each raw up to it before adding —
    // lhs_widen = NL/Nresult, rhs_widen = NR/Nresult (exact, Nresult divides both).
    static constexpr imax lhs_widen = (Notch<L> / Notch<result>).value_or(rational{1}).Numerator;
    static constexpr imax rhs_widen = (Notch<R> / Notch<result>).value_or(rational{1}).Numerator;

    template <policy_flag F = none, typename E = empty_ref, typename A = no_action>
    static constexpr add_return_t<F, A> add(L, R, policy<F, E> = {}, A&& = {});
  };

  //---------------------------------------------------------------------------
  // add
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<policy_flag F, typename E, typename A>
  constexpr auto addition<L,R>::add(L lhs, R rhs, policy<F, E> policy, A&& action) -> add_return_t<F, A>
  {
    result res;
    if constexpr (real_raw<result>)
    {
      res = result::from_raw(Grid<result>.snap_double(as_double(lhs) + as_double(rhs)));
    }
    else if constexpr (rational_raw<result>)
    {
      if constexpr (needs_overflow_check<F>)
      {
        auto sum = rational::add(lhs,rhs);
        if (!sum)
          return report_or_nullopt<result>(action, policy, errc::overflow,
                                           "rational overflow in add");
        res = result::from_raw(*sum);
      }
      else
        res = result::from_raw(rational::add_unchecked(lhs, rhs));
    }
    else if constexpr (rational_raw<L> || rational_raw<R>
                       || !((IsIntegerAligned<L> && IsIntegerAligned<R>)
                            || (index_raw<L> && index_raw<R>)))
    {
      // Rational store: a rational-raw operand, or a direct integer bound mixed
      // with a fractional notch-offset one (where neither to_value nor offset-widen
      // is exact — to_value would truncate the fractional operand). Compute the
      // exact rational sum and convert to result's raw via raw_from_offset.
      auto sum = rational::add_unchecked(lhs,rhs);
      res = result::from_raw(raw_from_offset<result>(
          ((sum - Lower<result>) / Notch<result>).value().Numerator));
    }
    else if constexpr (IsIntegerAligned<L> && IsIntegerAligned<R>)
    {
      // Both operands are integer-valued (Notch and Lower integers), so the
      // value-space add is exact.
      from_value(res, to_value(lhs) + to_value(rhs));
    }
    else
    {
      // Both notch-offset: scale each raw to the result notch and add in offset
      // space (offsets compose because result Lower = Lower<L> + Lower<R>).
      res = result::from_raw(raw_cast<result>(raw_imax(lhs) * lhs_widen + raw_imax(rhs) * rhs_widen));
    }
    return res;
  }
} // namespace bnd::detail

#endif // BNDadditionHPP
