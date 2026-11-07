// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_MATH_CONSTANTS_HPP
#define BEMAN_INSIDE_MATH_CONSTANTS_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/compare.hpp>
#include <beman/inside/math/detail/cores.hpp>

#include <algorithm>
#include <bit>
#include <compare>

//---------------------------------------------------------------------------
// Mathematical constants, computed while compiling to the precision each use
// needs — no fixed width:
//   Out o = math::pi;          correctly rounded onto Out, by Out's rounding
//   x * math::pi               π as the simplest fraction (a continued-fraction
//   x + math::e, x / math::ln2 convergent) within 2^-32 of x's resolution:
//                              x's notch, or 1/MaxDenominator; a point x, its
//                              value's 2^-32 — so a coarse x keeps 64-bit
//                              grid numbers, a fine one a precise point
//   x < math::pi, x == math::e exact: π against the grid point next to it
//   math::pi.within<1e-40_g>   the point (a convergent) within 10^-40
// Each value comes from the engine's series (math/detail/adaptive.hpp) at as
// many bits as asked, within one unit; γ by Brent–McMillan, Catalan's G and
// ζ(3) by binomial series.
//---------------------------------------------------------------------------
namespace beman::inside::math {
namespace detail {
enum class constant_id : unsigned char {
    pi,
    two_pi,
    half_pi,
    inv_pi,
    inv_sqrt_pi,
    e,
    log2e,
    log10e,
    ln2,
    ln10,
    sqrt2,
    sqrt3,
    inv_sqrt2,
    inv_sqrt3,
    phi,
    egamma,
    catalan,
    zeta3
};

namespace constants {
using namespace ::beman::inside::detail;
using namespace ::beman::inside::math::detail::engine;

// v·q for a small q (a series' recurrence), exact.
template <std::size_t K>
constexpr wide_sint<K> mul_small(const wide_sint<K>& v, umax q) noexcept {
    const bool   neg = v.negative();
    wide_uint<K> m{neg ? -v : v};
    limb::mul_1(m.Word, m.Word, K, q);
    const wide_sint<K> r{m};
    return neg ? -r : r;
}

// ⌊√(a·2^2G)⌋: √a at scale G, within one unit.
template <int G>
constexpr fixed_t<G + 8> sqrt_of(umax a) noexcept {
    using T = fixed_t<2 * G + 8>;
    return static_cast<fixed_t<G + 8>>(isqrt(T{a} << (2 * G)));
}

// e = Σ 1/k!: each truncation within one unit.
template <int G>
constexpr fixed_t<G + 8> e_series() noexcept {
    using I = fixed_t<G + 8>;
    I t     = one_q<limbs_of<I>>(G), s{0};
    for (umax k = 1; !t.is_zero(); ++k) {
        s = s + t;
        t = div_small(t, k);
    }
    return s;
}

// γ by Brent–McMillan: with n = 2^m, B_k = (n^k/k!)², A_k = (A_{k−1}·n²/k +
// B_k)/k from A_0 = −ln n, γ = ΣA/ΣB within π·e^-4n < 2^-(G+8). The terms
// reach e^2n: the words carry 3n bits more.
template <int G>
constexpr fixed_t<G + 8> egamma_series() noexcept {
    constexpr int  m  = std::bit_width(static_cast<unsigned>((G + 8) / 5 + 1));
    constexpr umax n2 = umax{1} << (2 * m);
    using J           = fixed_t<G + 3 * (1 << m) + 24>;
    J b               = one_q<limbs_of<J>>(G);
    J a               = -mul_small(static_cast<J>(ln2_q<G>), static_cast<umax>(m));
    J u = a, v = b;
    for (umax k = 1; !(a.is_zero() && b.is_zero()); ++k) {
        b = div_small(div_small(mul_small(b, n2), k), k);
        a = div_small(div_small(mul_small(a, n2), k) + b, k);
        u = u + a;
        v = v + b;
    }
    using D = fixed_t<2 * G + 3 * (1 << m) + 40>;
    return static_cast<fixed_t<G + 8>>((D{u} << G) / D{v});
}

// 1/C(2j+2, j+1) = 1/C(2j, j) · (j+1)/(2(2j+1)): the binomial series' terms.
// Catalan's G = π/8·ln(2+√3) + 3/8·Σ 1/((2j+1)²·C(2j, j)), with
// ln(2+√3) = 2·atanh(1/√3) = (2/√3)·Σ 3^-k/(2k+1).
template <int G>
constexpr fixed_t<G + 8> catalan_series() noexcept {
    using I     = fixed_t<G + 8>;
    const I one = one_q<limbs_of<I>>(G);
    I       s1{0}, t = one;
    for (umax k = 0; !t.is_zero(); ++k, t = div_small(t, 3))
        s1 = s1 + div_small(t, 2 * k + 1);
    const I inv_sqrt3 = static_cast<I>(isqrt((fixed_t<2 * G + 8>{1} << (2 * G)) / fixed_t<2 * G + 8>{3}));
    const I log_term  = mul_q(s1 << 1, inv_sqrt3, G);
    I       s2{0}, w = one;
    for (umax j = 0; !w.is_zero(); ++j) {
        s2 = s2 + div_small(div_small(w, 2 * j + 1), 2 * j + 1);
        w  = div_small(mul_small(w, j + 1), 2 * (2 * j + 1));
    }
    return (mul_q(static_cast<I>(pi_q<G>), log_term, G) + mul_small(s2, 3)) >> 3;
}

// ζ(3) = 5/2·Σ_{k≥1} (−1)^(k−1)/(k³·C(2k, k)).
template <int G>
constexpr fixed_t<G + 8> zeta3_series() noexcept {
    using I = fixed_t<G + 8>;
    I s{0}, w = one_q<limbs_of<I>>(G);
    for (umax k = 1;; ++k) {
        w = div_small(mul_small(w, k), 2 * (2 * k - 1));
        if (w.is_zero())
            break;
        const I t = div_small(div_small(div_small(w, k), k), k);
        s         = k % 2 != 0 ? s + t : s - t;
    }
    return mul_small(s, 5) >> 1;
}

// c·2^W within one unit. Every value is computed at G = W + 32 within 2^20
// units (a few truncations per series term), then rounded to W.
template <constant_id Id, int W>
inline constexpr fixed_t<W + 8> constant_q = [] {
    constexpr int G = W + 32;
    using I         = fixed_t<G + 8>;
    using T         = fixed_t<2 * G + 8>;
    I v;
    if constexpr (Id == constant_id::pi)
        v = static_cast<I>(pi_q<G>);
    else if constexpr (Id == constant_id::two_pi)
        v = static_cast<I>(pi_q<G>) << 1;
    else if constexpr (Id == constant_id::half_pi)
        v = static_cast<I>(pi_q<G>) >> 1;
    else if constexpr (Id == constant_id::inv_pi)
        v = static_cast<I>(inverse_q<G>(pi_q<G + 16>));
    else if constexpr (Id == constant_id::inv_sqrt_pi)
        v = static_cast<I>(isqrt(static_cast<T>(inverse_q<2 * G>(pi_q<2 * G + 16>))));
    else if constexpr (Id == constant_id::e)
        v = e_series<G>();
    else if constexpr (Id == constant_id::log2e)
        v = static_cast<I>(log2e_q<G>);
    else if constexpr (Id == constant_id::log10e)
        v = static_cast<I>(log10e_q<G>);
    else if constexpr (Id == constant_id::ln2)
        v = static_cast<I>(ln2_q<G>);
    else if constexpr (Id == constant_id::ln10)
        v = static_cast<I>(ln10_q<G>);
    else if constexpr (Id == constant_id::sqrt2)
        v = sqrt_of<G>(2);
    else if constexpr (Id == constant_id::sqrt3)
        v = sqrt_of<G>(3);
    else if constexpr (Id == constant_id::inv_sqrt2)
        v = sqrt_of<G>(2) >> 1;
    else if constexpr (Id == constant_id::inv_sqrt3)
        v = static_cast<I>(isqrt((T{1} << (2 * G)) / T{3}));
    else if constexpr (Id == constant_id::phi)
        v = (one_q<limbs_of<I>>(G) + sqrt_of<G>(5)) >> 1;
    else if constexpr (Id == constant_id::egamma)
        v = egamma_series<G>();
    else if constexpr (Id == constant_id::catalan)
        v = catalan_series<G>();
    else
        v = zeta3_series<G>();
    return static_cast<fixed_t<W + 8>>(round_shift(v, G - W));
}();

// The constant as an engine core: at W bits within one unit, so the driver
// escalates until Out's rounding is decided.
template <constant_id Id>
struct constant_core {
    template <int W>
    constexpr approx<limbs_of<fixed_t<W + 8>>> run() const noexcept {
        return {constant_q<Id, W>, W, 1};
    }
};

// The constant correctly rounded onto Out, at compile time.
template <class Out, constant_id Id>
inline constexpr Out stored = [] {
    static_assert(has_flag(flags_of<Out>, ::beman::inside::detail::snap),
                  "beman::inside::math: a constant is irrational - store it into a grid with a rounding mode "
                  "(round_nearest, round_floor, ...)");
    return evaluate<Out, start_bits<Out>>(constant_core<Id>{});
}();

consteval grid_rational pow2(int k) {
    return k >= 0 ? grid_rational{big_int{1} << k} : grid_rational{1} / grid_rational{big_int{1} << -k};
}

// The first convergent p/q of the constant (of its reciprocal when Inverse)
// within Eps of it. c lies within E/2^B of V/2^B (B twice Eps's bits and
// 64 more, so a convergent that close exists among V/2^B's).
template <constant_id Id, grid_rational Eps, bool Inverse = false>
consteval grid_rational convergent_within() {
    static_assert(Eps > 0);
    constexpr int k  = std::max(bit_width_of(grid_denominator(Eps)) - bit_width_of(grid_numerator(Eps)) + 2, 1);
    constexpr int B  = 2 * k + 64;
    constexpr int NB = bit_width_of(grid_numerator(Eps)) + bit_width_of(grid_denominator(Eps));
    using W          = fixed_t<3 * B + NB + 72>;
    const W D        = W{1} << B;
    W       V        = static_cast<W>(constant_q<Id, B>), E{1};
    if constexpr (Inverse) { // 1/c within 16 units: c ≥ 1/4 for every constant
        V = (W{1} << (2 * B)) / V;
        E = W{16};
    }
    const W en = static_cast<W>(grid_numerator(Eps)), ed = static_cast<W>(grid_denominator(Eps));
    W       num = V, den = D, h1{1}, h2{0}, q1{0}, q2{1};
    for (;;) {
        const auto [a, r] = W::divmod(num, den);
        const W h = a * h1 + h2, q = a * q1 + q2;
        W       diff = V * q - h * D; // |V/D − h/q| + E/D ≤ Eps, times q·D·ed
        if (diff.negative())
            diff = -diff;
        if (!(en * q * D < ed * (diff + E * q)))
            return grid_rational{big_int{h}, big_int{q}};
        num = den;
        den = r;
        h2  = h1;
        h1  = h;
        q2  = q1;
        q1  = q;
    }
}

// The resolution of X's values: its notch, 1/MaxDenominator on a continuous
// grid, 2^-32 of its value for a point. The largest magnitude, at least that.
template <insidable X>
consteval grid_rational resolution_of() {
    if (lower_of<X> == upper_of<X>) {
        const grid_rational v = abs(lower_of<X>);
        return (v == 0 ? grid_rational{1} : v) * pow2(-32);
    }
    if (notch_of<X> != 0)
        return notch_of<X>;
    return grid_rational{1} / grid_rational{den_bound(grid_of<X>)};
}
template <insidable X>
consteval grid_rational magnitude_of() {
    const grid_rational m = std::max(abs(lower_of<X>), abs(upper_of<X>));
    return m == 0 ? grid_rational{1} : m;
}
// The points for x op c: within 2^-32 of x's resolution, for a product
// scaled by |x|'s largest value.
template <constant_id Id, insidable X>
inline constexpr grid_rational sum_point = convergent_within<Id, resolution_of<X>() * pow2(-32)>();
template <constant_id Id, insidable X>
inline constexpr grid_rational product_point =
    convergent_within<Id, resolution_of<X>() * pow2(-32) / magnitude_of<X>()>();
template <constant_id Id, insidable X>
inline constexpr grid_rational quotient_point =
    convergent_within<Id, resolution_of<X>() * pow2(-32) / magnitude_of<X>(), true>();
// c / x: the constant within 2^-64.
template <constant_id Id>
inline constexpr grid_rational dividend_point = convergent_within<Id, pow2(-64)>();

// A lower bound on |v − c| over X's values v, from c within 2^(1−B): the
// distance to the lattice points around c; to a point; on a continuous grid
// of denominators up to D, 1/(2D²) (a closer fraction is a convergent of c,
// Legendre) or the distance to a convergent of denominator up to D. A
// bracket that does not separate doubles B.
template <constant_id Id, insidable X, int B = 128>
consteval grid_rational separation() {
    constexpr grid_rational V{big_int{constant_q<Id, B>}};
    constexpr grid_rational lo = (V - grid_rational{1}) * pow2(-B), hi = (V + grid_rational{1}) * pow2(-B);
    constexpr bool          point = lower_of<X> == upper_of<X>;
    if constexpr (point) {
        constexpr grid_rational v = lower_of<X>;
        if constexpr (v < lo)
            return lo - v;
        else if constexpr (hi < v)
            return v - hi;
        else
            return separation<Id, X, 2 * B>();
    } else if constexpr (notch_of<X> != 0) {
        constexpr grid_rational n = notch_of<X>, L = lower_of<X>;
        constexpr grid_rational j = grid_round<round_mode::floor>((lo - L) / n);
        if constexpr (j != grid_round<round_mode::floor>((hi - L) / n) || L + j * n == lo)
            return separation<Id, X, 2 * B>();
        else
            return std::min(lo - (L + j * n), L + (j + grid_rational{1}) * n - hi);
    } else {
        constexpr grid_int D = grid_numerator(grid_of<X>.MaxDenominator);
        if constexpr (B < 4 * bit_width_of(D) + 64)
            return separation<Id, X, 4 * bit_width_of(D) + 64>();
        else {
            grid_rational       d   = grid_rational{1} / (grid_rational{D} * grid_rational{D} * grid_rational{2});
            bool                ok  = true;
            grid_int            num = grid_numerator(V), den = big_int{1} << B; // V/2^B: V is an integer
            grid_int            h1{1}, h2{0}, q1{0}, q2{1};
            const grid_rational r = V * pow2(-B);
            while (den != grid_int{0}) {
                const grid_int a = num / den, rem = num - a * den;
                const grid_int h = a * h1 + h2, q = a * q1 + q2;
                if (D < q)
                    break;
                const grid_rational g = abs(r - grid_rational{h, q}) - pow2(-B);
                if (!(g > 0))
                    ok = false;
                else
                    d = std::min(d, g);
                num = den;
                den = rem;
                h2  = h1;
                h1  = h;
                q2  = q1;
                q1  = q;
            }
            return ok ? d : grid_rational{0};
        }
    }
}
template <constant_id Id, insidable X>
inline constexpr grid_rational separating_point = [] {
    constexpr grid_rational d = separation<Id, X>();
    static_assert(d > 0, "beman::inside::math: the constant cannot be told from a value of the grid");
    return convergent_within<Id, d / grid_rational{2}>();
}();
} // namespace constants
} // namespace detail

// A mathematical constant: an exact value at any precision, as its use needs.
template <detail::constant_id Id>
struct constant {
    // The constant correctly rounded onto inside<G, P> (a grid with a
    // rounding mode), computed while compiling.
    template <grid G, policy_flag P>
    [[nodiscard]] constexpr operator inside<G, P>() const {
        return detail::constants::stored<inside<G, P>, Id>;
    }
    // The point within Eps of the constant: its simplest such fraction.
    template <auto Eps>
    static constexpr auto within =
        just<detail::constants::convergent_within<Id, ::beman::inside::detail::grid_rational{Eps}>()>;
};

// x op c: c as the point x's resolution needs (see the top of the file).
template <insidable X, detail::constant_id Id>
[[nodiscard]] constexpr auto operator*(const X& x, constant<Id>) {
    return x * just<detail::constants::product_point<Id, X>>;
}
template <insidable X, detail::constant_id Id>
[[nodiscard]] constexpr auto operator*(constant<Id>, const X& x) {
    return just<detail::constants::product_point<Id, X>> * x;
}
template <insidable X, detail::constant_id Id>
[[nodiscard]] constexpr auto operator+(const X& x, constant<Id>) {
    return x + just<detail::constants::sum_point<Id, X>>;
}
template <insidable X, detail::constant_id Id>
[[nodiscard]] constexpr auto operator+(constant<Id>, const X& x) {
    return just<detail::constants::sum_point<Id, X>> + x;
}
template <insidable X, detail::constant_id Id>
[[nodiscard]] constexpr auto operator-(const X& x, constant<Id>) {
    return x - just<detail::constants::sum_point<Id, X>>;
}
template <insidable X, detail::constant_id Id>
[[nodiscard]] constexpr auto operator-(constant<Id>, const X& x) {
    return just<detail::constants::sum_point<Id, X>> - x;
}
template <insidable X, detail::constant_id Id>
[[nodiscard]] constexpr auto operator/(const X& x, constant<Id>) {
    return x * just<detail::constants::quotient_point<Id, X>>;
}
template <insidable X, detail::constant_id Id>
[[nodiscard]] constexpr auto operator/(constant<Id>, const X& x) {
    return just<detail::constants::dividend_point<Id>> / x;
}
// Exact: no value of X equals the constant, and the point is closer to it
// than any value of X.
template <insidable X, detail::constant_id Id>
[[nodiscard]] constexpr std::strong_ordering operator<=>(const X& x, constant<Id>) {
    return x <=> just<detail::constants::separating_point<Id, X>>;
}
template <insidable X, detail::constant_id Id>
[[nodiscard]] constexpr bool operator==(const X&, constant<Id>) {
    (void)detail::constants::separating_point<Id, X>; // proves it
    return false;
}

inline constexpr constant<detail::constant_id::pi>          pi{};
inline constexpr constant<detail::constant_id::two_pi>      two_pi{};
inline constexpr constant<detail::constant_id::half_pi>     half_pi{};
inline constexpr constant<detail::constant_id::inv_pi>      inv_pi{};
inline constexpr constant<detail::constant_id::inv_sqrt_pi> inv_sqrt_pi{};
inline constexpr constant<detail::constant_id::e>           e{};
inline constexpr constant<detail::constant_id::log2e>       log2e{};
inline constexpr constant<detail::constant_id::log10e>      log10e{};
inline constexpr constant<detail::constant_id::ln2>         ln2{};
inline constexpr constant<detail::constant_id::ln10>        ln10{};
inline constexpr constant<detail::constant_id::sqrt2>       sqrt2{};
inline constexpr constant<detail::constant_id::sqrt3>       sqrt3{};
inline constexpr constant<detail::constant_id::inv_sqrt2>   inv_sqrt2{};
inline constexpr constant<detail::constant_id::inv_sqrt3>   inv_sqrt3{};
inline constexpr constant<detail::constant_id::phi>         phi{};     // golden ratio
inline constexpr constant<detail::constant_id::egamma>      egamma{};  // Euler–Mascheroni γ
inline constexpr constant<detail::constant_id::catalan>     catalan{}; // Catalan's G
inline constexpr constant<detail::constant_id::zeta3>       zeta3{};   // Apéry's ζ(3)
} // namespace beman::inside::math

#endif // BEMAN_INSIDE_MATH_CONSTANTS_HPP
