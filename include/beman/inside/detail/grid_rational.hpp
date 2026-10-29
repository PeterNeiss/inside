// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_GRID_RATIONAL_HPP
#define BEMAN_INSIDE_DETAIL_GRID_RATIONAL_HPP

#include <beman/inside/detail/config.hpp>

#include <beman/inside/detail/rational.hpp>
#include <beman/inside/detail/number_text.hpp> // the literal grammar (_g)
#include <beman/inside/detail/big_rational.hpp>

//---------------------------------------------------------------------------
// The grid-number vocabulary. A grid's limits and notch (the NTTP substrate
// of `interval` and `grid`) are big_rationals, of any size: C++26 static
// reflection (std::define_static_array) interns their limbs in static
// storage, so equal values stay the same template argument.
//   grid_rational          the type of a grid's limits and notch
//   grid_int               exact integers for grid computations (slot counts,
//                          value indices): wide enough for every grid
//   wide_numerator(r) /    a grid number's signed numerator and positive
//   wide_denominator(r)    denominator as grid_int (also for a rational)
//   grid_divides_evenly    a / n is an integer (true for n == 0)
//   grid_same_lattice      (a − b) / n is an integer (true for n == 0)
//   fits_rational(r) /     whether, and as which, 64-bit rational a grid
//   to_rational(r)         number serves the 64-bit fast paths
// + − × / and gcd are big_rational's own.
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
    auto    num_of = [](const rational& r) { return signed_numerator<W>(r); };
    auto    den_of = [](const rational& r) { return W{abs_den(r.Denominator)}; };
    const W num    = (num_of(a) * den_of(b) - num_of(b) * den_of(a)) * den_of(n);
    const W den    = den_of(a) * den_of(b) * num_of(n);
    return num % den == W{0};
}

using grid_rational = big_rational;
using grid_int      = big_int;

constexpr grid_int wide_numerator(const big_rational& r) { return r.Num; }
constexpr grid_int wide_denominator(const big_rational& r) { return r.Den; }

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

// The 64-bit rational of a grid number, for the 64-bit fast paths.
constexpr bool     fits_rational(const big_rational& r) { return r.fits_rational(); }
constexpr rational to_rational(const big_rational& r) { return r; }

constexpr grid_int wide_numerator(const rational& r) noexcept {
    const grid_int n{r.Numerator};
    return r.Denominator < 0 ? -n : n;
}
constexpr grid_int wide_denominator(const rational& r) noexcept { return grid_int{abs_den(r.Denominator)}; }

// Value bits a signed integer needs to hold every value in [lo, hi].
constexpr int signed_value_bits(const grid_int& lo, const grid_int& hi) noexcept {
    auto      mag = [](const grid_int& v) { return bit_width_of(v.negative() ? -(v + grid_int{1}) : v); };
    const int a = mag(lo), b = mag(hi);
    return a > b ? a : b;
}

// r rounded to an integer by M (half away from zero for nearest).
template <round_mode M>
constexpr grid_rational grid_to_int(const grid_rational& r) {
    const grid_int n = wide_numerator(r), d = wide_denominator(r);
    grid_int       q   = n / d;
    const grid_int rem = n - q * d;
    const bool     neg = n.negative();
    const bool     odd = !(q / grid_int{2} * grid_int{2} == q);
    if (rounds_away(M, neg, classify_remainder(M, neg ? -rem : rem, d), odd))
        q = neg ? q - grid_int{1} : q + grid_int{1};
    return grid_rational{q};
}

//---------------------------------------------------------------------------
// parse_grid_literal — the _g literal: the decimal forms of the literal
// grammar (scan_number: digits with ' separators, an optional point, an
// optional e±n exponent), taken exactly, of any size.
//---------------------------------------------------------------------------
template <char... Chars>
consteval grid_rational parse_grid_literal() {
    if constexpr (octal_literal<Chars...>())
        constexpr_error<"_g literal: a leading 0 makes an octal literal; write the number without it">();
    constexpr char text[] = {Chars...};
    const grid_int ten{10};
    // The value is m·10^e10. Zero digits are held back (`zeros`) and only
    // multiplied in before a later nonzero digit, so trailing zeros
    // (1.000…) never grow m.
    grid_int             m{0};
    int                  e10 = 0, zeros = 0;
    const scanned_number sc = scan_number(text, text + sizeof(text), [&](int d, bool in_frac, int) {
        if (d == 0)
            ++zeros;
        else {
            for (; zeros > 0; --zeros)
                m = m * ten;
            m = m * ten + grid_int{d};
        }
        e10 -= in_frac;
        return true;
    });
    if (sc.Fail == parse_fail::empty)
        constexpr_error<"_g literal: expected decimal digits">();
    if (sc.Fail != parse_fail::none || sc.Base != 10)
        constexpr_error<"_g literal: decimal digits, a point and e±n only">();
    if (sc.Exp > 10000)
        constexpr_error<"_g literal: exponent too large">();
    if (m == grid_int{0})
        return big_rational{0};
    e10 += zeros + (sc.ExpNeg ? -sc.Exp : sc.Exp);
    const grid_int p = pow_by_squaring(ten, static_cast<unsigned>(e10 < 0 ? -e10 : e10));
    return e10 < 0 ? big_rational{m, p} : big_rational{m * p, grid_int{1}};
}
} // namespace beman::inside::detail

namespace beman::inside {
// A grid number: `inside<{0, 1267650600228229401496703205376_g}>`, or an
// exact decimal notch `{{0, 1}, 1e-30_g}`. Any size.
template <char... Chars>
consteval detail::grid_rational operator""_g() {
    return detail::parse_grid_literal<Chars...>();
}
} // namespace beman::inside

#endif // BEMAN_INSIDE_DETAIL_GRID_RATIONAL_HPP
