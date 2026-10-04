// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_DETAIL_ADDITION_HPP
#define BEMAN_INSIDE_DETAIL_ADDITION_HPP

#include <beman/inside/detail/rep.hpp>
#include <beman/inside/generic.hpp>
#include <beman/inside/detail/wide_value.hpp>
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
      static_assert(!exact_valued<L> && !exact_valued<R>,
        "addition: a wide-index operand with a continuous result is not supported yet");
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
    else if constexpr (point_raw<result>)
      res = result::from_raw(raw_t<result>{});        // point + point: a point
    else if constexpr (integer_raw<L> && integer_raw<R>)
    {
      // Integer raws: add the value indices in result-notch units (the result
      // notch is gcd(N_L, N_R), so it divides both), in imax or by wrapping
      // arithmetic (wide_value.hpp). Exact for every grid, at any width.
      using W = index_work_t<result, L, notch_of<result>, R, notch_of<result>>;
      res = from_value_index<result>(value_in_units<W, notch_of<result>>(lhs)
                                   + value_in_units<W, notch_of<result>>(rhs));
    }
    else if constexpr (exact_valued<result>)
      // An fp or rational operand into a result with more than 2^64 slots.
      res = exact_result<result>(exact_of(lhs) + exact_of(rhs));
    else
    {
      // An fp or rational operand into an integer result: the exact rational
      // sum, converted to the result's raw.
      auto sum = rational::add_unchecked(lhs,rhs);
      res = result::from_raw(raw_from_offset<result>(
          ((sum - detail::lower64<result>) / detail::notch64<result>).value().Numerator));
    }
    return res;
  }
  };
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_DETAIL_ADDITION_HPP
