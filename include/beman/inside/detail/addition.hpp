// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_DETAIL_ADDITION_HPP
#define BEMAN_INSIDE_DETAIL_ADDITION_HPP

#include <beman/inside/detail/rep.hpp>
#include <beman/inside/generic.hpp>
#include <beman/inside/grid.hpp>
#include <beman/inside/policy.hpp>

//---------------------------------------------------------------------------
// addition — `add(L, R, policy, action) -> inside<G>`, G = grid_of<L> + grid_of<R>.
// The grid arithmetic is sound by construction (the result interval contains
// every runtime sum), so overflow can only happen on rational-raw results.
// Specialises on the storage shapes: rational result, mixed rational/integer,
// direct integer-space add, or both notch-offset (scale via lhs/rhs_widen).
//---------------------------------------------------------------------------
namespace beman::inside::detail
{
  template <insidable L, insidable R = L>
  struct addition
  {
    static_assert((grid_of<L> + grid_of<R>).has_value(),
      "addition: result grid's notch/interval exceeds the representable rational "
      "range — coarsen the operand grids");
    static constexpr grid result_grid = (grid_of<L> + grid_of<R>).value();
    // fp / representation propagation — shared rule in detail/rep.hpp.
    using rep_t = fp_rep<L, R, result_grid>;
    using result = inside<result_grid, rep_t::result_policy>;

    template <policy_flag F>
    static constexpr bool needs_overflow_check =
        rational_raw<result>
        && (has_any_flag(F, checked) || is_checked(policy_of<L>) || is_checked(policy_of<R>)
            || has_any_flag(F | policy_of<L> | policy_of<R>, exact))
        && !rational_add_is_safe(grid_of<L>, grid_of<R>);

    // Plain result when an overflow action takes the failure or no check is
    // needed; else std::expected<result, errc>.
    template <policy_flag F, typename A>
    using return_t = std::conditional_t<overflow_action<plain_t<A>> || !needs_overflow_check<F>,
                                        result, std::expected<result, errc>>;

    // Mixed integer-aligned / notch-offset fast path: with a unit-numerator
    // result notch 1/d, both operand offsets in result-notch units are exact
    // integer math — (to_value − Lower)·d for the integer-aligned operand,
    // raw·widen for the notch-offset one (offsets compose because
    // lower_of<result> = lower_of<L> + lower_of<R>). Gated on an index-raw result and
    // the result slot count fitting imax so no intermediate can overflow
    // (each operand contribution ≤ its own span/N ≤ the result slot count).
    static constexpr bool mixed_offset_ok = []{
      if constexpr (rational_raw<L> || rational_raw<R> || rational_raw<result>
                    || fp_raw<L> || fp_raw<R>          // double raws: no integer offset
                    || fp_raw<result> || !index_raw<result>
                    || (is_integer_aligned<L> && is_integer_aligned<R>)
                    || (index_raw<L> && index_raw<R>)
                    || notch_of<result> == 0 || notch_of<result>.Numerator != 1)
        return false;
      else
      {
        constexpr auto span = upper_of<result> - lower_of<result>;
        if (!span.has_value())
          return false;
        const auto slots = *span / notch_of<result>;
        return slots.has_value()
            && (*slots).Numerator
                 <= static_cast<umax>(std::numeric_limits<imax>::max());
      }
    }();

    // One operand's offset in result-notch units (see mixed_offset_ok).
    template <insidable X>
    static constexpr imax mixed_offset_units(X const& x, imax widen)
    {
      if constexpr (is_integer_aligned<X>)
      {
        constexpr imax den = static_cast<imax>(abs_den(notch_of<result>.Denominator));
        return (to_value(x) - lower_imax<X>) * den;
      }
      else
        return raw_imax(x) * widen;
    }

    // Result notch is gcd(NL, NR); scale each raw up to it before adding —
    // lhs_widen = NL/Nresult, rhs_widen = NR/Nresult (exact, Nresult divides both).
    // A continuous result (notch_of<result> == 0) has no widen (it takes the
    // rational path), so 1 stands in.
    static constexpr imax lhs_widen = (notch_of<result> == 0) ? imax{1}
        : (notch_of<L> / notch_of<result>).value_or(rational{1}).Numerator;
    static constexpr imax rhs_widen = (notch_of<result> == 0) ? imax{1}
        : (notch_of<R> / notch_of<result>).value_or(rational{1}).Numerator;

    template <policy_flag F = none, typename E = empty_ref, typename A = no_action>
    static constexpr auto add(L lhs, R rhs, policy<F, E> policy = {}, A&& action = {}) -> return_t<F, A>
  {
    result res;
    if constexpr (fp_raw<result>)
    {
      // Exact by construction, no snap: fp storage is kept only when the
      // result grid is double/float-exact (fp_rep), and grid values are notch
      // multiples, so the sum is itself a representable result-grid point and
      // the double add is exact. (Division still snaps — a quotient is not a
      // grid point.)
      res = result::from_raw(raw_cast<result>(as_double(lhs) + as_double(rhs)));
    }
    else if constexpr (rational_raw<result>)
    {
      if constexpr (needs_overflow_check<F>)
      {
        auto sum = rational::add(lhs,rhs);
        if (!sum) [[unlikely]]
          return report_or_unexpected<result>(action, policy, errc::overflow,
                                              "rational overflow in add");
        res = result::from_raw(*sum);
      }
      else
        res = result::from_raw(rational::add_unchecked(lhs, rhs));
    }
    else if constexpr (mixed_offset_ok)
    {
      // Mixed integer-aligned / notch-offset operands, pure integer offsets
      // (see mixed_offset_ok above).
      res = result::from_raw(raw_cast<result>(mixed_offset_units(lhs, lhs_widen)
                                            + mixed_offset_units(rhs, rhs_widen)));
    }
    else if constexpr (rational_raw<L> || rational_raw<R>
                       || !((is_integer_aligned<L> && is_integer_aligned<R>)
                            || (index_raw<L> && index_raw<R>)))
    {
      // Rational store: a rational-raw operand, or a mix the integer fast
      // paths can't express exactly (non-unit result notch numerator, or a
      // slot count past imax). Compute the exact rational sum and convert to
      // result's raw via raw_from_offset.
      auto sum = rational::add_unchecked(lhs,rhs);
      res = result::from_raw(raw_from_offset<result>(
          ((sum - lower_of<result>) / notch_of<result>).value().Numerator));
    }
    else if constexpr (is_integer_aligned<L> && is_integer_aligned<R>)
    {
      // Both operands are integer-valued (Notch and Lower integers), so the
      // value-space add is exact.
      from_value(res, to_value(lhs) + to_value(rhs));
    }
    else
    {
      // Both notch-offset: scale each raw to the result notch and add in offset
      // space (offsets compose because result Lower = lower_of<L> + lower_of<R>).
      res = result::from_raw(raw_cast<result>(raw_imax(lhs) * lhs_widen + raw_imax(rhs) * rhs_widen));
    }
    return res;
  }
  };
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_DETAIL_ADDITION_HPP
