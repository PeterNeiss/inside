//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDdivisionHPP
#define BNDdivisionHPP

#include "bound/generic.hpp"
#include "bound/grid.hpp"
#include "bound/policy.hpp"

//---------------------------------------------------------------------------
// division / modulo
//
// `division::div` returns `slim::optional<result>` because division by zero
// is always possible at runtime. Two code paths:
//   - native_div:   integer-aligned grids + `ignore_round` → use native
//                   integer division, return type is integer-grid bound.
//   - rational:     exact rational arithmetic; result grid is a rational
//                   interval (`bound<{rational}>`), can overflow under
//                   `checked`.
//
// `modulo::mod` is integer-only (the `native_mod` static_assert below
// hard-rejects rational/non-`ignore_round` grids) — non-integer remainders
// aren't well-defined on fractional notches.
//---------------------------------------------------------------------------
namespace bnd
{
  template <boundable L, boundable R = L, policy_flag F = none>
  struct division
  {
    // Native integer division fires only when all three conditions hold:
    //   1. `ignore_round` is set (caller accepts truncation toward zero),
    //   2. neither operand uses rational raw storage,
    //   3. both operands are integer-aligned (notch + lower have integer
    //      denominators).
    // Otherwise the rational path runs and returns an exact `bound<rational>`.
    static constexpr bool native_div =
        ((F | BoundPolicy<L> | BoundPolicy<R>) & ignore_round)
        && !IsRawRational<L> && !IsRawRational<R>
        && IsIntegerAligned<L> && IsIntegerAligned<R>;

    static constexpr grid result_grid = native_div
        ? grid{static_cast<imax>((*(Grid<L> / Grid<R>)).Interval.Lower),
               static_cast<imax>((*(Grid<L> / Grid<R>)).Interval.Upper)}
        : *(Grid<L> / Grid<R>);

    using result = bound<result_grid>;

    template <policy_flag G = F>
    static constexpr bool needs_overflow_check =
        ((G | F | BoundPolicy<L> | BoundPolicy<R>) & checked);

    template <typename A>
    using div_return_t = std::conditional_t<is_overflow_action<plain<A>>,
                                            result,
                                            slim::optional<result>>;

    template <policy_flag G = F, typename E = empty_ref, typename A = no_action>
    static constexpr div_return_t<A> div(L, R, policy<G, E> = {}, A&& = {});
  };

  //---------------------------------------------------------------------------
  // div
  //---------------------------------------------------------------------------
  template<boundable L, boundable R, policy_flag F>
  template<policy_flag G, typename E, typename A>
  constexpr auto division<L,R,F>::div(L lhs, R rhs, policy<G, E> policy, A&& action) -> div_return_t<A>
  {
    auto fail = [&](errc code, const char* what) -> div_return_t<A> {
      if constexpr (is_overflow_action<plain<A>>)
      { result res; action.fn(res, code); return res; }
      else
      {
        if constexpr (uses_error_ref_v<bnd::policy<G, E>>)
          policy.report(code, what);
        return slim::nullopt;
      }
    };

    if constexpr (native_div)
    {
      imax rhs_val = to_value(rhs);
      if (rhs_val == 0) return fail(errc::division_by_zero, "division by zero in div");
      result res;
      from_value(res, to_value(lhs) / rhs_val);
      return res;
    }
    else if constexpr (needs_overflow_check<G>)
    {
      auto rhs_r = static_cast<rational>(rhs);
      if (rhs_r.Numerator == 0) return fail(errc::division_by_zero, "division by zero in div");
      auto q = static_cast<rational>(lhs) / rhs_r;
      if (!q) return fail(errc::overflow, "rational overflow in div");
      result res; res.Raw = *q; return res;
    }
    else
    {
      auto rhs_r = static_cast<rational>(rhs);
      if (rhs_r.Numerator == 0) return fail(errc::division_by_zero, "division by zero in div");
      result res;
      res.Raw = rational::div_unchecked(static_cast<rational>(lhs), rhs_r);
      return res;
    }
  }
  //---------------------------------------------------------------------------
  // modulo (requires integer-valued grids + ignore_round)
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_flag F = none>
  struct modulo
  {
    static constexpr bool native_mod =
        ((F | BoundPolicy<L> | BoundPolicy<R>) & ignore_round)
        && !IsRawRational<L> && !IsRawRational<R>
        && IsIntegerAligned<L> && IsIntegerAligned<R>;

    // Hard requirement, not a fallback: there is no exact-rational modulo —
    // `a mod b` is only defined when both operands are integers. The grid
    // must therefore be integer-aligned and `ignore_round` must be set
    // (modulo on rational grids would have to round-then-mod, which is not
    // a meaningful operation).
    static_assert(native_mod, "modulo requires integer-valued grids and ignore_round");

    static constexpr imax max_rem =
        std::max(abs_den(static_cast<imax>(Lower<R>)),
                 abs_den(static_cast<imax>(Upper<R>))) - 1;

    static constexpr grid result_grid =
        (static_cast<imax>(Lower<L>) < 0)
        ? grid{-max_rem, max_rem}
        : grid{static_cast<imax>(0), max_rem};

    using result = bound<result_grid>;

    template <typename A>
    using mod_return_t = std::conditional_t<is_overflow_action<plain<A>>,
                                            result,
                                            slim::optional<result>>;

    template <policy_flag G = F, typename E = empty_ref, typename A = no_action>
    static constexpr mod_return_t<A> mod(L, R, policy<G, E> = {}, A&& = {});
  };

  template<boundable L, boundable R, policy_flag F>
  template<policy_flag G, typename E, typename A>
  constexpr auto modulo<L,R,F>::mod(L lhs, R rhs, policy<G, E> policy, A&& action) -> mod_return_t<A>
  {
    imax rhs_val = to_value(rhs);
    if (rhs_val == 0)
    {
      if constexpr (is_overflow_action<plain<A>>)
      { result res; action.fn(res, errc::division_by_zero); return res; }
      else
      {
        if constexpr (uses_error_ref_v<bnd::policy<G, E>>)
          policy.report(errc::division_by_zero, "division by zero in mod");
        return slim::nullopt;
      }
    }
    result res;
    from_value(res, to_value(lhs) % rhs_val);
    return res;
  }
} // namespace bnd

#endif // BNDdivisionHPP
