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
    // Native integer division has two flavours, both gated on `ignore_round`
    // (the caller has accepted integer-truncation semantics):
    //
    //   native_div_integer — both operands integer-aligned (Notch.Denominator
    //                        == 1). Result is an integer-grid bound; the
    //                        formula is plain `lhs_value / rhs_value`.
    //   native_div_qformat — both operands share the same Q-format
    //                        (Notch = 1/N, Lower = 0). The formula
    //                        `(lhs.Raw * N) / rhs.Raw` reproduces the native
    //                        `(a << log2(N)) / b` idiom; the result is
    //                        another Q-format with the same Notch.
    //
    // Otherwise the exact-rational path runs and returns `bound<rational>`.
    static constexpr bool native_div_integer =
        ((F | BoundPolicy<L> | BoundPolicy<R>) & ignore_round)
        && !IsRawRational<L> && !IsRawRational<R>
        && IsIntegerAligned<L> && IsIntegerAligned<R>;

    static constexpr bool native_div_qformat =
        ((F | BoundPolicy<L> | BoundPolicy<R>) & ignore_round)
        && IsQFormat<L> && IsQFormat<R>
        && Notch<L> == Notch<R>;

    static constexpr bool native_div = native_div_integer || native_div_qformat;

    static constexpr grid result_grid =
        native_div_integer
            ? grid{(*(Grid<L> / Grid<R>)).Interval.Lower.trunc(),
                   (*(Grid<L> / Grid<R>)).Interval.Upper.trunc()}
      : native_div_qformat
            ? grid{interval{0_r, (Upper<L> / Notch<R>).value()}, Notch<L>}
            : *(Grid<L> / Grid<R>);

    using result = bound<result_grid>;

    template <policy_flag G = F>
    static constexpr bool needs_overflow_check =
        ((G | F | BoundPolicy<L> | BoundPolicy<R>) & checked);

    template <typename A>
    using div_return_t = std::conditional_t<overflow_action<plain<A>>,
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
      return report_or_nullopt<result>(action, policy, code, what);
    };

    if constexpr (native_div_qformat)
    {
      // rhs.Raw == 0 iff rhs.value == 0 (Lower<R> == 0 by IsQFormat).
      // Formula matches `(a << log2(N)) / b` which the compiler folds when
      // N is a power of two — i.e. literally the native Q-format idiom.
      if (rhs.Raw == 0) return fail(errc::division_by_zero, "division by zero in div");
      constexpr umax N = static_cast<umax>(abs_den(Notch<L>.Denominator));
      result res;
      res.Raw = raw_cast<result>(
          (static_cast<umax>(lhs.Raw) * N) / static_cast<umax>(rhs.Raw));
      return res;
    }
    else if constexpr (native_div_integer)
    {
      imax rhs_val = to_value(rhs);
      if (rhs_val == 0) return fail(errc::division_by_zero, "division by zero in div");
      result res;
      from_value(res, to_value(lhs) / rhs_val);
      return res;
    }
    else if constexpr (needs_overflow_check<G>)
    {
      rational rhs_r = rhs;
      if (rhs_r.Numerator == 0) return fail(errc::division_by_zero, "division by zero in div");
      auto q = as_rational(lhs) / rhs_r;
      if (!q) return fail(errc::overflow, "rational overflow in div");
      result res; res.Raw = *q; return res;
    }
    else
    {
      rational rhs_r = rhs;
      if (rhs_r.Numerator == 0) return fail(errc::division_by_zero, "division by zero in div");
      result res;
      res.Raw = rational::div_unchecked(as_rational(lhs), rhs_r);
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
        std::max(abs_den(LowerImax<R>), abs_den(UpperImax<R>)) - 1;

    static constexpr grid result_grid =
        (LowerImax<L> < 0)
        ? grid{-max_rem, max_rem}
        : grid{imax{0}, max_rem};

    using result = bound<result_grid>;

    template <typename A>
    using mod_return_t = std::conditional_t<overflow_action<plain<A>>,
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
      return report_or_nullopt<result>(action, policy, errc::division_by_zero,
                                       "division by zero in mod");
    result res;
    from_value(res, to_value(lhs) % rhs_val);
    return res;
  }
} // namespace bnd

#endif // BNDdivisionHPP
