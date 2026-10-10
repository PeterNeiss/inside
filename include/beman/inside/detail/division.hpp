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
inline constexpr bool integer_ops =
    ((F | policy_of<L> | policy_of<R>)&snap) && integer_lattice<L> && integer_lattice<R>;

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
    using U        = std::make_unsigned_t<T>;
    const T    t   = a / b;                        // C++ truncation toward zero
    const T    r   = a % b;                        // sign of a, |r| < |b|
    const bool neg = (a < 0) != (b < 0);           // exact quotient is negative
    const U    ar  = r < 0 ? U(~U(r) + 1u) : U(r); // |r|, |b| in U (safe for T::min)
    const U    ab  = b < 0 ? U(~U(b) + 1u) : U(b);
    if (!rounds_away(m, neg, classify_remainder(m, ar, ab), (t & 1) != 0))
        return t;
    return neg ? T(t - 1) : T(t + 1);
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

// The zero-divisor check is skipped when R's grid excludes zero or
// `ignore_zero` is set (a zero divisor is then UB, matching the `/= 0` no-op).
template <insidable L, insidable R, policy_flag F, policy_flag G>
inline constexpr bool divisor_unchecked =
    divisor_excludes_zero<R> || ((G | F | policy_of<L> | policy_of<R>)&ignore_zero) != 0;

// Compile-time rounding of a quotient-interval endpoint to an integer index.
// Under half_even the endpoint is bracketed by [floor, ceil] (Upper: ceil)
// rather than reproducing the parity rule at compile time.
constexpr imax round_rat(rational q, round_mode m, bool upper) noexcept {
    if (m == round_mode::half_even)
        m = upper ? round_mode::ceil : round_mode::floor;
    return round_to_int(q, m);
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
// fields (rational::div_impl). Bounded only for 64-bit integer raws; a
// continuous operand gives false.
template <insidable L, insidable R>
constexpr bool quotient_fits_rational() noexcept {
    if constexpr (wide_valued<L> || wide_valued<R>)
        return false;
    else {
        if (!integer_storage<L> || !integer_storage<R>) // a continuous operand bounds no denominator
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
    //   native_div_fixed   — both on one notch 1/K (K > 1), anchored, any sign:
    //                        the quotient's index on that notch is
    //                        round(ja·K / jb) of the value indices (the native
    //                        `(a << log2 K)/b` idiom for a power-of-two K).
    // Otherwise the exact-rational path returns a continuous quotient.
    static constexpr bool native_div_integer = integer_native_ops<L, R, F>;

    // (if constexpr: naming a 64-bit view instantiates it, even where && would
    // skip it — so an exact-valued operand returns before any is named.)
    static constexpr bool native_div_fixed = [] {
        if constexpr (wide_grid_numbers<L> || wide_grid_numbers<R> || !notched<L> || notch_of<L> != notch_of<R> ||
                      !anchored<L> || !anchored<R> || !(grid_of<L> / grid_of<R>).has_value())
            return false;
        else
            return ((F | policy_of<L> | policy_of<R>)&snap) && wide_numerator(notch_of<L>) == grid_wide{1} &&
                   grid_wide{1} < wide_denominator(notch_of<L>);
    }();

    static constexpr bool native_div = native_div_integer || native_div_fixed;

    // The rounding mode for the native paths (shared by the grid and runtime).
    static constexpr round_mode rmode = rounding_of(F | policy_of<L> | policy_of<R>);

    // A clear diagnostic when the result grid is unrepresentable, instead of the
    // raw expected-deref / .value() below failing cryptically (mirrors add/mul).
    static_assert((grid_of<L> / grid_of<R>).has_value(),
                  "division: result grid not representable (notch/interval exceeds the "
                  "representable rational range) — coarsen the operand grids");

    // The native paths' endpoints rounded with the same mode as the runtime
    // quotient, so e.g. round_ceil can't escape the grid: onto the integers,
    // or onto the operands' notch 1/K.
    static constexpr grid result_grid = [] {
        constexpr interval q = (*(grid_of<L> / grid_of<R>)).Interval;
        if constexpr (native_div_integer)
            return grid{round_rat(to_rational(q.Lower), rmode, false), round_rat(to_rational(q.Upper), rmode, true)};
        else if constexpr (native_div_fixed) {
            // An endpoint on the notch stays; one off it rounds onto it.
            constexpr rational n = detail::notch64<L>, k = (rational{1} / n).value();
            auto               on = [&](const grid_rational& x, bool up) {
                return grid_divides_evenly(x, notch_of<L>)
                           ? to_rational(x)
                           : (rational{round_rat((to_rational(x) * k).value(), rmode, up)} * n).value();
            };
            return grid{interval{on(q.Lower, false), on(q.Upper, true)}, n};
        } else
            return *(grid_of<L> / grid_of<R>);
    }();

    // A result is checked, whatever its operands' policies.
    using result = inside<result_grid>;

    // The exact quotient of two anchored integer raws as one 64-bit fraction:
    // |J_L|·p_L·q_R and |J_R|·p_R·q_L bounded by 2^62 for every value.
    static constexpr bool int_quotient_fits = [] {
        if constexpr (!rational_storage<result> || !notched<L> || !notched<R> || !anchored<L> || !anchored<R> ||
                      wide_valued<L> || wide_valued<R>)
            return false;
        else {
            constexpr grid_wide lim   = grid_wide{1} << 62;
            auto                max_j = []<insidable B>() {
                const grid_wide lo = exact_quotient(lower_of<B>, notch_of<B>),
                                hi = exact_quotient(upper_of<B>, notch_of<B>);
                const grid_wide a = lo.negative() ? -lo : lo, b = hi.negative() ? -hi : hi;
                return a < b ? b : a;
            };
            const grid_wide       a = wide_numerator(notch_of<L>) * wide_denominator(notch_of<R>);
            const grid_wide       b = wide_numerator(notch_of<R>) * wide_denominator(notch_of<L>);
            return max_j.template operator()<L>() * a < lim && max_j.template operator()<R>() * b < lim;
        }
    }();

    template <policy_flag G = F>
    static constexpr bool needs_overflow_check =
        has_any_flag(G | F, checked) || is_checked(policy_of<L>) || is_checked(policy_of<R>);

    // For a nonzero divisor the op fails only on the checked rational path
    // (overflow). So when the divisor excludes zero AND this is false, `div`
    // returns a plain `result` rather than expected<result, errc>. The
    // operand grids may prove the quotient fits (quotient_fits_rational).
    // A wide-index operand's quotient may outgrow the 64-bit rational
    // whatever the policy, so that path always reports.
    static constexpr bool fits_rational = quotient_fits_rational<L, R>();
    static constexpr bool may_overflow_nonzero =
        !native_div && !fits_rational && (needs_overflow_check<F> != 0 || wide_valued<L> || wide_valued<R>);

    // Plain `result` when the op cannot fail (overflow-action, or the divisor
    // grid excludes zero with no rational overflow), else expected<result, errc>.
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

    if constexpr (native_div_fixed) {
        // ja·K and jb in the narrowest signed type that holds them: a 32-bit
        // divide where it fits (Q8.8, Q15, ...), else 64 bits, else a wide_int.
        constexpr grid_wide K    = wide_denominator(notch_of<L>);
        constexpr int       bits = signed_value_bits_of({exact_quotient(lower_of<L>, notch_of<L>) * K,
                                                         exact_quotient(upper_of<L>, notch_of<L>) * K,
                                                         exact_quotient(lower_of<R>, notch_of<R>),
                                                         exact_quotient(upper_of<R>, notch_of<R>)});
        using W    = std::conditional_t<(bits <= 32),
                                        std::int32_t,
                                        std::conditional_t<(bits <= 64), imax, wide_sint<limbs_for_bits(bits)>>>;
        const W jb = value_index<W>(rhs);
        if constexpr (!zero_unchecked)
            if (jb == W{0})
                return fail(errc::division_by_zero, "division by zero in div");
        const W ja = value_index<W>(lhs) * static_cast<W>(K);
        if constexpr (std::is_integral_v<W>)
            return from_value_index<result>(div_rounded(ja, jb, rmode));
        else
            return from_value_index<result>(rounded_div<rmode>(ja, jb));
    } else if constexpr (native_div_integer) {
        using T         = native_div_t<L, R>;
        const T rhs_val = static_cast<T>(to_value(rhs));
        if constexpr (!zero_unchecked)
            if (rhs_val == 0)
                return fail(errc::division_by_zero, "division by zero in div");
        result res;
        from_value(res, imax{div_rounded(static_cast<T>(to_value(lhs)), rhs_val, rmode)});
        return res;
    } else if constexpr (wide_valued<L> || wide_valued<R>) {
        // A wide-index or big-grid operand: the exact quotient, in the result's raw.
        const auto d = exact_of(rhs);
        if constexpr (!zero_unchecked)
            if (d.Num.is_zero())
                return fail(errc::division_by_zero, "division by zero in div");
        if constexpr (fraction_storage<result>) {
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
    } else if constexpr (int_quotient_fits) {
        // Two integer raws: J_L·Notch_L / (J_R·Notch_R) as one fraction of
        // value indices, reduced once — no operand decodes, no checked divide.
        constexpr imax a =
            static_cast<imax>(notch64<L>.Numerator) * static_cast<imax>(abs_den(notch64<R>.Denominator));
        constexpr imax b =
            static_cast<imax>(notch64<R>.Numerator) * static_cast<imax>(abs_den(notch64<L>.Denominator));
        const imax d = value_index<imax>(rhs) * b;
        if constexpr (!zero_unchecked)
            if (d == 0)
                return fail(errc::division_by_zero, "division by zero in div");
        const imax n = value_index<imax>(lhs) * a;
        return result::from_raw(d < 0 ? rational{-n, -d} : rational{n, d});
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
