// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_DETAIL_DIVISION_HPP
#define BEMAN_INSIDE_DETAIL_DIVISION_HPP

#include <beman/inside/detail/rep.hpp>
#include <beman/inside/generic.hpp>
#include <beman/inside/grid.hpp>
#include <beman/inside/policy.hpp>

//---------------------------------------------------------------------------
// division / modulo. `division::div` returns expected<result, errc> (division by zero
// is always runtime-possible). Two paths: native (integer-aligned grids +
// snap → native integer division) and rational (exact, can overflow under
// checked). `modulo::mod` is integer-only — non-integer remainders aren't
// well-defined on fractional notches.
//---------------------------------------------------------------------------
namespace beman::inside::detail
{
  // Both operands are plain integer grids and the caller accepted integer
  // truncation (snap) — the prerequisite for native integer div / mod.
  template <insidable L, insidable R, policy_flag F>
  inline constexpr bool integer_native_ops =
      ((F | policy_of<L> | policy_of<R>) & snap)
      && !rational_raw<L> && !rational_raw<R>
      && is_integer_aligned<L> && is_integer_aligned<R>;

  //---------------------------------------------------------------------------
  // Rounding mode for the native div & mod paths (fire when `snap` is set).
  // Decided from the combined flags by rounding_of (policy_flag.hpp), the one
  // precedence all rounding paths share; `snap` alone is truncate-toward-zero.
  // The runtime quotient and the compile-time grid endpoints MUST agree on the
  // mode (both read div_round_mode), or a result could escape its own grid.
  //---------------------------------------------------------------------------
  constexpr round_mode div_round_mode(policy_flag eff) noexcept { return rounding_of(eff); }

  // Round the signed exact quotient a/b (b != 0) to an integer per `m`.
  template <std::signed_integral T>
  constexpr T div_rounded(T a, T b, round_mode m) noexcept
  {
    using U = std::make_unsigned_t<T>;
    const T t = a / b;                        // C++ truncation toward zero
    const T r = a % b;                        // sign of a, |r| < |b|
    if (r == 0 || m == round_mode::trunc) return t;
    const bool neg = (a < 0) != (b < 0);      // exact quotient is negative
    // |r|, |b| in U (safe for T::min); ab - ar is safe: 0 < ar < ab
    const U ar = r < 0 ? U(~U(r) + 1u) : U(r);
    const U ab = b < 0 ? U(~U(b) + 1u) : U(b);
    const T away = neg ? T(t - 1) : T(t + 1);
    switch (m)
    {
      case round_mode::floor:   return neg ? away : t;
      case round_mode::ceil:    return neg ? t : away;
      case round_mode::nearest: return (ar >= ab - ar) ? away : t;   // half away from zero
      case round_mode::half_even:
        if (ar != ab - ar) return (ar < ab - ar) ? t : away;
        return (t & 1) == 0 ? t : away;                           // tie → even
      default:                  return t;
    }
  }

  // The narrowest signed type in which native div/mod of L by R is exact: int32
  // when both value ranges fit (excluding INT32_MIN, so a / -1 cannot overflow),
  // else imax. A 32-bit divide is markedly cheaper than a 64-bit one.
  template <insidable L, insidable R>
  using native_div_t = std::conditional_t<
      (lower_imax<L> > std::numeric_limits<std::int32_t>::min()
       && upper_imax<L> <= std::numeric_limits<std::int32_t>::max()
       && lower_imax<R> > std::numeric_limits<std::int32_t>::min()
       && upper_imax<R> <= std::numeric_limits<std::int32_t>::max()),
      std::int32_t, imax>;

  // Round a non-negative quotient num/den (den != 0) per `m`. Used by the
  // Q-format path, whose raws are non-negative (Lower == 0).
  template <std::unsigned_integral U>
  constexpr U round_uquotient(U num, U den, round_mode m) noexcept
  {
    const U t = num / den, r = num % den;
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

  template <insidable L, insidable R = L, policy_flag F = none>
  struct division
  {
    // Native integer division, two flavours gated on `snap`:
    //   native_div_integer — both operands integer-aligned; formula `a / b`.
    //   native_div_qformat — both same Q-format (Notch = 1/N, Lower = 0); formula
    //                        `(a·N)/b` (the native `(a << log2 N)/b` idiom).
    // Otherwise the exact-rational path returns inside<rational>.
    static constexpr bool native_div_integer = integer_native_ops<L, R, F>;

    static constexpr bool native_div_qformat =
        ((F | policy_of<L> | policy_of<R>) & snap)
        && is_qformat<L> && is_qformat<R>
        && notch_of<L> == notch_of<R>;

    static constexpr bool native_div = native_div_integer || native_div_qformat;

    // The rounding mode for the native paths (shared by the grid and runtime).
    static constexpr round_mode rmode =
        div_round_mode(F | policy_of<L> | policy_of<R>);

    // A clear diagnostic when the result grid is unrepresentable, instead of the
    // raw expected-deref / .value() below failing cryptically (mirrors add/mul).
    static_assert(native_div_qformat || (grid_of<L> / grid_of<R>).has_value(),
      "division: result grid not representable (notch/interval exceeds the "
      "representable rational range) — coarsen the operand grids");
    static_assert(!native_div_qformat || (upper_of<L> / notch_of<R>).has_value(),
      "division: Q-format result grid not representable — coarsen the operand grids");

    // Native-integer endpoints rounded with the same mode as the runtime
    // quotient, so e.g. round_ceil can't escape the grid. (The Q-format extreme
    // is always exact, so its grid is unchanged.)
    static constexpr grid result_grid =
        native_div_integer
            ? grid{round_rat_lo((*(grid_of<L> / grid_of<R>)).Interval.Lower, rmode),
                   round_rat_hi((*(grid_of<L> / grid_of<R>)).Interval.Upper, rmode)}
      : native_div_qformat
            ? grid{interval{rational{0}, (upper_of<L> / notch_of<R>).value()}, notch_of<L>}
            : *(grid_of<L> / grid_of<R>);

    // fp / representation propagation — shared rule in detail/rep.hpp.
    // AllowContinuous: a continuous quotient (Notch 0) keeps fp verbatim.
    using rep_t = fp_rep<L, R, result_grid, /*AllowContinuous=*/true>;
    using result = inside<result_grid, rep_t::result_policy>;

    template <policy_flag G = F>
    static constexpr bool needs_overflow_check =
        has_any_flag(G | F | policy_of<L> | policy_of<R>, checked | exact);

    // For a nonzero divisor the op fails only on the checked rational path
    // (overflow). So when the divisor excludes zero AND this is false, `div`
    // returns a plain `result` rather than expected<result, errc>.
    static constexpr bool may_overflow_nonzero =
        !native_div && !fp_raw<result> && (needs_overflow_check<F> != 0);

    // Real division can still fail on a zero divisor, so it uses the same
    // return-type rule as the rest: plain `result` when the op cannot fail
    // (overflow-action, or the divisor grid excludes zero with no rational
    // overflow), else expected<result, errc>. Real has no rational overflow, so
    // may_overflow_nonzero is false for it (above).
    template <typename A>
    using div_return_t = std::conditional_t<
        overflow_action<plain_t<A>> || (divisor_excludes_zero<R> && !may_overflow_nonzero),
        result,
        std::expected<result, errc>>;

    template <policy_flag G = F, typename E = empty_ref, typename A = no_action>
    static constexpr div_return_t<A> div(L, R, policy<G, E> = {}, A&& = {});
  };

  //---------------------------------------------------------------------------
  // div
  //---------------------------------------------------------------------------
  template<insidable L, insidable R, policy_flag F>
  template<policy_flag G, typename E, typename A>
  constexpr auto division<L,R,F>::div(L lhs, R rhs, policy<G, E> policy, A&& action) -> div_return_t<A>
  {
    // `fail` must stay well-formed even when div_return_t narrowed to plain
    // `result` (divisor excludes zero, no overflow); there every call to it is
    // removed by the guards below, so the final arm is dead (return-type only).
    // Shared by the f64 and non-f64 paths (f64 fails only on a zero divisor).
    [[maybe_unused]] auto fail = [&](errc code, const char* what) -> div_return_t<A> {
      if constexpr (overflow_action<plain_t<A>>)
        return report_or_unexpected<result>(action, policy, code, what);   // -> result
      else if constexpr (!divisor_excludes_zero<R> || may_overflow_nonzero)
        return report_or_unexpected<result>(action, policy, code, what);   // -> expected<result, errc>
      else
        return result{};   // unreachable: divisor excludes zero, op cannot fail
    };

    // Div-by-zero check elided when R's grid excludes zero, or `ignore_zero` is
    // set (zero divisor is then UB, matching the `/= 0` no-op). The fail arms stay
    // keyed on divisor_excludes_zero (which narrows the return type; ignore_zero doesn't).
    [[maybe_unused]] constexpr bool zero_unchecked = divisor_excludes_zero<R>
        || (((G | F | policy_of<L> | policy_of<R>) & ignore_zero) != 0);

    if constexpr (fp_raw<result>)
    {
      // Real division reports zero like every other path (throw / report /
      // action / unexpected). Finite operands keep the quotient finite, so no
      // non-finite ever reaches storage.
      if constexpr (!zero_unchecked)
        if (as_double(rhs) == 0.0) return fail(errc::division_by_zero, "division by zero in div");
      return result::from_raw(raw_cast<result>(snap_double<grid_of<result>, rmode>(as_double(lhs) / as_double(rhs))));
    }
    else if constexpr (native_div_qformat)
    {
      // rhs.Raw == 0 iff rhs.value == 0 (lower_of<R> == 0). Formula folds to
      // `(a << log2 N)/b` for power-of-two N — the native Q-format idiom.
      if constexpr (!zero_unchecked)
        if (rhs.raw() == 0) return fail(errc::division_by_zero, "division by zero in div");
      constexpr umax N = abs_den(notch_of<L>.Denominator);
      // 32-bit divide when the scaled dividend fits (Q8.8, Q16.15, ...).
      using U = std::conditional_t<(max_index_v<L> <= std::numeric_limits<std::uint32_t>::max() / N),
                                   std::uint32_t, umax>;
      return result::from_raw(raw_cast<result>(round_uquotient<U>(
          static_cast<U>(static_cast<U>(lhs.raw()) * U{N}), static_cast<U>(rhs.raw()), rmode)));
    }
    else if constexpr (native_div_integer)
    {
      using T = native_div_t<L, R>;
      const T rhs_val = static_cast<T>(to_value(rhs));
      if constexpr (!zero_unchecked)
        if (rhs_val == 0) return fail(errc::division_by_zero, "division by zero in div");
      result res;
      from_value(res, imax{div_rounded(static_cast<T>(to_value(lhs)), rhs_val, rmode)});
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
  template <insidable L, insidable R, policy_flag F = none>
  struct modulo
  {
    static constexpr bool native_mod = integer_native_ops<L, R, F>;

    // Hard requirement, not a fallback: `a mod b` is only defined for integer
    // operands, so the grid must be integer-aligned with `snap` set.
    static_assert(native_mod, "modulo requires integer-valued grids and snap");

    static constexpr imax max_rem =
        (abs_den(lower_imax<R>) > abs_den(upper_imax<R>) ? abs_den(lower_imax<R>) : abs_den(upper_imax<R>)) - 1;

    // Remainder consistent with the rounded quotient: r = a − round(a/b)·b. Under
    // truncation it takes the dividend's sign (non-negative for a non-negative
    // dividend grid); any directional mode can flip the sign, so the grid widens
    // to the symmetric ±max_rem (|r| ≤ max_rem for every mode).
    static constexpr round_mode rmode =
        div_round_mode(F | policy_of<L> | policy_of<R>);

    static constexpr grid result_grid =
        (rmode == round_mode::trunc && lower_imax<L> >= 0)
        ? grid{imax{0}, max_rem}
        : grid{-max_rem, max_rem};

    using result = inside<result_grid>;

    // Modulo never overflows (the remainder fits result_grid), so the only
    // failure is a zero divisor — excluded by the grid → plain `result`.
    template <typename A>
    using mod_return_t = std::conditional_t<
        overflow_action<plain_t<A>> || divisor_excludes_zero<R>,
        result,
        std::expected<result, errc>>;

    template <policy_flag G = F, typename E = empty_ref, typename A = no_action>
    static constexpr mod_return_t<A> mod(L, R, policy<G, E> = {}, A&& = {});
  };

  template<insidable L, insidable R, policy_flag F>
  template<policy_flag G, typename E, typename A>
  constexpr auto modulo<L,R,F>::mod(L lhs, R rhs, policy<G, E> policy, A&& action) -> mod_return_t<A>
  {
    using T = native_div_t<L, R>;
    const T rhs_val = static_cast<T>(to_value(rhs));
    // Zero check elided when R's grid excludes zero (mod_return_t is plain
    // `result`) or `ignore_zero` is set (zero divisor is then UB, matching `%= 0`).
    constexpr bool zero_unchecked = divisor_excludes_zero<R>
        || (((G | F | policy_of<L> | policy_of<R>) & ignore_zero) != 0);
    if constexpr (!zero_unchecked)
      if (rhs_val == 0)
        return report_or_unexpected<result>(action, policy, errc::division_by_zero,
                                            "division by zero in mod");
    result res;
    // Remainder consistent with the rounded quotient (trunc → C++ `%`).
    const T lhs_val = static_cast<T>(to_value(lhs));
    // The product stays in imax: q·b can exceed |a| + |b| ≥ 2^31 in T.
    from_value(res, lhs_val - imax{div_rounded(lhs_val, rhs_val, rmode)} * rhs_val);
    return res;
  }
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_DETAIL_DIVISION_HPP
