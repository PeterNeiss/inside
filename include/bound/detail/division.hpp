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
//   - native_div:   integer-aligned grids + `snapping` → use native
//                   integer division, return type is integer-grid bound.
//   - rational:     exact rational arithmetic; result grid is a rational
//                   interval (`bound<{rational}>`), can overflow under
//                   `checked`.
//
// `modulo::mod` is integer-only (the `native_mod` static_assert below
// hard-rejects rational/non-`snapping` grids) — non-integer remainders
// aren't well-defined on fractional notches.
//---------------------------------------------------------------------------
namespace bnd::detail
{
  // Both operands are plain integer grids and the caller accepted integer
  // truncation (snapping) — the prerequisite for native integer div / mod.
  template <boundable L, boundable R, policy_flag F>
  inline constexpr bool integer_native_ops =
      ((F | BoundPolicy<L> | BoundPolicy<R>) & snapping)
      && !rational_raw<L> && !rational_raw<R>
      && IsIntegerAligned<L> && IsIntegerAligned<R>;

  //---------------------------------------------------------------------------
  // Rounding mode for the native (integer / Q-format) div & mod paths.
  //
  // The native paths fire when `snapping` is set; *which* rounding mode applies
  // is decided here from the combined policy flags, with the same precedence as
  // assignment.hpp (nearest → floor → ceil → half_even → truncate). Without a
  // mode bit, `snapping` alone means truncate-toward-zero (== the `truncated`
  // convenience policy), preserving the historical native-division semantics.
  //
  // The runtime quotient (`div_rounded` / `round_uquotient`) and the compile-
  // time result-grid endpoints (`round_rat_lo` / `round_rat_hi`) MUST agree on
  // the mode, or a rounded result could land outside its own grid. Both read
  // the same `div_round_mode(F | BoundPolicy<L> | BoundPolicy<R>)`.
  //---------------------------------------------------------------------------
  enum class round_mode { truncate, nearest, floor, ceil, half_even };

  constexpr round_mode div_round_mode(policy_flag eff) noexcept
  {
    if ((eff & round_nearest)   == round_nearest)   return round_mode::nearest;
    if ((eff & round_floor)     == round_floor)     return round_mode::floor;
    if ((eff & round_ceil)      == round_ceil)      return round_mode::ceil;
    if ((eff & round_half_even) == round_half_even) return round_mode::half_even;
    return round_mode::truncate;
  }

  // |v| as umax, safe for imax_min (negating it would be UB).
  constexpr umax uabs(imax v) noexcept
  { return v < 0 ? ~static_cast<umax>(v) + 1u : static_cast<umax>(v); }

  // Round the signed exact quotient a/b (b != 0) to an integer per `m`.
  constexpr imax div_rounded(imax a, imax b, round_mode m) noexcept
  {
    const imax t = a / b;                     // C++ truncation toward zero
    const imax r = a % b;                     // sign of a, |r| < |b|
    if (r == 0 || m == round_mode::truncate) return t;
    const bool neg = (a < 0) != (b < 0);      // exact quotient is negative
    const umax ar = uabs(r), ab = uabs(b);    // ab - ar is safe: 0 < ar < ab
    switch (m)
    {
      case round_mode::floor:   return neg ? t - 1 : t;
      case round_mode::ceil:    return neg ? t : t + 1;
      case round_mode::nearest:                       // half away from zero
        return (ar >= ab - ar) ? (neg ? t - 1 : t + 1) : t;
      case round_mode::half_even:
        if (ar < ab - ar) return t;
        if (ar > ab - ar) return neg ? t - 1 : t + 1;
        return (t & 1) == 0 ? t : (neg ? t - 1 : t + 1);   // tie → even
      default:                  return t;
    }
  }

  // Round a non-negative quotient num/den (den != 0) per `m`. Used by the
  // Q-format path, whose raws are non-negative (Lower == 0).
  constexpr umax round_uquotient(umax num, umax den, round_mode m) noexcept
  {
    const umax t = num / den, r = num % den;
    if (r == 0 || m == round_mode::truncate) return t;
    switch (m)
    {
      case round_mode::floor:   return t;             // non-negative: floor == trunc
      case round_mode::ceil:    return t + 1;
      case round_mode::nearest: return (r >= den - r) ? t + 1 : t;
      case round_mode::half_even:
        if (r < den - r) return t;
        if (r > den - r) return t + 1;
        return (t & 1) == 0 ? t : t + 1;
      default:                  return t;
    }
  }

  // Compile-time rounding of a quotient-interval endpoint to an integer index.
  // `lo`/`hi` differ only for half_even, where the exact tie-rounded endpoint
  // is bracketed by [floor, ceil] (a safe, ≤1-wide superset) rather than
  // reproducing the parity rule at compile time.
  constexpr imax round_rat_lo(rational q, round_mode m) noexcept
  {
    switch (m)
    {
      case round_mode::nearest:   return q.round();
      case round_mode::floor:     return q.floor();
      case round_mode::ceil:      return q.ceil();
      case round_mode::half_even: return q.floor();
      default:                    return q.trunc();
    }
  }
  constexpr imax round_rat_hi(rational q, round_mode m) noexcept
  {
    switch (m)
    {
      case round_mode::nearest:   return q.round();
      case round_mode::floor:     return q.floor();
      case round_mode::ceil:      return q.ceil();
      case round_mode::half_even: return q.ceil();
      default:                    return q.trunc();
    }
  }

  template <boundable L, boundable R = L, policy_flag F = none>
  struct division
  {
    // Native integer division has two flavours, both gated on `snapping`
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
        ((F | BoundPolicy<L> | BoundPolicy<R>) & snapping)
        && IsQFormat<L> && IsQFormat<R>
        && Notch<L> == Notch<R>;

    static constexpr bool native_div = native_div_integer || native_div_qformat;

    // The rounding mode for the native paths (shared by the grid and runtime).
    static constexpr round_mode rmode =
        div_round_mode(F | BoundPolicy<L> | BoundPolicy<R>);

    // Native-integer endpoints are rounded with the *same* mode as the runtime
    // quotient, so e.g. round_ceil can land a result one above the truncated
    // bound without escaping its grid. (The Q-format extreme is always exact —
    // max value / smallest divisor divides evenly — so its grid is unchanged.)
    static constexpr grid result_grid =
        native_div_integer
            ? grid{round_rat_lo((*(Grid<L> / Grid<R>)).Interval.Lower, rmode),
                   round_rat_hi((*(Grid<L> / Grid<R>)).Interval.Upper, rmode)}
      : native_div_qformat
            ? grid{interval{rational{0}, (Upper<L> / Notch<R>).value()}, Notch<L>}
            : *(Grid<L> / Grid<R>);

    static constexpr bool any_real =
        (BoundPolicy<L> & bnd::real) == bnd::real || (BoundPolicy<R> & bnd::real) == bnd::real;
    // Carry every representation flag of both operands (a mixed pair resolves
    // widest-wins at storage selection: exact > real > direct > indexed).
    static constexpr policy_flag rep =
        ((BoundPolicy<L> | BoundPolicy<R>) & (bnd::exact | bnd::direct | bnd::indexed))
        | (any_real ? bnd::real : none);
    using result = bound<result_grid, rep != none ? rep : checked>;

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
    if constexpr (real_raw<result>)
    {
      (void)policy; (void)action;
      return result::from_raw(Grid<result>.snap_double(as_double(lhs) / as_double(rhs)));
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
      constexpr umax N = abs_den(Notch<L>.Denominator);
      return result::from_raw(raw_cast<result>(round_uquotient(
          static_cast<umax>(lhs.raw()) * N, static_cast<umax>(rhs.raw()), rmode)));
    }
    else if constexpr (native_div_integer)
    {
      imax rhs_val = to_value(rhs);
      if constexpr (!zero_unchecked)
        if (rhs_val == 0) return fail(errc::division_by_zero, "division by zero in div");
      result res;
      from_value(res, div_rounded(to_value(lhs), rhs_val, rmode));
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
  // modulo (requires integer-valued grids + snapping)
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_flag F = none>
  struct modulo
  {
    static constexpr bool native_mod = integer_native_ops<L, R, F>;

    // Hard requirement, not a fallback: there is no exact-rational modulo —
    // `a mod b` is only defined when both operands are integers. The grid
    // must therefore be integer-aligned and `snapping` must be set
    // (modulo on rational grids would have to round-then-mod, which is not
    // a meaningful operation).
    static_assert(native_mod, "modulo requires integer-valued grids and snapping");

    static constexpr imax max_rem =
        std::max(abs_den(LowerImax<R>), abs_den(UpperImax<R>)) - 1;

    // The remainder is consistent with the rounded quotient: r = a − round(a/b)·b
    // (so `(a/b)·b + a%b == a` holds for every mode). Under plain truncation the
    // remainder takes the dividend's sign — non-negative when the dividend grid
    // is non-negative. Any directional mode (floor/ceil/nearest/half_even) can
    // flip the remainder's sign regardless of the dividend, so the grid widens
    // to the symmetric ±max_rem. |r| ≤ max_rem holds for every mode.
    static constexpr round_mode rmode =
        div_round_mode(F | BoundPolicy<L> | BoundPolicy<R>);

    static constexpr grid result_grid =
        (rmode == round_mode::truncate && LowerImax<L> >= 0)
        ? grid{imax{0}, max_rem}
        : grid{-max_rem, max_rem};

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
    // Remainder consistent with the rounded quotient (truncate → C++ `%`).
    const imax lhs_val = to_value(lhs);
    from_value(res, lhs_val - div_rounded(lhs_val, rhs_val, rmode) * rhs_val);
    return res;
  }
} // namespace bnd::detail

#endif // BNDdivisionHPP
