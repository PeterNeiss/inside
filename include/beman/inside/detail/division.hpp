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
namespace beman::inside {
// divmod's result: the rounded quotient and its remainder, for
// `auto [q, r] = divmod(a, b);`.
template <insidable Q, insidable R>
struct divmod_result {
    Q Quotient;
    R Remainder;
};
} // namespace beman::inside

namespace beman::inside::detail {
// Both operands are plain integer grids and the caller accepted integer
// truncation (snap) — the prerequisite for native integer div / mod.
template <insidable L, insidable R, policy_flag F>
inline constexpr bool integer_ops =
    ((F | policy_of<L> | policy_of<R>)&snap) && integer_lattice<L> && integer_lattice<R>;

// ...and every value fits imax, so the builtin integer division applies.
template <insidable L, insidable R, policy_flag F>
inline constexpr bool integer_native_ops = integer_ops<L, R, F> && values_fit_imax<L> && values_fit_imax<R>;

// Whether `l / r`, `l % r` and divmod have a result under flags F, as
// constraints of div / mod / divmod and their operators: the quotient grid
// exists (not a zero divisor grid; with 64-bit grid numbers, its corners
// fit), and the remainder needs integer-valued grids (divmod: within imax).
// ...or past imax but with 64-bit limits (a uint64 grid, the 2^63 of
// INT64_MIN / -1): the rounded quotient in exact wide integers, on a grid of
// the corner quotients rounded the same way.
template <insidable L, insidable R, policy_flag F>
inline constexpr bool integer_wide_ops =
    integer_ops<L, R, F> && !integer_native_ops<L, R, F> && fits_rational(lower_of<L>) && fits_rational(upper_of<L>) &&
    fits_rational(lower_of<R>) && fits_rational(upper_of<R>);

template <insidable L, insidable R, policy_flag F>
inline constexpr bool quotient_grid_representable =
    (integer_native_ops<L, R, F> || integer_wide_ops<L, R, F>) ? lower_of<R> != 0 || upper_of<R> != 0
                                                               : grid_quotient_fits(grid_of<L>, grid_of<R>);

// (A zero divisor grid has no remainder grid: |r| < |b| holds no value.)
template <insidable L, insidable R, policy_flag F>
inline constexpr bool modulo_operands_integer_valued =
    integer_ops<L, R, F | snap> && (lower_of<R> != 0 || upper_of<R> != 0);

template <insidable L, insidable R, policy_flag F>
inline constexpr bool divmod_operands_integer_valued_within_imax =
    integer_native_ops<L, R, F | snap> && quotient_grid_representable<L, R, F | snap>;

//---------------------------------------------------------------------------
// Rounding mode for the native div & mod paths (fire when `snap` is set).
// Decided from the combined flags by rounding_of (policy_flag.hpp), the one
// precedence all rounding paths share; `snap` alone is truncate-toward-zero.
// The runtime quotient and the compile-time grid endpoints MUST agree on the
// mode (both read rounding_of), or a result could escape its own grid.
//---------------------------------------------------------------------------

// The signed exact quotient a/b (b != 0) rounded to an integer per `m`, and
// the remainder a − q·b consistent with it (computed without forming q·b,
// which can pass T when q rounds away from zero).
template <std::signed_integral T>
struct rounded_division {
    T Quotient, Remainder;
};

template <std::signed_integral T>
constexpr rounded_division<T> div_rem_rounded(T a, T b, round_mode m) noexcept {
    using U = std::make_unsigned_t<T>;
    if (b == -1) // exact; a / -1 and a % -1 are UB for T::min (−T::min wraps: no caller's grid holds it)
        return {T(U(0) - U(a)), T(0)};
    const T    t   = a / b;                        // C++ truncation toward zero
    const T    r   = a % b;                        // sign of a, |r| < |b|
    const bool neg = (a < 0) != (b < 0);           // exact quotient is negative
    const U    ar  = r < 0 ? U(~U(r) + 1u) : U(r); // |r|, |b| in U (safe for T::min)
    const U    ab  = b < 0 ? U(~U(b) + 1u) : U(b);
    if (!rounds_away(m, neg, classify_remainder(m, ar, ab), (t & 1) != 0))
        return {t, r};
    // One step away from zero; r and b differ in sign (neg) or agree, so the
    // adjusted remainder stays below |b| in magnitude.
    return neg ? rounded_division<T>{T(t - 1), T(r + b)} : rounded_division<T>{T(t + 1), T(r - b)};
}

template <std::signed_integral T>
constexpr T div_rounded(T a, T b, round_mode m) noexcept {
    return div_rem_rounded(a, b, m).Quotient;
}

// The native quotient q of a / b stored into Q. Q's grid reaches 2^63 when
// a can be INT64_MIN and b can be −1 — past from_value's imax, and where
// div_rem_rounded's q wraps — so such a Q stores through its exact index.
template <insidable Q, std::signed_integral T>
constexpr Q native_quotient(T a, T b, T q) noexcept {
    Q res;
    if constexpr (values_fit_imax<Q>)
        from_value(res, imax{q});
    else {
        constexpr std::size_t K = exact_limbs<Q>;
        using I                 = wide_sint<K>;
        const I v               = a == std::numeric_limits<T>::min() && b == T{-1} ? -I{a} : I{q};
        res                     = exact_result<Q>(exact_frac<K>{v, I{1}});
    }
    return res;
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
constexpr rational round_rat(rational q, round_mode m, bool upper) noexcept {
    if (m == round_mode::half_even)
        m = upper ? round_mode::ceil : round_mode::floor;
    return round_to_integral(q, m); // exact: INT64_MIN / -1 reaches 2^63
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
                      !anchored<L> || !anchored<R> || wide_numerator(notch_of<L>) != grid_wide{1} ||
                      !(grid_wide{1} < wide_denominator(notch_of<L>)) || !(grid_of<L> / grid_of<R>).has_value())
            return false;
        else
            return ((F | policy_of<L> | policy_of<R>)&snap) != 0;
    }();

    static constexpr bool wide_div_integer = integer_wide_ops<L, R, F>;

    static constexpr bool native_div = native_div_integer || native_div_fixed || wide_div_integer;

    // The rounding mode for the native paths (shared by the grid and runtime).
    static constexpr round_mode rmode = rounding_of(F | policy_of<L> | policy_of<R>);

    // A clear diagnostic when the result grid is unrepresentable, instead of the
    // raw expected-deref / .value() below failing cryptically (mirrors add/mul).
    // The native integer path forms no exact quotient grid (below).
    static_assert(quotient_grid_representable<L, R, F>,
                  "division: result grid not representable (notch/interval exceeds the "
                  "representable rational range) — coarsen the operand grids");

    // The native integer quotient's range: the corner quotients rounded by
    // the runtime's mode (half_even: the low end by floor, the high by ceil,
    // as round_rat). a/b is monotonic in each operand on either side of 0,
    // and so is its rounding, so these are the rounded endpoints of the exact
    // quotient grid — which is not formed: its endpoints can pass the 64-bit
    // rational (7/2^63 for a divisor reaching INT64_MIN).
    static constexpr interval integer_quotient_range() {
        const round_mode lo_mode = rmode == round_mode::half_even ? round_mode::floor : rmode;
        const round_mode hi_mode = rmode == round_mode::half_even ? round_mode::ceil : rmode;
        // INT64_MIN / -1 is 2^63, past imax.
        auto quotient = [](imax a, imax b, round_mode m) {
            return a == std::numeric_limits<imax>::min() && b == -1 ? -rational{a} : rational{div_rounded(a, b, m)};
        };
        // The divisor's extremes on each side of 0; the one nearest 0 is ±step
        // when its range reaches 0 (grid operator/).
        constexpr imax lo = lower_imax<R>, hi = upper_imax<R>;
        constexpr imax step = signed_numerator(to_rational(grid_of<R>.value_unit()));
        imax           divisors[4];
        int            n = 0;
        if (lo > 0 || hi < 0) {
            divisors[n++] = lo;
            divisors[n++] = hi;
        } else {
            if (hi > 0) {
                divisors[n++] = step;
                divisors[n++] = hi;
            }
            if (lo < 0) {
                divisors[n++] = lo;
                divisors[n++] = -step;
            }
        }
        if (n == 0) // a zero divisor grid (the static_assert above)
            return interval{0, 0};
        interval q{quotient(lower_imax<L>, divisors[0], lo_mode), quotient(lower_imax<L>, divisors[0], hi_mode)};
        for (const imax a : {lower_imax<L>, upper_imax<L>})
            for (int i = 0; i < n; ++i) {
                const rational l = quotient(a, divisors[i], lo_mode), h = quotient(a, divisors[i], hi_mode);
                if (l < q.Lower)
                    q.Lower = l;
                if (q.Upper < h)
                    q.Upper = h;
            }
        return q;
    }

    // integer_quotient_range for limits past imax (64-bit magnitudes): the
    // corner quotients rounded in umax, sign apart. |a / b| ≤ |a| for an
    // integer |b| ≥ 1, so every rounded quotient fits.
    static constexpr interval wide_integer_quotient_range() {
        const round_mode lo_mode = rmode == round_mode::half_even ? round_mode::floor : rmode;
        const round_mode hi_mode = rmode == round_mode::half_even ? round_mode::ceil : rmode;
        struct signed_mag {
            umax Mag;
            bool Neg;
        };
        auto of = [](const rational& r) { return signed_mag{r.Numerator, r.Denominator < 0 && r.Numerator != 0}; };
        auto quotient = [](signed_mag a, signed_mag b, round_mode m) {
            const umax q = a.Mag / b.Mag, r = a.Mag % b.Mag;
            const bool neg = a.Neg != b.Neg && a.Mag != 0;
            const umax mag = rounds_away(m, neg, classify_remainder(m, r, b.Mag), (q & 1) != 0) ? q + 1 : q;
            return neg ? -rational{mag} : rational{mag};
        };
        const signed_mag lo = of(lower64<R>), hi = of(upper64<R>);
        const umax       step = to_rational(grid_of<R>.value_unit()).Numerator;
        signed_mag       divisors[4];
        int              n = 0;
        if (!lo.Neg && lo.Mag != 0) {
            divisors[n++] = lo;
            divisors[n++] = hi;
        } else if (hi.Neg) {
            divisors[n++] = lo;
            divisors[n++] = hi;
        } else {
            if (hi.Mag != 0) {
                divisors[n++] = signed_mag{step, false};
                divisors[n++] = hi;
            }
            if (lo.Neg) {
                divisors[n++] = lo;
                divisors[n++] = signed_mag{step, true};
            }
        }
        if (n == 0) // a zero divisor grid (the static_assert above)
            return interval{0, 0};
        const signed_mag a0 = of(lower64<L>);
        interval         q{quotient(a0, divisors[0], lo_mode), quotient(a0, divisors[0], hi_mode)};
        for (const signed_mag a : {of(lower64<L>), of(upper64<L>)})
            for (int i = 0; i < n; ++i) {
                const rational l = quotient(a, divisors[i], lo_mode), h = quotient(a, divisors[i], hi_mode);
                if (l < q.Lower)
                    q.Lower = l;
                if (q.Upper < h)
                    q.Upper = h;
            }
        return q;
    }

    // The native paths' endpoints rounded with the same mode as the runtime
    // quotient, so e.g. round_ceil can't escape the grid: onto the integers,
    // or onto the operands' notch 1/K.
    static constexpr grid result_grid = [] {
        if constexpr (native_div_integer)
            return grid{integer_quotient_range(), rational{1}};
        else if constexpr (wide_div_integer)
            return grid{wide_integer_quotient_range(), rational{1}};
        else if constexpr (native_div_fixed) {
            constexpr interval q = (*(grid_of<L> / grid_of<R>)).Interval;
            // An endpoint on the notch stays; one off it rounds onto it.
            constexpr rational n = detail::notch64<L>, k = (rational{1} / n).value();
            auto               on = [&](const grid_rational& x, bool up) {
                return grid_divides_evenly(x, notch_of<L>)
                           ? to_rational(x)
                           : (round_rat((to_rational(x) * k).value(), rmode, up) * n).value();
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
                const grid_wide a = abs(lo), b = abs(hi);
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

    // `x / just<c>` (c != 0) off the native paths: the result keeps x's
    // lattice scaled by 1/|c| (grid operator/), so it is x's offset (see
    // rep.hpp) — or, past 64 bits, the exact quotient on that lattice.
    // Exact and in range by construction: it cannot fail.
    static constexpr bool point_divisor = !native_div && point_grid<R> && lower_of<R> != 0 && notched<result>;

    // A continuous quotient whose denominator bound proves it fits (rep.hpp)
    // cannot overflow either.
    static constexpr bool bound_fits = exact_fits<exact_op::div, result, L, R>;

    static constexpr bool may_overflow_nonzero =
        !native_div && !point_divisor && !fits_rational && !bound_fits &&
        (needs_overflow_check<F> != 0 || wide_valued<L> || wide_valued<R> || wide_valued<result>);

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
        // ja·K, jb and the quotient in the narrowest signed type that holds
        // them: a 32-bit divide where it fits (Q8.8, Q15, ...), else 64 bits,
        // else a wide_int. The quotient's magnitude is at most |ja·K|; −ja·K
        // is listed for the one it can reach past the type, min / −1.
        constexpr grid_wide K     = wide_denominator(notch_of<L>);
        constexpr grid_wide ja_lo = exact_quotient(lower_of<L>, notch_of<L>) * K;
        constexpr int       bits  = signed_value_bits_of({ja_lo,
                                                          -ja_lo,
                                                          exact_quotient(upper_of<L>, notch_of<L>) * K,
                                                          exact_quotient(lower_of<R>, notch_of<R>),
                                                          exact_quotient(upper_of<R>, notch_of<R>)});
        using W    = std::conditional_t<(bits <= 31), std::int32_t, work_int_t<bits>>; // bits: without the sign
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
        const T lhs_val = static_cast<T>(to_value(lhs));
        return native_quotient<result>(lhs_val, rhs_val, div_rounded(lhs_val, rhs_val, rmode));
    } else if constexpr (wide_div_integer) {
        // Integer values past imax: the rounded quotient in exact wide
        // integers, as modulo's remainder.
        constexpr std::size_t K = exact_limbs<L, R, result>;
        using I                 = wide_sint<K>;
        const I b               = value_in_units<I, grid_rational{1}>(rhs); // integer values: no division
        if constexpr (!zero_unchecked)
            if (b.is_zero())
                return fail(errc::division_by_zero, "division by zero in div");
        const I a = value_in_units<I, grid_rational{1}>(lhs);
        return exact_result<result>(exact_frac<K>{rounded_div<rmode>(a, b), I{1}});
    } else if constexpr (point_divisor) {
        if constexpr (point_scale<R, L, result>)
            return scale_by_point<result, (lower_of<R> < 0)>(lhs);
        else
            return exact_result<result>(exact_of(lhs) / exact_of(rhs));
    } else if constexpr (wide_valued<L> || wide_valued<R> || wide_valued<result>) {
        // A wide-index or big-grid operand: the exact quotient, in the result's raw.
        const auto d = exact_of(rhs);
        if constexpr (!zero_unchecked)
            if (d.Num.is_zero())
                return fail(errc::division_by_zero, "division by zero in div");
        if constexpr (fraction_storage<result>) {
            // Grids past 64 bits: the exact quotient in the result's wide fraction.
            const auto q = frac_raw_of<raw_t<result>>(exact_of(lhs) / d);
            if constexpr (!bound_fits)
                if (!q) [[unlikely]]
                    return fail(errc::overflow, "quotient past its fraction raw in div");
            return result::from_raw(*q);
        } else {
            const auto q = try_rational(exact_of(lhs) / d);
            if constexpr (!bound_fits)
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
    } else if constexpr (needs_overflow_check<G> && !fits_rational && !bound_fits) {
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
// modulo (requires integer-valued grids)
//---------------------------------------------------------------------------
template <insidable L, insidable R, policy_flag F = none>
struct modulo {
    // `a mod b` is the remainder of an integer quotient, so it always rounds:
    // truncation (C++ `%`) unless the policy names a mode — with or without
    // `snap`, which only decides whether `/` rounds.
    static constexpr policy_flag FS = F | snap;

    // Hard requirement, not a fallback: `a mod b` is only defined for integer
    // operands, so the grid must be integer-aligned.
    static_assert(integer_ops<L, R, FS>, "modulo requires integer-valued grids");

    // Builtin division when every value fits imax; else exact wide integers.
    static constexpr bool native_mod = integer_native_ops<L, R, FS>;

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
        const I b                     = value_in_units<I, grid_rational{1}>(rhs); // integer values: no division
        if constexpr (!zero_unchecked)
            if (b.is_zero())
                return report_or_unexpected<result>(action, policy, errc::division_by_zero, "division by zero in mod");
        const I a = value_in_units<I, grid_rational{1}>(lhs);
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
        from_value(res, imax{div_rem_rounded(lhs_val, rhs_val, rmode).Remainder});
        return res;
    }
}

//---------------------------------------------------------------------------
// divmod (what `%` requires, with every value within imax): the rounded
// quotient and the remainder consistent with it, r = a − q·b, from one native
// division. The two halves have the grids of `a / b` and `a % b`.
//---------------------------------------------------------------------------
template <insidable L, insidable R, policy_flag F = none>
struct quotient_remainder {
    // Rounds like `%`, so like `%` it needs no `snap`.
    static constexpr policy_flag FS = F | snap;

    static_assert(integer_native_ops<L, R, FS>, "divmod requires integer-valued grids within imax");

    static constexpr round_mode rmode = rounding_of(FS | policy_of<L> | policy_of<R>);

    using result = divmod_result<typename division<L, R, FS>::result, typename modulo<L, R, FS>::result>;

    template <typename A>
    using return_t = std::
        conditional_t<overflow_action<plain_t<A>> || divisor_excludes_zero<R>, result, std::expected<result, errc>>;

    template <policy_flag G = F, typename E = empty_ref, typename A = no_action>
    static constexpr return_t<A> divmod(L lhs, R rhs, policy<G, E> policy = {}, A&& action = {}) {
        using T         = native_div_t<L, R>;
        const T rhs_val = static_cast<T>(to_value(rhs));
        if constexpr (!divisor_unchecked<L, R, F, G>)
            if (rhs_val == 0) {
                if constexpr (overflow_action<plain_t<A>>) {
                    result res{};
                    action.Fn(res.Quotient, errc::division_by_zero);
                    return res;
                } else {
                    if constexpr (uses_error_ref<std::remove_cvref_t<decltype(policy)>>)
                        policy.report(errc::division_by_zero);
                    return std::unexpected{errc::division_by_zero};
                }
            }
        const T    lhs_val = static_cast<T>(to_value(lhs));
        const auto qr      = div_rem_rounded(lhs_val, rhs_val, rmode);
        result     res{native_quotient<typename division<L, R, FS>::result>(lhs_val, rhs_val, qr.Quotient), {}};
        from_value(res.Remainder, imax{qr.Remainder});
        return res;
    }
};

} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_DETAIL_DIVISION_HPP
