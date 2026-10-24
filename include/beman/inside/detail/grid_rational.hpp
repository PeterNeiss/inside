// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_GRID_RATIONAL_HPP
#define BEMAN_INSIDE_DETAIL_GRID_RATIONAL_HPP

#include <beman/inside/detail/config.hpp>
BEMAN_INSIDE_WARNINGS_PUSH

#include <beman/inside/detail/rational.hpp>
#include <beman/inside/detail/number_text.hpp> // the literal grammar (_g)
#include <beman/inside/detail/wide_int.hpp>

//---------------------------------------------------------------------------
// grid_rational — the number type of a grid's limits and notch (the NTTP
// substrate of `interval` and `grid`).
//
// BEMAN_INSIDE_BIG_GRIDS selects it. Under C++26 static reflection
// (std::define_static_array) a grid number has no size limit: its limbs are
// interned in static storage, so equal values stay the same template argument.
// Without reflection (C++23, older compilers) it is the 64-bit runtime
// `rational`, and grids keep 64-bit limits. Define the macro to 0 to force the
// 64-bit grids on a C++26 compiler.
//
// The two modes give `grid` and `interval` different layouts, so each mode
// lives in its own inline namespace: linking a C++23 TU against a C++26 one is
// a link error, not a silent ODR violation.
//---------------------------------------------------------------------------
// (BEMAN_INSIDE_BIG_GRIDS is resolved in config.hpp.)

#include <beman/inside/detail/big_rational.hpp> // empty without big grids

#if BEMAN_INSIDE_BIG_GRIDS
    #define BEMAN_INSIDE_GRID_ABI big_grids_v2
#else
    #define BEMAN_INSIDE_GRID_ABI small_grids_v2
#endif

//---------------------------------------------------------------------------
// The grid-number vocabulary, the same in both modes:
//   grid_rational          the type of a grid's limits and notch
//   grid_int              exact integers for grid computations (slot counts,
//                          value indices): wide enough for every grid
//   wide_numerator(r) /    a grid number's signed numerator and positive
//   wide_denominator(r)    denominator as grid_int (also for a rational)
//   grid_divides_evenly    a / n is an integer (true for n == 0)
//   grid_same_lattice      (a − b) / n is an integer (true for n == 0)
//   grid_gcd               gcd of two fractions; may fail only with 64-bit
//                          grid numbers (returns expected there)
//   fits_rational(r) /     whether, and as which, 64-bit rational a grid
//   to_rational(r)         number serves the 64-bit-only paths
//---------------------------------------------------------------------------
namespace beman::inside::detail {
// (a − b) / n is an integer (true for n == 0), for 64-bit rationals. A
// difference past the rational range is tested in four limbs:
// (a − b)/n = (a_n·b_d − b_n·a_d)·n_d / (a_d·b_d·n_n), whose products of
// three 64-bit magnitudes fit.
constexpr bool same_lattice64(const rational& a, const rational& b, const rational& n) {
    if (n == 0)
        return true;
    if (const auto d = try_sub(a, b)) [[likely]]
        return divides_evenly(*d, n);
    using W        = wide_sint<4>;
    auto    num_of = [](const rational& r) { return r.Denominator < 0 ? -W{r.Numerator} : W{r.Numerator}; };
    auto    den_of = [](const rational& r) { return W{abs_den(r.Denominator)}; };
    const W num    = (num_of(a) * den_of(b) - num_of(b) * den_of(a)) * den_of(n);
    const W den    = den_of(a) * den_of(b) * num_of(n);
    return num % den == W{0};
}

#if BEMAN_INSIDE_BIG_GRIDS
using grid_rational = big_rational;
using grid_int      = big_int;

constexpr grid_int wide_numerator(const big_rational& r) { return r.Num; }
constexpr grid_int wide_denominator(const big_rational& r) { return r.Den; }

// A grid number as a transient fraction, for tests that run at runtime too.
constexpr big::fraction fraction_of(const big_rational& r) {
    return {r.Num.magnitude(), r.Den.magnitude(), r.Num.negative()};
}

// (a − b) / n is an integer (true for n == 0): b and a share n's lattice.
// (No big value is formed, so the test runs at runtime too.)
constexpr bool grid_same_lattice(const big_rational& a, const big_rational& b, const big_rational& n) {
    if (n == 0)
        return true;
    if (a.fits_rational() && b.fits_rational() && n.fits_rational())
        return same_lattice64(a, b, n);
    return big::same_lattice(fraction_of(a), fraction_of(b), fraction_of(n));
}

// (Convention, as divides_evenly: everything divides 0 evenly.)
constexpr bool grid_divides_evenly(const big_rational& a, const big_rational& n) {
    return grid_same_lattice(a, big_rational{0}, n);
}

// The 64-bit rational of a grid number, for 64-bit-only paths.
constexpr bool         fits_rational(const big_rational& r) { return r.fits_rational(); }
constexpr rational     to_rational(const big_rational& r) { return r; }
constexpr big_rational grid_gcd(const big_rational& a, const big_rational& b) { return gcd(a, b); }
#else
using grid_rational = rational;
// A product of three 64-bit magnitudes plus a sign.
using grid_int = wide_sint<4>;

constexpr bool grid_divides_evenly(const rational& a, const rational& n) { return divides_evenly(a, n); }

constexpr bool grid_same_lattice(const rational& a, const rational& b, const rational& n) {
    return same_lattice64(a, b, n);
}

constexpr bool                          fits_rational(const rational&) { return true; }
constexpr rational                      to_rational(const rational& r) { return r; }
constexpr std::expected<rational, errc> grid_gcd(const rational& a, const rational& b) { return gcd(a, b); }
#endif

// grid_gcd as a callable, for lift over expected grid numbers.
inline constexpr auto grid_gcd_of_two = [](const grid_rational& a, const grid_rational& b) { return grid_gcd(a, b); };

constexpr grid_int wide_numerator(const rational& r) noexcept {
    const grid_int n{r.Numerator};
    return r.Denominator < 0 ? -n : n;
}
constexpr grid_int wide_denominator(const rational& r) noexcept { return grid_int{abs_den(r.Denominator)}; }

// + − × and gcd of grid numbers as grid numbers in both modes, for the
// compile-time grid computations (a 64-bit overflow stops the build).
template <class T>
constexpr grid_rational grid_value(const T& v) {
    if constexpr (std::same_as<T, grid_rational>)
        return v;
    else
        return v.value();
}
constexpr grid_rational grid_add(const grid_rational& a, const grid_rational& b) { return grid_value(a + b); }
constexpr grid_rational grid_sub(const grid_rational& a, const grid_rational& b) { return grid_value(a - b); }
constexpr grid_rational grid_mul(const grid_rational& a, const grid_rational& b) { return grid_value(a * b); }
constexpr grid_rational grid_div_of(const grid_rational& a, const grid_rational& b) { return grid_value(a / b); }
constexpr grid_rational grid_gcd_of(const grid_rational& a, const grid_rational& b) {
    return grid_value(grid_gcd(a, b));
}
// gcd of two grid integers, non-negative (0 when both are).
constexpr grid_int grid_int_gcd(grid_int a, grid_int b) {
#if BEMAN_INSIDE_BIG_GRIDS
    return gcd(a, b);
#else
    return wide_gcd(a < grid_int{0} ? -a : a, b < grid_int{0} ? -b : b);
#endif
}
// Whether gcd(a, b) is a grid number: always with big grid numbers; with
// 64-bit ones a 2/3 against a 2^-62 has none (1/(3·2^62)).
constexpr bool grid_gcd_fits([[maybe_unused]] const grid_rational& a, [[maybe_unused]] const grid_rational& b) {
#if BEMAN_INSIDE_BIG_GRIDS
    return true;
#else
    return grid_gcd(a, b).has_value();
#endif
}

//---------------------------------------------------------------------------
// parse_grid_literal — the _g literal: decimal digits (with ' separators),
// an optional point and an optional e±n exponent, taken exactly. With big
// grid numbers any size; with 64-bit grid numbers the value must fit a
// 64-bit rational.
//---------------------------------------------------------------------------
template <char... Chars>
consteval grid_rational parse_grid_literal() {
    if constexpr (octal_literal<Chars...>())
        constexpr_error<"_g literal: a leading 0 makes an octal literal; write the number without it">();
    constexpr char text[] = {Chars...};
    const grid_int ten{10};
    // Times ten. 64-bit grid numbers compute in a 256-bit grid_int, which
    // holds every literal that can fit (a reduced denominator of 10^k is at
    // least 2^k, so k ≤ 62 and m ≤ 2^64·5^62): a value past 2^246 cannot fit,
    // so stop there rather than wrap.
    const auto times_ten = [&](const grid_int& v) {
#if !BEMAN_INSIDE_BIG_GRIDS
        if ((grid_int{1} << 246) < v)
            constexpr_error<"_g literal: past the 64-bit grid numbers (needs C++26 big grids)">();
#endif
        return v * ten;
    };
    // The value is m·10^e10. Zero digits are held back (`zeros`) and only
    // multiplied in before a later nonzero digit, so trailing zeros
    // (1.000…) never grow m.
    grid_int    m{0};
    int         e10 = 0, zeros = 0;
    std::size_t i     = 0;
    bool        point = false, digits = false;
    for (; i < sizeof(text); ++i) {
        const char c = text[i];
        if (c == '\'')
            continue;
        if (c == '.' && !point) {
            point = true;
            continue;
        }
        if (c < '0' || c > '9')
            break;
        if (c == '0')
            ++zeros;
        else {
            for (; zeros > 0; --zeros)
                m = times_ten(m);
            m = times_ten(m) + grid_int{c - '0'};
        }
        if (point)
            --e10;
        digits = true;
    }
    if (!digits)
        constexpr_error<"_g literal: expected decimal digits">();
    e10 += zeros;
    if (i < sizeof(text)) {
        if (text[i] != 'e' && text[i] != 'E')
            constexpr_error<"_g literal: decimal digits, a point and e±n only">();
        ++i;
        bool neg = false;
        if (i < sizeof(text) && (text[i] == '+' || text[i] == '-')) {
            neg = text[i] == '-';
            ++i;
        }
        if (i == sizeof(text))
            constexpr_error<"_g literal: missing exponent digits">();
        int e = 0;
        for (; i < sizeof(text); ++i) {
            if (text[i] < '0' || text[i] > '9')
                constexpr_error<"_g literal: decimal digits, a point and e±n only">();
            e = e * 10 + (text[i] - '0');
            if (e > 10000)
                constexpr_error<"_g literal: exponent too large">();
        }
        e10 += neg ? -e : e;
    }
    grid_int num = m, den{1};
    if (!(m == grid_int{0}))
        for (; e10 != 0; e10 += e10 > 0 ? -1 : 1)
            (e10 > 0 ? num : den) = times_ten(e10 > 0 ? num : den);
#if BEMAN_INSIDE_BIG_GRIDS
    return big_rational{num, den};
#else
    const grid_int a = wide_gcd(num, den); // reduce (num ≥ 0), then it must fit
    num              = num / a;
    den              = den / a;
    if (grid_int{std::numeric_limits<umax>::max()} < num || grid_int{std::numeric_limits<imax>::max()} < den)
        constexpr_error<"_g literal: past the 64-bit grid numbers (needs C++26 big grids)">();
    return rational{static_cast<umax>(num), static_cast<imax>(den)};
#endif
}
} // namespace beman::inside::detail

namespace beman::inside {
// A grid number: `inside<{0, 1267650600228229401496703205376_g}>`, or an
// exact decimal notch `{{0, 1}, 1e-30_g}`. Any size under C++26 big grids.
template <char... Chars>
consteval detail::grid_rational operator""_g() {
    return detail::parse_grid_literal<Chars...>();
}
} // namespace beman::inside

BEMAN_INSIDE_WARNINGS_POP

#endif // BEMAN_INSIDE_DETAIL_GRID_RATIONAL_HPP
