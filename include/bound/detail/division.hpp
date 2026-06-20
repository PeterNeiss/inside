//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDdivisionHPP
#define BNDdivisionHPP

#include "bound/generic.hpp"
#include "bound/grid.hpp"
#include "bound/policy.hpp"

//---------------------------------------------------------------------------
// division / modulo. `division::div` returns optional<result> (division by zero
// is always runtime-possible). Two paths: native (integer-aligned grids +
// snap → native integer division) and rational (exact, can overflow under
// checked). `modulo::mod` is integer-only — non-integer remainders aren't
// well-defined on fractional notches.
//---------------------------------------------------------------------------
namespace bnd::detail
{
  // Both operands are plain integer grids and the caller accepted integer
  // truncation (snap) — the prerequisite for native integer div / mod.
  template <boundable L, boundable R, policy_flag F>
  inline constexpr bool integer_native_ops =
      ((F | BoundPolicy<L> | BoundPolicy<R>) & snap)
      && !rational_raw<L> && !rational_raw<R>
      && IsIntegerAligned<L> && IsIntegerAligned<R>;

  //---------------------------------------------------------------------------
  // Rounding mode for the native div & mod paths (fire when `snap` is set).
  // Decided from the combined flags with assignment.hpp's precedence (nearest →
  // floor → ceil → half_even → trunc); `snap` alone is truncate-toward-zero.
  // The runtime quotient and the compile-time grid endpoints MUST agree on the
  // mode (both read div_round_mode), or a result could escape its own grid.
  //---------------------------------------------------------------------------
  enum class round_mode { trunc, nearest, floor, ceil, half_even };

  constexpr round_mode div_round_mode(policy_flag eff) noexcept
  {
    if ((eff & round_nearest)   == round_nearest)   return round_mode::nearest;
    if ((eff & round_floor)     == round_floor)     return round_mode::floor;
    if ((eff & round_ceil)      == round_ceil)      return round_mode::ceil;
    if (has_flag(eff, round_half_even)) return round_mode::half_even;
    return round_mode::trunc;
  }

  // |v| as umax, safe for imax_min (negating it would be UB).
  constexpr umax uabs(imax v) noexcept
  { return v < 0 ? ~static_cast<umax>(v) + 1u : static_cast<umax>(v); }

  // Round the signed exact quotient a/b (b != 0) to an integer per `m`.
  constexpr imax div_rounded(imax a, imax b, round_mode m) noexcept
  {
    const imax t = a / b;                     // C++ truncation toward zero
    const imax r = a % b;                     // sign of a, |r| < |b|
    if (r == 0 || m == round_mode::trunc) return t;
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
    if (r == 0 || m == round_mode::trunc) return t;
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
  // lo/hi differ only for half_even, where the endpoint is bracketed by
  // [floor, ceil] rather than reproducing the parity rule at compile time.
  constexpr imax round_rat_lo(rational q, round_mode m) noexcept
  {
    switch (m)
    {
      case round_mode::nearest:   return round(q);
      case round_mode::floor:     return floor(q);
      case round_mode::ceil:      return ceil(q);
      case round_mode::half_even: return floor(q);
      default:                    return trunc(q);
    }
  }
  constexpr imax round_rat_hi(rational q, round_mode m) noexcept
  {
    switch (m)
    {
      case round_mode::nearest:   return round(q);
      case round_mode::floor:     return floor(q);
      case round_mode::ceil:      return ceil(q);
      case round_mode::half_even: return ceil(q);
      default:                    return trunc(q);
    }
  }

  template <boundable L, boundable R = L, policy_flag F = none>
  struct division
  {
    // Native integer division, two flavours gated on `snap`:
    //   native_div_integer — both operands integer-aligned; formula `a / b`.
    //   native_div_qformat — both same Q-format (Notch = 1/N, Lower = 0); formula
    //                        `(a·N)/b` (the native `(a << log2 N)/b` idiom).
    // Otherwise the exact-rational path returns bound<rational>.
    static constexpr bool native_div_integer = integer_native_ops<L, R, F>;

    static constexpr bool native_div_qformat =
        ((F | BoundPolicy<L> | BoundPolicy<R>) & snap)
        && IsQFormat<L> && IsQFormat<R>
        && Notch<L> == Notch<R>;

    static constexpr bool native_div = native_div_integer || native_div_qformat;

    // The rounding mode for the native paths (shared by the grid and runtime).
    static constexpr round_mode rmode =
        div_round_mode(F | BoundPolicy<L> | BoundPolicy<R>);

    // A clear diagnostic when the result grid is unrepresentable, instead of the
    // raw optional-deref / .value() below failing cryptically (mirrors add/mul).
    static_assert(native_div_qformat || (Grid<L> / Grid<R>).has_value(),
      "division: result grid not representable (notch/interval exceeds the "
      "representable rational range) — coarsen the operand grids");
    static_assert(!native_div_qformat || (Upper<L> / Notch<R>).has_value(),
      "division: Q-format result grid not representable — coarsen the operand grids");

    // Native-integer endpoints rounded with the same mode as the runtime
    // quotient, so e.g. round_ceil can't escape the grid. (The Q-format extreme
    // is always exact, so its grid is unchanged.)
    static constexpr grid result_grid =
        native_div_integer
            ? grid{round_rat_lo((*(Grid<L> / Grid<R>)).Interval.Lower, rmode),
                   round_rat_hi((*(Grid<L> / Grid<R>)).Interval.Upper, rmode)}
      : native_div_qformat
            ? grid{interval{rational{0}, (Upper<L> / Notch<R>).value()}, Notch<L>}
            : *(Grid<L> / Grid<R>);

    static constexpr bool any_real =
        (BoundPolicy<L> & bnd::real) == bnd::real || (BoundPolicy<R> & bnd::real) == bnd::real;
    // Keep `real` for a continuous result (Notch 0: double stores the quotient
    // verbatim) or a double-exact dyadic result; otherwise drop it (the double
    // quotient would not land on the result grid). See grid::double_exact.
    static constexpr bool keep_real =
        any_real && (result_grid.Notch == 0 || double_exact<result_grid>);
    // Carry both operands' representation flags (widest-wins at storage selection).
    static constexpr policy_flag rep =
        ((BoundPolicy<L> | BoundPolicy<R>) & (bnd::exact | bnd::direct | bnd::indexed))
        | (keep_real ? bnd::real : none);
    using result = bound<result_grid, rep != none ? rep : checked>;

    template <policy_flag G = F>
    static constexpr bool needs_overflow_check =
        ((G | F | BoundPolicy<L> | BoundPolicy<R>) & checked);

    // For a nonzero divisor the op fails only on the checked rational path
    // (overflow). So when the divisor excludes zero AND this is false, `div`
    // returns a plain `result` rather than optional<result>.
    static constexpr bool may_overflow_nonzero =
        !native_div && !real_raw<result> && (needs_overflow_check<F> != 0);

    // Real division can still fail on a zero divisor, so it uses the same
    // return-type rule as the rest: plain `result` when the op cannot fail
    // (overflow-action, or the divisor grid excludes zero with no rational
    // overflow), else optional<result>. Real has no rational overflow, so
    // may_overflow_nonzero is false for it (above).
    template <typename A>
    using div_return_t = std::conditional_t<
        overflow_action<plain<A>> || (DivisorExcludesZero<R> && !may_overflow_nonzero),
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
    // `fail` must stay well-formed even when div_return_t narrowed to plain
    // `result` (divisor excludes zero, no overflow); there every call to it is
    // removed by the guards below, so the final arm is dead (return-type only).
    // Shared by the real and non-real paths (real fails only on a zero divisor).
    [[maybe_unused]] auto fail = [&](errc code, const char* what) -> div_return_t<A> {
      if constexpr (overflow_action<plain<A>>)
        return report_or_nullopt<result>(action, policy, code, what);   // -> result
      else if constexpr (!DivisorExcludesZero<R> || may_overflow_nonzero)
        return report_or_nullopt<result>(action, policy, code, what);   // -> optional<result>
      else
        return result{};   // unreachable: divisor excludes zero, op cannot fail
    };

    // Div-by-zero check elided when R's grid excludes zero, or `ignore_zero` is
    // set (zero divisor is then UB, matching the `/= 0` no-op). The fail arms stay
    // keyed on DivisorExcludesZero (which narrows the return type; ignore_zero doesn't).
    [[maybe_unused]] constexpr bool zero_unchecked = DivisorExcludesZero<R>
        || (((G | F | BoundPolicy<L> | BoundPolicy<R>) & ignore_zero) != 0);

    if constexpr (real_raw<result>)
    {
      // Real division reports zero like every other path (throw / report /
      // action / nullopt). Finite operands keep the quotient finite, so no
      // non-finite ever reaches storage.
      if constexpr (!zero_unchecked)
        if (as_double(rhs) == 0.0) return fail(errc::division_by_zero, "division by zero in div");
      return result::from_raw(Grid<result>.snap_double(as_double(lhs) / as_double(rhs)));
    }
    else if constexpr (native_div_qformat)
    {
      // rhs.Raw == 0 iff rhs.value == 0 (Lower<R> == 0). Formula folds to
      // `(a << log2 N)/b` for power-of-two N — the native Q-format idiom.
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
      if (!q) [[unlikely]] return fail(errc::overflow, "rational overflow in div");
      return result::from_raw(*q);
    }
    else
    {
      rational rhs_r = rhs;
      if constexpr (!zero_unchecked)
        if (rhs_r.Numerator == 0) return fail(errc::division_by_zero, "division by zero in div");
      return result::from_raw(rational::div_unchecked(as_rational(lhs), rhs_r));
    }
  }
  //---------------------------------------------------------------------------
  // modulo (requires integer-valued grids + snap)
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_flag F = none>
  struct modulo
  {
    static constexpr bool native_mod = integer_native_ops<L, R, F>;

    // Hard requirement, not a fallback: `a mod b` is only defined for integer
    // operands, so the grid must be integer-aligned with `snap` set.
    static_assert(native_mod, "modulo requires integer-valued grids and snap");

    static constexpr imax max_rem =
        std::max(abs_den(LowerImax<R>), abs_den(UpperImax<R>)) - 1;

    // Remainder consistent with the rounded quotient: r = a − round(a/b)·b. Under
    // truncation it takes the dividend's sign (non-negative for a non-negative
    // dividend grid); any directional mode can flip the sign, so the grid widens
    // to the symmetric ±max_rem (|r| ≤ max_rem for every mode).
    static constexpr round_mode rmode =
        div_round_mode(F | BoundPolicy<L> | BoundPolicy<R>);

    static constexpr grid result_grid =
        (rmode == round_mode::trunc && LowerImax<L> >= 0)
        ? grid{imax{0}, max_rem}
        : grid{-max_rem, max_rem};

    using result = bound<result_grid>;

    // Modulo never overflows (the remainder fits result_grid), so the only
    // failure is a zero divisor — excluded by the grid → plain `result`.
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
    // Zero check elided when R's grid excludes zero (mod_return_t is plain
    // `result`) or `ignore_zero` is set (zero divisor is then UB, matching `%= 0`).
    constexpr bool zero_unchecked = DivisorExcludesZero<R>
        || (((G | F | BoundPolicy<L> | BoundPolicy<R>) & ignore_zero) != 0);
    if constexpr (!zero_unchecked)
      if (rhs_val == 0)
        return report_or_nullopt<result>(action, policy, errc::division_by_zero,
                                         "division by zero in mod");
    result res;
    // Remainder consistent with the rounded quotient (trunc → C++ `%`).
    const imax lhs_val = to_value(lhs);
    from_value(res, lhs_val - div_rounded(lhs_val, rhs_val, rmode) * rhs_val);
    return res;
  }
} // namespace bnd::detail

#endif // BNDdivisionHPP
