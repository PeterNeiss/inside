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
namespace bnd::detail
{
  // Both operands are plain integer grids and the caller accepted integer
  // truncation (ignore_round) — the prerequisite for native integer div / mod.
  template <boundable L, boundable R, policy_flag F>
  inline constexpr bool integer_native_ops =
      ((F | BoundPolicy<L> | BoundPolicy<R>) & ignore_round)
      && storage_of<L> != storage::rational && storage_of<R> != storage::rational
      && IsIntegerAligned<L> && IsIntegerAligned<R>;

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
    static constexpr bool native_div_integer = integer_native_ops<L, R, F>;

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
            ? grid{interval{rational{0}, (Upper<L> / Notch<R>).value()}, Notch<L>}
            : *(Grid<L> / Grid<R>);

    static constexpr bool any_real =
        (BoundPolicy<L> & bnd::real) == bnd::real || (BoundPolicy<R> & bnd::real) == bnd::real;
    using result = bound<result_grid, any_real ? bnd::real : checked>;

    template <policy_flag G = F>
    static constexpr bool needs_overflow_check =
        ((G | F | BoundPolicy<L> | BoundPolicy<R>) & checked);

    // For a *nonzero* divisor the op can still fail only on the checked
    // rational path (overflow). The native paths and the unchecked rational
    // path cannot fail once zero is ruled out. So when the divisor's grid
    // excludes zero AND this is false, `div` cannot fail at all and returns a
    // plain `result` rather than `slim::optional<result>`.
    static constexpr bool may_overflow_nonzero =
        !native_div && (needs_overflow_check<F> != 0);

    template <typename A>
    using div_return_t = std::conditional_t<
        any_real,
        result,                                       // double division: never fails (IEEE)
        std::conditional_t<
            overflow_action<plain<A>> || (DivisorExcludesZero<R> && !may_overflow_nonzero),
            result,
            slim::optional<result>>>;

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
    if constexpr (storage_of<result> == storage::real)
    {
      (void)policy; (void)action;
      return result::from_raw(as_double(lhs) / as_double(rhs));
    }
    else
    {
    // `fail` must stay well-formed for every configuration, including the one
    // where `div_return_t` has narrowed to plain `result` because the divisor
    // excludes zero and the op cannot overflow. In that case every call to
    // `fail` has been removed by the `if constexpr` guards below, so the final
    // arm is dead — it exists only to satisfy the return type.
    auto fail = [&](errc code, const char* what) -> div_return_t<A> {
      if constexpr (overflow_action<plain<A>>)
        return report_or_nullopt<result>(action, policy, code, what);   // -> result
      else if constexpr (!DivisorExcludesZero<R> || may_overflow_nonzero)
        return report_or_nullopt<result>(action, policy, code, what);   // -> optional<result>
      else
        return result{};   // unreachable: divisor excludes zero, op cannot fail
    };

    // The div-by-zero check is elided when R's grid statically excludes zero,
    // OR when `ignore_zero` is set (e.g. `unsafe`): a zero divisor is then
    // undefined behavior, matching the compound `/= 0` no-op contract. (The
    // `fail` arms above stay keyed on DivisorExcludesZero, which is what
    // narrows the *return type* — `ignore_zero` does not change it.)
    constexpr bool zero_unchecked = DivisorExcludesZero<R>
        || (((G | F | BoundPolicy<L> | BoundPolicy<R>) & ignore_zero) != 0);

    if constexpr (native_div_qformat)
    {
      // rhs.Raw == 0 iff rhs.value == 0 (Lower<R> == 0 by IsQFormat).
      // Formula matches `(a << log2(N)) / b` which the compiler folds when
      // N is a power of two — i.e. literally the native Q-format idiom.
      if constexpr (!zero_unchecked)
        if (rhs.raw() == 0) return fail(errc::division_by_zero, "division by zero in div");
      constexpr umax N = static_cast<umax>(abs_den(Notch<L>.Denominator));
      return result::from_raw(raw_cast<result>(
          (static_cast<umax>(lhs.raw()) * N) / static_cast<umax>(rhs.raw())));
    }
    else if constexpr (native_div_integer)
    {
      imax rhs_val = to_value(rhs);
      if constexpr (!zero_unchecked)
        if (rhs_val == 0) return fail(errc::division_by_zero, "division by zero in div");
      result res;
      from_value(res, to_value(lhs) / rhs_val);
      return res;
    }
    else if constexpr (needs_overflow_check<G>)
    {
      rational rhs_r = rhs;
      if constexpr (!zero_unchecked)
        if (rhs_r.Numerator == 0) return fail(errc::division_by_zero, "division by zero in div");
      auto q = as_rational(lhs) / rhs_r;
      if (!q) return fail(errc::overflow, "rational overflow in div");
      return result::from_raw(*q);
    }
    else
    {
      rational rhs_r = rhs;
      if constexpr (!zero_unchecked)
        if (rhs_r.Numerator == 0) return fail(errc::division_by_zero, "division by zero in div");
      return result::from_raw(rational::div_unchecked(as_rational(lhs), rhs_r));
    }
    }  // end else (non-real)
  }
  //---------------------------------------------------------------------------
  // modulo (requires integer-valued grids + ignore_round)
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_flag F = none>
  struct modulo
  {
    static constexpr bool native_mod = integer_native_ops<L, R, F>;

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

    // Modulo never overflows (the remainder always fits result_grid), so the
    // only failure is a zero divisor. When the divisor's grid excludes zero
    // the op cannot fail and returns a plain `result`.
    template <typename A>
    using mod_return_t = std::conditional_t<
        overflow_action<plain<A>> || DivisorExcludesZero<R>,
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
    // The zero check is elided when R's grid statically excludes zero (then
    // `mod_return_t` is a plain `result`) OR when `ignore_zero` is set (e.g.
    // `unsafe`): a zero divisor is then undefined behavior, matching the
    // compound `%= 0` no-op contract. When kept, `report_or_nullopt` matches
    // the optional return type; when elided, the tail returns a plain result.
    constexpr bool zero_unchecked = DivisorExcludesZero<R>
        || (((G | F | BoundPolicy<L> | BoundPolicy<R>) & ignore_zero) != 0);
    if constexpr (!zero_unchecked)
      if (rhs_val == 0)
        return report_or_nullopt<result>(action, policy, errc::division_by_zero,
                                         "division by zero in mod");
    result res;
    from_value(res, to_value(lhs) % rhs_val);
    return res;
  }
} // namespace bnd::detail

#endif // BNDdivisionHPP
