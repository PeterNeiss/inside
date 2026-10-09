// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_DIVISION_HPP
#define BEMAN_INSIDE_DETAIL_DIVISION_HPP

#include <beman/inside/detail/rep.hpp>
#include <beman/inside/generic.hpp>
#include <beman/inside/detail/wide_value.hpp>
#include <beman/inside/grid.hpp>
#include <beman/inside/policy.hpp>

#include <limits>
#include <numeric>

//---------------------------------------------------------------------------
// division / modulo. `division::div` returns expected<result, errc> (division by zero
// is always runtime-possible). Two paths: native (integer-aligned grids +
// snap → native integer division) and rational (exact, can overflow under
// checked). `modulo::mod` is integer-only — non-integer remainders aren't
// well-defined on fractional notches.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
// Both operands are plain integer grids and the caller accepted integer
// truncation (snap) — the prerequisite for native integer div / mod.
template <insidable L, insidable R, policy_flag F>
inline constexpr bool integer_ops = ((F | policy_of<L> | policy_of<R>)&snap) && !rational_raw<L> && !rational_raw<R> &&
                                    is_integer_aligned<L> && is_integer_aligned<R>;

// ...and every value fits imax, so the builtin integer division applies.
template <insidable L, insidable R, policy_flag F>
inline constexpr bool integer_native_ops = integer_ops<L, R, F> && values_fit_imax<L> && values_fit_imax<R>;

//---------------------------------------------------------------------------
// Rounding mode for the native div & mod paths (fire when `snap` is set).
// Decided from the combined flags by rounding_of (policy_flag.hpp), the one
// precedence all rounding paths share; `snap` alone is truncate-toward-zero.
// The runtime quotient and the compile-time grid endpoints MUST agree on the
// mode (both read rounding_of), or a result could escape its own grid.
//---------------------------------------------------------------------------

// Round the signed exact quotient a/b (b != 0) to an integer per `m`.
template <std::signed_integral T>
constexpr T div_rounded(T a, T b, round_mode m) noexcept {
    using U   = std::make_unsigned_t<T>;
    const T t = a / b; // C++ truncation toward zero
    const T r = a % b; // sign of a, |r| < |b|
    if (r == 0 || m == round_mode::trunc)
        return t;
    const bool neg = (a < 0) != (b < 0); // exact quotient is negative
    // |r|, |b| in U (safe for T::min); ab - ar is safe: 0 < ar < ab
    const U ar   = r < 0 ? U(~U(r) + 1u) : U(r);
    const U ab   = b < 0 ? U(~U(b) + 1u) : U(b);
    const T away = neg ? T(t - 1) : T(t + 1);
    switch (m) {
    case round_mode::floor:
        return neg ? away : t;
    case round_mode::ceil:
        return neg ? t : away;
    case round_mode::nearest:
        return (ar >= ab - ar) ? away : t; // half away from zero
    case round_mode::half_even:
        if (ar != ab - ar)
            return (ar < ab - ar) ? t : away;
        return (t & 1) == 0 ? t : away; // tie → even
    default:
        return t;
    }
}

// The narrowest signed type in which native div/mod of L by R is exact: int32
// when both value ranges fit (excluding INT32_MIN, so a / -1 cannot overflow),
// else imax. A 32-bit divide is markedly cheaper than a 64-bit one.
template <insidable L, insidable R>
using native_div_t = std::conditional_t<(lower_imax<L> > std::numeric_limits<std::int32_t>::min() &&
                                         upper_imax<L> <= std::numeric_limits<std::int32_t>::max() &&
                                         lower_imax<R> > std::numeric_limits<std::int32_t>::min() &&
                                         upper_imax<R> <= std::numeric_limits<std::int32_t>::max()),
                                        std::int32_t,
                                        imax>;

// Round a non-negative quotient num/den (den != 0) per `m`. Used by the
// Q-format path, whose raws are non-negative (Lower == 0).
// U is a builtin unsigned integer or an unsigned wide_int.
template <raw_integer U>
constexpr U round_uquotient(U num, U den, round_mode m) noexcept {
    const U t = num / den, r = num % den;
    if (r == 0 || m == round_mode::trunc)
        return t;
    switch (m) {
    case round_mode::floor:
        return t; // non-negative: floor == trunc
    case round_mode::ceil:
        return t + 1;
    case round_mode::nearest:
        return (r >= den - r) ? t + 1 : t;
    case round_mode::half_even:
        if (r < den - r)
            return t;
        if (r > den - r)
            return t + 1;
        return (t & 1) == 0 ? t : t + 1;
    default:
        return t;
    }
}

// The zero-divisor check is skipped when R's grid excludes zero or
// `ignore_zero` is set (a zero divisor is then UB, matching the `/= 0` no-op).
template <insidable L, insidable R, policy_flag F, policy_flag G>
inline constexpr bool divisor_unchecked =
    divisor_excludes_zero<R> || ((G | F | policy_of<L> | policy_of<R>)&ignore_zero) != 0;

// Compile-time rounding of a quotient-interval endpoint to an integer index.
// Under half_even the endpoint is bracketed by [floor, ceil] (Upper: ceil)
// rather than reproducing the parity rule at compile time.
constexpr imax round_rat(rational q, round_mode m, bool upper) noexcept {
    switch (m) {
    case round_mode::nearest:
        return round(q);
    case round_mode::floor:
        return floor(q);
    case round_mode::ceil:
        return ceil(q);
    case round_mode::half_even:
        return upper ? ceil(q) : floor(q);
    default:
        return trunc(q);
    }
}

// Every value of a 64-bit grid with a nonzero notch (or a point grid) is
// n/d with d | Den and |n| ≤ Num: Den = lcm(den(lower), den(notch)) (the
// upper limit's denominator divides it too), Num = max(|lower|, |upper|)·Den.
// Ok is false when a bound passes 64 bits.
struct value_bounds {
    umax Num, Den;
    bool Ok;
};

template <insidable B>
constexpr value_bounds value_bounds_of() noexcept {
    const rational lo = lower64<B>, hi = upper64<B>, n = notch64<B>;
    const umax     dl = abs_den(lo.Denominator), dn = n.Numerator == 0 ? umax{1} : abs_den(n.Denominator);
    umax           den, nlo, nhi;
    if (mul_overflow(dl / std::gcd(dl, dn), dn, &den))
        return {0, 0, false};
    if (mul_overflow(lo.Numerator, den / dl, &nlo))
        return {0, 0, false};
    if (mul_overflow(hi.Numerator, den / abs_den(hi.Denominator), &nhi))
        return {0, 0, false};
    return {nlo > nhi ? nlo : nhi, den, true};
}

// The checked rational quotient (a/b)/(c/d) = (a·d)/(b·c) cannot overflow
// for any values of L and R: c, a·d and b·c stay within the rational's
// fields (rational::div_impl). Bounded only for 64-bit grids with nonzero
// notches and point grids; a continuous or exact-valued operand gives false.
template <insidable L, insidable R>
constexpr bool quotient_fits_rational() noexcept {
    if constexpr (exact_valued<L> || exact_valued<R>)
        return false;
    else {
        constexpr auto bounded = []<insidable B>() { return notch64<B>.Numerator != 0 || lower64<B> == upper64<B>; };
        if (!bounded.template operator()<L>() || !bounded.template operator()<R>())
            return false;
        constexpr value_bounds l = value_bounds_of<L>(), r = value_bounds_of<R>();
        constexpr umax         imax_max = static_cast<umax>(std::numeric_limits<imax>::max());
        umax                   ad, bc;
        return l.Ok && r.Ok && r.Num <= imax_max && !mul_overflow(l.Num, r.Den, &ad) &&
               !mul_overflow(l.Den, r.Num, &bc) && bc <= imax_max;
    }
}

template <insidable L, insidable R = L, policy_flag F = none>
struct division {
    // Native integer division, two flavours gated on `snap`:
    //   native_div_integer — both operands integer-aligned; formula `a / b`.
    //   native_div_qformat — both same Q-format (Notch = 1/N, Lower = 0); formula
    //                        `(a·N)/b` (the native `(a << log2 N)/b` idiom).
    // Otherwise the exact-rational path returns inside<rational>.
    static constexpr bool native_div_integer = integer_native_ops<L, R, F>;

    // (if constexpr: naming a 64-bit view instantiates it, even where && would
    // skip it — so an exact-valued operand returns before any is named.)
    static constexpr bool native_div_qformat = [] {
        if constexpr (exact_valued<L> || exact_valued<R>)
            return false;
        else
            return ((F | policy_of<L> | policy_of<R>)&snap) && is_qformat<L> && is_qformat<R> &&
                   notch_of<L> == notch_of<R>;
    }();

    static constexpr bool native_div = native_div_integer || native_div_qformat;

    // The rounding mode for the native paths (shared by the grid and runtime).
    static constexpr round_mode rmode = rounding_of(F | policy_of<L> | policy_of<R>);

    // A clear diagnostic when the result grid is unrepresentable, instead of the
    // raw expected-deref / .value() below failing cryptically (mirrors add/mul).
    static_assert(native_div_qformat || (grid_of<L> / grid_of<R>).has_value(),
                  "division: result grid not representable (notch/interval exceeds the "
                  "representable rational range) — coarsen the operand grids");
    static_assert(
        [] {
            if constexpr (native_div_qformat)
                return (detail::upper64<L> / detail::notch64<R>).has_value();
            else
                return true;
        }(),
        "division: Q-format result grid not representable — coarsen the operand grids");

    // Native-integer endpoints rounded with the same mode as the runtime
    // quotient, so e.g. round_ceil can't escape the grid. (The Q-format extreme
    // is always exact, so its grid is unchanged.)
    static constexpr grid result_grid = [] {
        if constexpr (native_div_integer)
            return grid{round_rat(to_rational((*(grid_of<L> / grid_of<R>)).Interval.Lower), rmode, false),
                        round_rat(to_rational((*(grid_of<L> / grid_of<R>)).Interval.Upper), rmode, true)};
        else if constexpr (native_div_qformat)
            return grid{interval{rational{0}, (detail::upper64<L> / detail::notch64<R>).value()}, detail::notch64<L>};
        else
            return *(grid_of<L> / grid_of<R>);
    }();

    // fp / representation propagation — shared rule in detail/rep.hpp.
    // AllowContinuous: a continuous quotient (Notch 0) keeps fp verbatim.
    using rep_t  = fp_rep<L, R, result_grid, /*AllowContinuous=*/true>;
    using result = inside<result_grid, rep_t::result_policy>;

    template <policy_flag G = F>
    static constexpr bool needs_overflow_check =
        has_any_flag(G | F, checked) || is_checked(policy_of<L>) || is_checked(policy_of<R>) ||
        has_any_flag(G | F | policy_of<L> | policy_of<R>, exact);

    // For a nonzero divisor the op fails only on the checked rational path
    // (overflow). So when the divisor excludes zero AND this is false, `div`
    // returns a plain `result` rather than expected<result, errc>. The
    // operand grids may prove the quotient fits (quotient_fits_rational).
    // A wide-index operand's quotient may outgrow the 64-bit rational
    // whatever the policy, so that path always reports.
    static constexpr bool fits_rational        = quotient_fits_rational<L, R>();
    static constexpr bool may_overflow_nonzero = !native_div && !fp_raw<result> && !fits_rational &&
                                                 (needs_overflow_check<F> != 0 || exact_valued<L> || exact_valued<R>);

    // Real division can still fail on a zero divisor, so it uses the same
    // return-type rule as the rest: plain `result` when the op cannot fail
    // (overflow-action, or the divisor grid excludes zero with no rational
    // overflow), else expected<result, errc>. Real has no rational overflow, so
    // may_overflow_nonzero is false for it (above).
    template <typename A>
    using return_t =
        std::conditional_t<overflow_action<plain_t<A>> || (divisor_excludes_zero<R> && !may_overflow_nonzero),
                           result,
                           std::expected<result, errc>>;

    template <policy_flag G = F, typename E = empty_ref, typename A = no_action>
    static constexpr return_t<A> div(L, R, policy<G, E> = {}, A&& = {});
};

//---------------------------------------------------------------------------
// div
//---------------------------------------------------------------------------
template <insidable L, insidable R, policy_flag F>
template <policy_flag G, typename E, typename A>
constexpr auto division<L, R, F>::div(L lhs, R rhs, policy<G, E> policy, A&& action) -> return_t<A> {
    // `fail` must stay well-formed even when return_t narrowed to plain
    // `result` (divisor excludes zero, no overflow); there every call to it is
    // removed by the guards below, so the final arm is dead (return-type only).
    // Shared by the f64 and non-f64 paths (f64 fails only on a zero divisor).
    [[maybe_unused]] auto fail = [&](errc code, const char* what) -> return_t<A> {
        if constexpr (overflow_action<plain_t<A>>)
            return report_or_unexpected<result>(action, policy, code, what); // -> result
        else if constexpr (!divisor_excludes_zero<R> || may_overflow_nonzero)
            return report_or_unexpected<result>(action, policy, code, what); // -> expected<result, errc>
        else
            return result{}; // unreachable: divisor excludes zero, op cannot fail
    };

    // The fail arms stay keyed on divisor_excludes_zero (which narrows the
    // return type; ignore_zero doesn't).
    [[maybe_unused]] constexpr bool zero_unchecked = divisor_unchecked<L, R, F, G>;

    if constexpr (fp_raw<result>) {
        // Real division reports zero like every other path (throw / report /
        // action / unexpected). Finite operands keep the quotient finite, so no
        // non-finite ever reaches storage.
        if constexpr (!zero_unchecked)
            if (as_double(rhs) == 0.0)
                return fail(errc::division_by_zero, "division by zero in div");
        // The quotient rounds once in double; its exact residual a − q·b gives
        // the side of the true quotient where that rounding sits on a boundary.
        const double a = as_double(lhs), b = as_double(rhs), q = a / b;
        return result::from_raw(raw_cast<result>(snap_double_from<grid_of<result>, rmode>(q, [&] {
            const double r = __builtin_fma(-q, b, a);
            return ((r > 0) - (r < 0)) * (b > 0 ? 1 : -1);
        })));
    } else if constexpr (native_div_qformat) {
        // rhs.Raw == 0 iff rhs.value == 0 (detail::lower64<R> == 0). Formula folds to
        // `(a << log2 N)/b` for power-of-two N — the native Q-format idiom.
        if constexpr (!zero_unchecked)
            if (rhs.raw() == 0)
                return fail(errc::division_by_zero, "division by zero in div");
        constexpr umax N = abs_den(detail::notch64<L>.Denominator);
        // The scaled dividend raw·N in the narrowest type that holds it: a 32-bit
        // divide where it fits (Q8.8, Q16.15, ...), else 64 bits, else a wide_int.
        constexpr int dividend_bits = std::bit_width(max_index_v<L>) + std::bit_width(N);
        using U = std::conditional_t<(max_index_v<L> <= std::numeric_limits<std::uint32_t>::max() / N),
                                     std::uint32_t,
                                     int_for_bits_t<(dividend_bits < 64 ? 64 : dividend_bits), false>>;
        return result::from_raw(raw_cast<result>(
            round_uquotient<U>(static_cast<U>(static_cast<U>(lhs.raw()) * U{N}), static_cast<U>(rhs.raw()), rmode)));
    } else if constexpr (native_div_integer) {
        using T         = native_div_t<L, R>;
        const T rhs_val = static_cast<T>(to_value(rhs));
        if constexpr (!zero_unchecked)
            if (rhs_val == 0)
                return fail(errc::division_by_zero, "division by zero in div");
        result res;
        from_value(res, imax{div_rounded(static_cast<T>(to_value(lhs)), rhs_val, rmode)});
        return res;
    } else if constexpr (exact_valued<L> || exact_valued<R>) {
        // A wide-index or big-grid operand: the exact quotient, in the result's raw.
        const auto d = exact_of(rhs);
        if constexpr (!zero_unchecked)
            if (d.Num.is_zero())
                return fail(errc::division_by_zero, "division by zero in div");
        if constexpr (frac_raw<result>) {
            // Grids past 64 bits: the exact quotient in the result's wide fraction.
            const auto q = frac_raw_of<raw_t<result>>(exact_of(lhs) / d);
            if (!q) [[unlikely]]
                return fail(errc::overflow, "quotient past its fraction raw in div");
            return result::from_raw(*q);
        } else {
            const auto q = try_rational(exact_of(lhs) / d);
            if (!q) [[unlikely]]
                return fail(errc::overflow, "rational overflow in div");
            return result::from_raw(*q);
        }
    } else if constexpr (needs_overflow_check<G> && !fits_rational) {
        rational rhs_r = rhs;
        if constexpr (!zero_unchecked)
            if (rhs_r.Numerator == 0)
                return fail(errc::division_by_zero, "division by zero in div");
        auto q = as_rational(lhs) / rhs_r;
        if (!q) [[unlikely]]
            return fail(errc::overflow, "rational overflow in div");
        return result::from_raw(*q);
    } else {
        rational rhs_r = rhs;
        if constexpr (!zero_unchecked)
            if (rhs_r.Numerator == 0)
                return fail(errc::division_by_zero, "division by zero in div");
        return result::from_raw(rational::div_unchecked(as_rational(lhs), rhs_r));
    }
}
//---------------------------------------------------------------------------
// modulo (requires integer-valued grids + snap)
//---------------------------------------------------------------------------
template <insidable L, insidable R, policy_flag F = none>
struct modulo {
    // Hard requirement, not a fallback: `a mod b` is only defined for integer
    // operands, so the grid must be integer-aligned with `snap` set.
    static_assert(integer_ops<L, R, F>, "modulo requires integer-valued grids and snap");

    // Builtin division when every value fits imax; else exact wide integers.
    static constexpr bool native_mod = integer_native_ops<L, R, F>;

    static constexpr grid_rational max_rem =
        lift_unwrap((abs(lower_of<R>) > abs(upper_of<R>) ? abs(lower_of<R>) : abs(upper_of<R>)) - grid_rational{1});

    // Remainder consistent with the rounded quotient: r = a − round(a/b)·b. Under
    // truncation it takes the dividend's sign (non-negative for a non-negative
    // dividend grid); any directional mode can flip the sign, so the grid widens
    // to the symmetric ±max_rem (|r| ≤ max_rem for every mode).
    static constexpr round_mode rmode = rounding_of(F | policy_of<L> | policy_of<R>);

    static constexpr grid result_grid =
        (rmode == round_mode::trunc && lower_of<L> >= 0) ? grid{grid_rational{0}, max_rem} : grid{-max_rem, max_rem};

    using result = inside<result_grid>;

    // Modulo never overflows (the remainder fits result_grid), so the only
    // failure is a zero divisor — excluded by the grid → plain `result`.
    template <typename A>
    using return_t = std::
        conditional_t<overflow_action<plain_t<A>> || divisor_excludes_zero<R>, result, std::expected<result, errc>>;

    template <policy_flag G = F, typename E = empty_ref, typename A = no_action>
    static constexpr return_t<A> mod(L, R, policy<G, E> = {}, A&& = {});
};

template <insidable L, insidable R, policy_flag F>
template <policy_flag G, typename E, typename A>
constexpr auto modulo<L, R, F>::mod(L lhs, R rhs, policy<G, E> policy, A&& action) -> return_t<A> {
    if constexpr (!native_mod) {
        // Integer values past imax: r = a − round(a/b)·b in exact wide integers.
        constexpr bool zero_unchecked = divisor_unchecked<L, R, F, G>;
        using I                       = wide_sint<exact_limbs<L, R, result>>;
        const I b{trunc(exact_of(rhs))};
        if constexpr (!zero_unchecked)
            if (b.is_zero())
                return report_or_unexpected<result>(action, policy, errc::division_by_zero, "division by zero in mod");
        const I a{trunc(exact_of(lhs))};
        return exact_result<result>(exact_frac<exact_limbs<L, R, result>>{a - rounded_div<rmode>(a, b) * b, I{1}});
    } else {
        using T                       = native_div_t<L, R>;
        const T        rhs_val        = static_cast<T>(to_value(rhs));
        constexpr bool zero_unchecked = divisor_unchecked<L, R, F, G>;
        if constexpr (!zero_unchecked)
            if (rhs_val == 0)
                return report_or_unexpected<result>(action, policy, errc::division_by_zero, "division by zero in mod");
        result res;
        // Remainder consistent with the rounded quotient (trunc → C++ `%`).
        const T lhs_val = static_cast<T>(to_value(lhs));
        // The product stays in imax: q·b can exceed |a| + |b| ≥ 2^31 in T.
        from_value(res, lhs_val - imax{div_rounded(lhs_val, rhs_val, rmode)} * rhs_val);
        return res;
    }
}
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_DETAIL_DIVISION_HPP
