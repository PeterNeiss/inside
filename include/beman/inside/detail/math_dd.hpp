// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The double-double kernels of the math engine's dd tier (cmath_adaptive.hpp):
// values as an unevaluated sum Hi + Lo of two doubles (about 106 bits), with
// error-free sums and products (std::fma), for outputs finer than the double
// tier decides. Every constant — ln 2, π, the 2^(j/64), 2^(j/4096) and sin(jπ/128)
// tables, the Taylor coefficients — comes at compile time from the integer
// path's exact series (detail/math_adaptive.hpp), never from a generator.
// Results are within about 2^-96 relative (measured); the tier bounds them
// generously and lets the integer path decide whenever the bound does not.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_DETAIL_MATH_DD_HPP
#define BEMAN_INSIDE_DETAIL_MATH_DD_HPP

#include <beman/inside/detail/math_adaptive.hpp>
#include <beman/inside/detail/math_fp.hpp>

#ifndef BEMAN_INSIDE_MATH_NO_FP

    #include <array>
    #include <bit>
    #include <cmath> // std::fma
    #include <concepts>
    #include <cstdint>
    #include <utility>

namespace beman::inside::math::detail::dd {
using namespace ::beman::inside::detail;
namespace ax  = ::beman::inside::math::detail::ax;
namespace fpk = ::beman::inside::math::detail::fp;

using dd = fpk::hilo;

//---------------------------------------------------------------------------
// Error-free transformations and the arithmetic on them (QD-library style).
// Products use std::fma at runtime and Dekker's split at compile time.
//---------------------------------------------------------------------------
using fpk::fast_two_sum;
using fpk::fenced;
using fpk::two_sum;

constexpr dd split(double a) noexcept {
    const double c  = 134217729.0 * a; // 2^27 + 1
    const double hi = c - (c - a);
    return {hi, a - hi};
}

using fpk::rounded_product;

constexpr dd two_prod(double a, double b) noexcept {
    if consteval {
        const double p = a * b;
        const dd     x = split(a), y = split(b);
        return {p, ((x.Hi * y.Hi - p) + x.Hi * y.Lo + x.Lo * y.Hi) + x.Lo * y.Lo};
    } else {
        const double p = rounded_product(a, b);
        return {p, std::fma(a, b, -p)};
    }
}

constexpr dd neg(dd a) noexcept { return {-a.Hi, -a.Lo}; }

constexpr dd add(dd a, dd b) noexcept {
    dd       s = two_sum(a.Hi, b.Hi);
    const dd t = two_sum(a.Lo, b.Lo);
    s          = fast_two_sum(s.Hi, s.Lo + t.Hi);
    return fast_two_sum(s.Hi, s.Lo + t.Lo);
}

constexpr dd add(dd a, double b) noexcept {
    const dd s = two_sum(a.Hi, b);
    return fast_two_sum(s.Hi, s.Lo + a.Lo);
}

constexpr dd sub(dd a, dd b) noexcept { return add(a, neg(b)); }

// a + b for |b| ≤ |a| (or a == 0), as in a Horner step: no cancellation, so
// the high parts need only fast_two_sum.
constexpr dd add_dominant(dd a, dd b) noexcept {
    const dd s = fast_two_sum(a.Hi, b.Hi);
    return fast_two_sum(s.Hi, s.Lo + (a.Lo + b.Lo));
}

constexpr dd add_dominant(dd a, double b) noexcept {
    const dd s = fast_two_sum(a.Hi, b);
    return fast_two_sum(s.Hi, s.Lo + a.Lo);
}

constexpr dd mul(dd a, dd b) noexcept {
    const dd p = two_prod(a.Hi, b.Hi);
    return fast_two_sum(p.Hi, p.Lo + (a.Hi * b.Lo + a.Lo * b.Hi));
}

constexpr dd mul(dd a, double b) noexcept {
    const dd p = two_prod(a.Hi, b);
    return fast_two_sum(p.Hi, p.Lo + a.Lo * b);
}

constexpr dd sqr(dd a) noexcept {
    const dd p = two_prod(a.Hi, a.Hi);
    return fast_two_sum(p.Hi, p.Lo + 2 * a.Hi * a.Lo);
}

// a/b: two quotient digits from one reciprocal; the remainder
// a − q1·b cancels its high part exactly (Sterbenz).
constexpr dd div(dd a, dd b) noexcept {
    const double inv = 1 / b.Hi;
    const double q1  = a.Hi * inv;
    const dd     p   = two_prod(q1, b.Hi);
    const double r   = ((a.Hi - p.Hi) - p.Lo) + (a.Lo - q1 * b.Lo);
    return fast_two_sum(q1, r * inv);
}

constexpr dd ldexp(dd a, int e) noexcept {
    return {::beman::inside::detail::ldexp(a.Hi, e), ::beman::inside::detail::ldexp(a.Lo, e)};
}

//---------------------------------------------------------------------------
// Constants from the integer path's fixed-point values Y·2^-S.
//---------------------------------------------------------------------------
template <std::size_t K>
constexpr dd of_fixed(wide_sint<K> y, int S) noexcept {
    const bool negative = y.negative();
    if (negative)
        y = -y;
    if (y.is_zero())
        return {0, 0};
    const int n = bit_width_of(y);
    if (n <= 53)
        return {::beman::inside::detail::ldexp(static_cast<double>(static_cast<umax>(y)), -S), 0};
    const int          sh   = n - 53;
    const wide_sint<K> top  = y >> sh;
    const wide_sint<K> rest = y - (top << sh);
    const double       hi   = ::beman::inside::detail::ldexp(static_cast<double>(static_cast<umax>(top)), sh - S);
    const double       lo =
        sh > 53
            ? ::beman::inside::detail::ldexp(static_cast<double>(static_cast<umax>(rest >> (sh - 53))), sh - 53 - S)
            : ::beman::inside::detail::ldexp(static_cast<double>(static_cast<umax>(rest)), -S);
    const dd r = fast_two_sum(hi, lo);
    return negative ? neg(r) : r;
}

inline constexpr int kS = 136; // the scale (bits after the point) of the constants
using fixed             = ax::fixed_t<2 * kS + 16>;

template <std::size_t K>
constexpr dd of_fixed_any(const wide_sint<K>& y, int S) noexcept {
    return of_fixed(static_cast<fixed>(y), S);
}

// c·2^-S cut into N parts of B bits each (the last one of 53), so that
// k·part is exact for |k| < 2^(53−B): Cody–Waite reduction constants.
template <std::size_t N>
constexpr std::array<double, N> parts(fixed y, int S, int B) noexcept {
    std::array<double, N> r{};
    int                   n = bit_width_of(y); // the bits still to place
    for (std::size_t i = 0; i < N && !y.is_zero(); ++i) {
        const int   bits = i + 1 < N ? B : 53;
        const int   sh   = n > bits ? n - bits : 0;
        const fixed top  = y >> sh;
        r[i]             = ::beman::inside::detail::ldexp(static_cast<double>(static_cast<umax>(top)), sh - S);
        y                = y - (top << sh);
        n                = sh;
    }
    return r;
}

// 1/n! and 1/n at full dd precision.
template <int S>
constexpr dd inv_fact(int n) noexcept {
    fixed f{1};
    for (int i = 2; i <= n; ++i)
        f = f * fixed{i};
    return of_fixed((fixed{1} << (2 * S)) / f, 2 * S);
}
template <int S>
constexpr dd inv_int(int n) noexcept {
    return of_fixed((fixed{1} << (2 * S)) / fixed{n}, 2 * S);
}

// 2^(j/2^shift), j = 0 … 63: the root 2^(1/2^shift) from the integer exp,
// then its powers at scale S (a unit lost per step).
template <int S>
constexpr std::array<dd, 64> exp2_table(int shift) noexcept {
    const auto         a    = ax::exp_fixed<S>(static_cast<fixed>(ax::ln2_q<S>) >> shift, 1, 4);
    const fixed        root = a.Scale == S ? a.Value : a.Value << (S - a.Scale);
    std::array<dd, 64> t{};
    fixed              p = fixed{1} << S;
    for (std::size_t j = 0; j < 64; ++j) {
        t[j] = of_fixed(p, S);
        p    = (p * root) >> S;
    }
    return t;
}

// sin(jπ/128), j = 0 … 255: cos and sin of π/128 by five half-angle square
// roots from cos π/4, then j rotations at scale kS (a unit lost per step).
template <int S>
constexpr std::array<dd, 256> sin_table() noexcept {
    const fixed one  = fixed{1} << S;
    auto        root = [](fixed v) { return ax::isqrt(v << S); }; // √(v·2^-S) at scale S
    fixed       c    = root(one >> 1);                            // cos π/4
    for (int i = 0; i < 4; ++i)
        c = root((one + c) >> 1);                                             // cos π/64
    const fixed         s1 = root((one - c) >> 1), c1 = root((one + c) >> 1); // π/128
    std::array<dd, 256> t{};
    fixed               sj{0}, cj = one;
    for (int j = 0; j <= 64; ++j) {
        const dd v                           = of_fixed(sj, S);
        t[static_cast<std::size_t>(j)]       = v;
        t[static_cast<std::size_t>(128 - j)] = v;
        t[static_cast<std::size_t>(128 + j)] = neg(v);
        if (j > 0)
            t[static_cast<std::size_t>(256 - j)] = neg(v);
        const fixed sn = (sj * c1 + cj * s1) >> S;
        cj             = (cj * c1 - sj * s1) >> S;
        sj             = sn;
    }
    t[64]  = dd{1, 0};
    t[192] = dd{-1, 0};
    return t;
}

// atan(j/32), j = 0 … 32, from the integer atan: one constant expression
// per entry, so each has the compiler's whole evaluation budget.
template <int S, int J>
inline constexpr dd atan_entry = of_fixed(ax::atan_fixed<S>(fixed{J} << (S - 5), 0).Value, S);

template <int S>
constexpr std::array<dd, 33> atan_table() noexcept {
    return []<int... J>(std::integer_sequence<int, J...>) {
        return std::array<dd, 33>{atan_entry<S, J>...};
    }(std::make_integer_sequence<int, 33>{});
}

// ln(j/128), j = 96 … 192: sums of ln(i/(i − 1)) = 2·atanh(1/(2i − 1))
// outward from j = 128, each a short series at scale S + 16 (a unit lost
// per term).
template <int S>
constexpr std::array<dd, 97> log_table() noexcept {
    constexpr int W    = S + 16;
    auto          step = [](int i) {
        const fixed d{2 * i - 1}, d2 = d * d;
        fixed       t = (fixed{1} << W) / d, sum{0};
        for (int k = 1; !t.is_zero(); k += 2) {
            sum = sum + t / fixed{k};
            t   = t / d2;
        }
        return sum + sum;
    };
    std::array<dd, 97> r{};
    fixed              acc{0};
    for (int j = 129; j <= 192; ++j)
        r[static_cast<std::size_t>(j - 96)] = of_fixed(acc = acc + step(j), W);
    acc = fixed{0};
    for (int j = 127; j >= 96; --j)
        r[static_cast<std::size_t>(j - 96)] = of_fixed(acc = acc - step(j + 1), W);
    return r;
}

// Every constant and table, as members of a class template: computed on
// first use only, so a translation unit that never reaches the dd tier
// pays nothing for them (the scale S is a parameter so that GCC cannot
// fold the initializers early). D is always dd.
template <typename D, int S = kS>
struct consts {
    static constexpr D Ln2    = of_fixed_any(ax::ln2_q<S>, S);
    static constexpr D Log2e  = of_fixed_any(ax::log2e_q<S>, S);
    static constexpr D Log10e = of_fixed_any(ax::log10e_q<S>, S);
    static constexpr D Pi     = of_fixed_any(ax::pi_q<S>, S);
    static constexpr D HalfPi = ldexp(Pi, -1);
    // ln 2/4096 in parts of 31 bits (|k| < 2^22: |x| ≤ 700 · 4096/ln 2), and
    // π/128 in parts of 27 bits (|n| < 2^26: |x| ≤ 2^20 · 128/π).
    static constexpr std::array<double, 3> Ln2By4096  = parts<3>(static_cast<fixed>(ax::ln2_q<S>), S + 12, 31);
    static constexpr std::array<double, 4> PiBy128    = parts<4>(static_cast<fixed>(ax::pi_q<S>), S + 7, 27);
    static constexpr std::array<D, 64>     Exp2By64   = exp2_table<S>(6);
    static constexpr std::array<D, 64>     Exp2By4096 = exp2_table<S>(12);
    static constexpr std::array<D, 256>    Sin        = sin_table<S>();
    static constexpr std::array<D, 33>     Atan       = atan_table<S>();
    static constexpr std::array<D, 97>     Log        = log_table<S>();
    static constexpr D F2 = inv_fact<S>(2), F3 = inv_fact<S>(3), F4 = inv_fact<S>(4), F5 = inv_fact<S>(5);
    static constexpr D F6 = inv_fact<S>(6), F7 = inv_fact<S>(7);
    static constexpr D I3 = inv_int<S>(3), I5 = inv_int<S>(5), I7 = inv_int<S>(7);
};

inline constexpr double kInvLn2By4096 = 4096 * fpk::kLog2e;
inline constexpr double kInvPiBy128   = 128 / 0x1.921fb54442d18p+1;

// a·2^m for |m| ≤ 1022 (a power-of-two factor, exact unless the result is
// subnormal).
inline dd scale(dd a, long m) noexcept {
    const double f = std::bit_cast<double>(static_cast<std::uint64_t>(m + 1023) << 52);
    return {a.Hi * f, a.Lo * f};
}

// x − k·(c0 + c1 + c2 + c3), the parts from parts<>: x.Hi − k·c0 is exact
// (k·c0 fits 53 bits and lies within a factor 2 of x.Hi), k·c1 and k·c2
// are exact and summed beside it, and k·c3 is far below the result's
// last bit — so the chain is two sums long.
inline dd reduce(dd x, double k, double c0, double c1, double c2, double c3) noexcept {
    const double a = std::fma(-k, c0, x.Hi);
    const dd     w = fast_two_sum(k * c1, k * c2);
    const dd     d = two_sum(a, -w.Hi);
    return fast_two_sum(d.Hi, d.Lo + ((x.Lo - w.Lo) - k * c3));
}

//---------------------------------------------------------------------------
// Kernels. Each is within about 2^-96 of its value (relative, plus an
// absolute 2^-99 where a subtraction cancels; dd_kernels_stay_far_inside_
// their_bound in math_adaptive.test.cpp checks it). Horner steps add a
// small product to a larger coefficient, so they use add_dominant. They are
// templates on D = dd only so that consts<D> is instantiated on first use.
//---------------------------------------------------------------------------
template <typename D>
concept dd_type = std::same_as<D, dd>;

// e^x for |x| ≤ 700: x = k·ln 2/4096 + r, |r| ≤ ln 2/8192, k = 4096m + 64i + j,
// e^x = 2^m · 2^(i/64) · 2^(j/4096) · (1 + p(r)). The terms of p from r^4
// on are below 2^-53 relative and summed in double.
template <dd_type D>
struct exp_reduced {
    D    R; // the reduced argument
    D    T; // 2^(i/64)·2^(j/4096)
    long M; // the power of 2
};

template <dd_type D>
[[gnu::always_inline]] inline exp_reduced<D> exp_reduce(D x) noexcept {
    using C         = consts<D>;
    const double k  = __builtin_nearbyint(x.Hi * kInvLn2By4096);
    const long   ik = static_cast<long>(k);
    return {
        reduce(x, k, C::Ln2By4096[0], C::Ln2By4096[1], C::Ln2By4096[2], 0),
        mul(C::Exp2By64[static_cast<std::size_t>((ik >> 6) & 63)], C::Exp2By4096[static_cast<std::size_t>(ik & 63)]),
        ik >> 12};
}

template <dd_type D>
inline D exp(D x) noexcept {
    using C              = consts<D>;
    const auto [r, t, m] = exp_reduce(x);
    const double rh      = r.Hi;
    const double tail    = fpk::horner(rh, 1.0 / 5040, 1.0 / 720, 1.0 / 120, 1.0 / 24);
    D            p       = add_dominant(C::F3, rh * tail);  // 1/3! + r/4! + …
    p                    = add_dominant(C::F2, mul(r, p));  // 1/2! + r/3! + …
    p                    = add_dominant(r, mul(sqr(r), p)); // e^r − 1
    return scale(add_dominant(t, mul(t, p)), m);
}

// ln x for normal x > 0: x = 2^m·f, f in [0.75, 1.5), c = j/128 the
// table point nearest f, ln x = m·ln 2 + ln c + 2·atanh s with
// s = (f − c)/(f + c), |s| ≤ 1/384. atanh s = s·Σ s^2k/(2k+1); the terms
// from s^7 on are below 2^-53 relative and summed in double, those from
// s^13 on below 2^-106.
template <dd_type D>
struct log_reduced {
    long M; // the power of 2
    D    S; // (f − c)/(f + c)
    D    L; // ln c
};

template <dd_type D>
[[gnu::always_inline]] inline log_reduced<D> log_reduce(D x) noexcept {
    long m = static_cast<long>(std::bit_cast<std::uint64_t>(x.Hi) >> 52) - 1023;
    D    f = scale(x, -m); // [1, 2)
    if (f.Hi >= 1.5) {
        f = {f.Hi * 0.5, f.Lo * 0.5};
        ++m;
    }
    const double j = __builtin_nearbyint(f.Hi * 128);
    const double c = j * 0x1p-7;
    return {m,
            div(two_sum(f.Hi - c, f.Lo), add(f, c)), // f.Hi − c is exact
            consts<D>::Log[static_cast<std::size_t>(j) - 96]};
}

template <dd_type D>
inline D log(D x) noexcept {
    using C               = consts<D>;
    const auto [m, s, lc] = log_reduce(x);
    const D      z        = sqr(s);
    const double zh       = z.Hi;
    D            p        = add_dominant(C::I5, zh * fpk::horner(zh, 1.0 / 11, 1.0 / 9, 1.0 / 7));
    p                     = add_dominant(C::I3, mul(z, p));     // 1/3 + z/5 + …
    p                     = add_dominant(s, mul(mul(s, z), p)); // atanh s
    const D t             = add(mul(C::Ln2, static_cast<double>(m)), lc);
    return add(t, D{2 * p.Hi, 2 * p.Lo});
}

struct sincos_t {
    dd Sin, Cos;
};

// sin and cos for |x| ≤ 2^20: x = n·π/128 + r, |r| ≤ π/256, then
// sin(a + r) = sin a + (sin a·(cos r − 1) + cos a·sin r) and the like with
// the table. The Taylor terms from r^9 (sin) and r^8 (cos) on are below
// 2^-53 relative and summed in double.
template <dd_type D>
struct trig_reduced {
    D R;      // the reduced argument
    D Sa, Ca; // sin and cos of nπ/128
};

template <dd_type D>
[[gnu::always_inline]] inline trig_reduced<D> trig_reduce(D x) noexcept {
    using C        = consts<D>;
    const double n = __builtin_nearbyint(x.Hi * kInvPiBy128);
    const long   j = static_cast<long>(n);
    return {reduce(x, n, C::PiBy128[0], C::PiBy128[1], C::PiBy128[2], C::PiBy128[3]),
            C::Sin[static_cast<std::size_t>(j & 255)],
            C::Sin[static_cast<std::size_t>((j + 64) & 255)]};
}

template <dd_type D>
[[gnu::always_inline]] inline sincos_t sincos(D x) noexcept {
    using C                = consts<D>;
    const auto [r, sa, ca] = trig_reduce(x);
    const D      z         = sqr(r);
    const double zh        = z.Hi;
    const double st        = fpk::horner(zh, 1.0 / 6227020800.0, -1.0 / 39916800.0, 1.0 / 362880.0);
    const double ct        = fpk::horner(zh, 1.0 / 479001600.0, -1.0 / 3628800.0, 1.0 / 40320.0);
    D            s         = add_dominant(neg(C::F7), zh * st); // −1/7! + z/9! − …
    s                      = add_dominant(C::F5, mul(z, s));
    s                      = add_dominant(neg(C::F3), mul(z, s));
    const D sr             = add_dominant(r, mul(mul(r, z), s)); // sin r
    D       c              = add_dominant(neg(C::F6), zh * ct);
    c                      = add_dominant(C::F4, mul(z, c));
    c                      = add_dominant(neg(C::F2), mul(z, c));
    const D cm             = mul(z, c); // cos r − 1
    return {add_dominant(sa, add(mul(sa, cm), mul(ca, sr))), add_dominant(ca, sub(mul(ca, cm), mul(sa, sr)))};
}

template <dd_type D>
inline D sin(D x) noexcept {
    return sincos(x).Sin;
}
template <dd_type D>
inline D cos(D x) noexcept {
    return sincos(x).Cos;
}

// atan a for 0 ≤ a ≤ 1: c = j/32 nearest a, atan a = atan c + atan t with
// t = (a − c)/(1 + a·c), |t| ≤ 1/64. The series terms from t^9 on (2^-51
// relative at most) are summed in double, within 2^-104.
template <dd_type D>
inline D atan_unit(D a) noexcept {
    using C           = consts<D>;
    const double j    = __builtin_nearbyint(a.Hi * 32);
    const double c    = j * (1.0 / 32);
    const D      t    = div(add(a, -c), add_dominant(D{1, 0}, mul(a, c)));
    const D      z    = sqr(t);
    const double zh   = z.Hi;
    const double tail = fpk::horner(zh, -1.0 / 19, 1.0 / 17, -1.0 / 15, 1.0 / 13, -1.0 / 11, 1.0 / 9);
    D            p    = add_dominant(neg(C::I7), zh * tail); // −1/7 + z/9 − …
    p                 = add_dominant(C::I5, mul(z, p));
    p                 = add_dominant(neg(C::I3), mul(z, p));
    const D r         = add_dominant(t, mul(mul(t, z), p)); // atan t
    return add_dominant(C::Atan[static_cast<std::size_t>(j)], r);
}

// atan2(y, x): the ratio of the smaller to the larger magnitude, then the
// octant.
template <dd_type D>
inline D atan2(D y, D x) noexcept {
    using C         = consts<D>;
    const double ay = y.Hi < 0 ? -y.Hi : y.Hi, ax_ = x.Hi < 0 ? -x.Hi : x.Hi;
    if (ay == 0 && ax_ == 0)
        return D{0, 0};
    if (ay <= ax_) {
        const D q = div(y, x);
        const D r = q.Hi < 0 ? neg(atan_unit(neg(q))) : atan_unit(q);
        if (x.Hi > 0)
            return r;
        return y.Hi < 0 ? sub(r, C::Pi) : add(r, C::Pi);
    }
    const D q = div(x, y); // |q| < 1
    const D r = q.Hi < 0 ? neg(atan_unit(neg(q))) : atan_unit(q);
    return y.Hi > 0 ? sub(C::HalfPi, r) : sub(neg(C::HalfPi), r);
}

template <dd_type D>
inline D atan(D x) noexcept {
    const bool negative = x.Hi < 0;
    const D    a        = negative ? neg(x) : x;
    const D    r        = a.Hi <= 1 ? atan_unit(a) : sub(consts<D>::HalfPi, atan_unit(div(D{1, 0}, a)));
    return negative ? neg(r) : r;
}

// √x: one Newton step from the correctly rounded double root.
template <dd_type D>
inline D sqrt(D x) noexcept {
    if (!(x.Hi > 0))
        return D{0, 0};
    const double y = fpk::fp_sqrt(x.Hi);
    const D      e = sub(x, two_prod(y, y));
    return fast_two_sum(y, e.Hi / (2 * y));
}

// ∛x: one Newton step from the double root.
template <dd_type D>
inline D cbrt(D x) noexcept {
    if (x.Hi == 0)
        return D{0, 0};
    const double y = fpk::seed_cbrt(x.Hi);
    const D      e = sub(mul(two_prod(y, y), y), x); // y³ − x
    return fast_two_sum(y, -e.Hi / (3 * y * y));
}

template <dd_type D>
inline D tan(D x) noexcept {
    const sincos_t sc = sincos(x);
    return div(sc.Sin, sc.Cos);
}

template <dd_type D>
inline D exp2(D x) noexcept {
    return exp(mul(x, consts<D>::Ln2));
}
template <dd_type D>
inline D log2(D x) noexcept {
    return mul(log(x), consts<D>::Log2e);
}
template <dd_type D>
inline D log10(D x) noexcept {
    return mul(log(x), consts<D>::Log10e);
}
template <dd_type D>
inline D sinh(D x) noexcept {
    const D e = exp(x);
    return ldexp(sub(e, div(D{1, 0}, e)), -1);
}
template <dd_type D>
inline D cosh(D x) noexcept {
    const D e = exp(x);
    return ldexp(add(e, div(D{1, 0}, e)), -1);
}
template <dd_type D>
inline D tanh(D x) noexcept {
    const D e = exp(ldexp(x, 1));
    return div(add(e, -1.0), add(e, 1.0));
}

// The inverse hyperbolics on |x|, as in the double tier.
template <dd_type D>
inline D asinh(D x) noexcept {
    const bool negative = x.Hi < 0;
    const D    a        = negative ? neg(x) : x;
    const D    m        = log(add(a, sqrt(add(sqr(a), 1.0))));
    return negative ? neg(m) : m;
}
template <dd_type D>
inline D acosh(D x) noexcept {
    return log(add(x, sqrt(mul(add(x, -1.0), add(x, 1.0)))));
}
template <dd_type D>
inline D atanh(D x) noexcept {
    const bool negative = x.Hi < 0;
    const D    a        = negative ? neg(x) : x;
    const D    m        = ldexp(log(div(add(a, 1.0), add(neg(a), 1.0))), -1);
    return negative ? neg(m) : m;
}
template <dd_type D>
inline D asin(D x) noexcept {
    return atan2(x, sqrt(mul(add(neg(x), 1.0), add(x, 1.0))));
}
template <dd_type D>
inline D acos(D x) noexcept {
    return atan2(sqrt(mul(add(neg(x), 1.0), add(x, 1.0))), x);
}
template <dd_type D>
inline D hypot(D x, D y) noexcept {
    return sqrt(add(sqr(x), sqr(y)));
}

//---------------------------------------------------------------------------
// The lean kernels: the dd tier's first attempt for sin, cos, exp and exp2.
// The tier serves outputs whose value indices stay below 2^62, which about
// 2^-70 decides nearly always, so these carry in double-double only the
// terms that need it — the table values, the reduced argument and its
// leading products, which are error-free — and the rest in double. Their
// bounds are proved the way the double tier's are (detail/math_fp.hpp):
// every rounding counted at its worst, up() for the second-order terms.
// Results they leave undecided go to the full kernels above.
//---------------------------------------------------------------------------
using fpk::kU;
using fpk::up;

// A table entry or constant (of_fixed: the top 53 bits and the next 53,
// both cut, from a value within 2^-120 of exact) is within 2^-104 of its
// value, relatively.
inline constexpr double kTableErr = 0x1p-104;

// reduce()'s error for |x.Lo| ≤ kU·|x.Hi|, |k| ≤ Kx·max(1, |x|), the second
// part below 2^E1 and a residual of the parts below 2^-Res of their sum: in
// units of max(1, |x|), the roundings of x.Lo − w.Lo (|w.Lo| ≤ kU·|k|·2^E1),
// k·c3 (below |k|·2^E3), their difference and the sum with d.Lo
// (|d.Lo| ≤ kU·R): three of kU on the terms, kU² of R, and |k|·Res.
consteval double reduce_abs(double Kx, int E1, int E3, int Res, double R) {
    const double t = kU * (1 + kU) + kU * Kx * fpk::pow_n(0.5, -E1) + Kx * fpk::pow_n(0.5, -E3);
    return up(3 * kU * t * (1 + 4 * kU) + Kx * fpk::pow_n(0.5, Res) + kU * kU * R);
}

// sin r = r + r³·S(r²), S = −1/3! + z/5! − z²/7!; cos r = 1 − r²/2 + r⁴·C(r²),
// C = 1/4! − z/6! + z²/8!.
inline constexpr fpk::poly<3> kLeanSinC =
    fpk::series<3>([](int k) { return fpk::term{k % 2 ? 1.0 : -1.0, fpk::factorial(2 * k + 3)}; });
inline constexpr fpk::poly<3> kLeanCosC =
    fpk::series<3>([](int k) { return fpk::term{k % 2 ? -1.0 : 1.0, fpk::factorial(2 * k + 4)}; });

// sin and cos for |x| ≤ 2^20, x = n·π/128 + r as in sincos. n's product
// rounds within 2^-26.6 of a step, and |x.Lo| ≤ 2^-33, so |r| ≤ R. With
// r = rh + rl (|rl| ≤ kU·|rh|), z = rh² exactly as zh + zl, and the table's
// A = sin(nπ/128), B = cos(nπ/128):
//   sin(a + r) = A + B·sr + A·cm,  sr = sin r = rh + ts,
//   cos(a + r) = B − A·sr + B·cm,  cm = cos r − 1 = −zh/2 + tc,
// ts = rh·zh·S + rl and tc = zh²·C − zl/2 − rh·rl in double. The products
// B·rh and A·(−zh/2) are error-free and join A in two exact fast two-sums
// (|A| ≥ sin(π/128) > 2|B·rh| unless A = 0); everything else is summed in
// double. All bounds are absolute (|A|, |B| ≤ 1):
// - sr: rl·(cos − 1) for rl·cos (kU·R³/2); rh·zh's two roundings, S's
//   Horner error at zh (horner_error with Dz = kU), its truncation; ts's
//   fma (kU·|ts|);
// - cm: rl·(rh − sin rh) and rl² (kU·R⁴/6, kU²·Z); zh²'s three roundings,
//   C's Horner error and truncation; the fmas of tc and of rh·rl + zl/2;
// - the sum of the low parts: at most 10 roundings of kU, each partial sum
//   within Lmax; the dropped B.Lo·ts and A.Lo·tc; the table's 2^-104 of A
//   and B times (1 + R).
// The reduction's error (reduce_abs: |n| ≤ 42·max(1, |x|), c1 < 2^-31,
// c3 < 2^-86, the parts within 2^-139 of π/128) moves either value by as
// much, times 1 + R: AbsX per max(1, |x|).
struct lean_trig {
    static constexpr double            R = 0.01228; // ≥ (½ + 2^-26)·π/128 + 2^-33
    static constexpr double            Z = R * R, R3 = Z * R, R4 = Z * Z;
    static constexpr fpk::horner_bound HS = fpk::horner_error(kLeanSinC, Z, kU);
    static constexpr fpk::horner_bound HC = fpk::horner_error(kLeanCosC, Z, kU);
    static constexpr double            TS = R3 * HS.Mag * (1 + 4 * kU) + kU * R * (1 + kU);       // ≥ |ts|
    static constexpr double            TC = R4 * HC.Mag * (1 + 4 * kU) + 1.5 * kU * Z * (1 + kU); // ≥ |tc|
    static constexpr double            SinR =
        kU * R3 / 2 + R3 * (2 * kU * HS.Mag + HS.Err + fpk::pow_n(Z, 3) / fpk::factorial(9)) + kU * TS;
    static constexpr double CosR = kU * R4 / 6 + 2 * kU * kU * Z +
                                   R4 * (3 * kU * HC.Mag + HC.Err + fpk::pow_n(Z, 3) / fpk::factorial(10)) + kU * TC;
    static constexpr double Lmax = 2 * kU * (1 + R + Z) + kU * R + kU * Z / 2 + kU + TS + kU * R + TC + kU * Z / 2;
    static constexpr double Abs  = up(SinR + CosR + 10 * kU * Lmax + kU * (TS + TC) + kTableErr * (1 + R));
    static constexpr double AbsX = up(reduce_abs(42, -31, -86, 139, R) * (1 + R));
};

// A + B·(rh + ts) + A·(h + tc), h = −zh/2, as above.
template <dd_type D>
[[gnu::always_inline]] inline D lean_rotate(D a, D b, double rh, double ts, double h, double tc) noexcept {
    const D p  = two_prod(b.Hi, rh);
    const D q  = two_prod(a.Hi, h);
    const D s1 = fast_two_sum(a.Hi, p.Hi);
    const D s2 = fast_two_sum(s1.Hi, q.Hi);
    double  lo = std::fma(a.Lo, h, std::fma(b.Lo, rh, a.Lo + (p.Lo + q.Lo)));
    lo         = std::fma(a.Hi, tc, lo);
    lo         = std::fma(b.Hi, ts, lo);
    return fast_two_sum(s2.Hi, (s1.Lo + s2.Lo) + lo);
}

template <dd_type D>
[[gnu::always_inline]] inline sincos_t sincos_lean(D x) noexcept {
    const auto [r, sa, ca] = trig_reduce(x);
    const double rh = r.Hi, rl = r.Lo;
    const D      z  = two_prod(rh, rh);
    const double zh = z.Hi;
    const double ts = std::fma(fpk::rounded_product(rh, zh), fpk::horner(zh, kLeanSinC.C), rl);
    const double tc =
        std::fma(fpk::rounded_product(zh, zh), fpk::horner(zh, kLeanCosC.C), -std::fma(rh, rl, 0.5 * z.Lo));
    const double h = -0.5 * zh;
    return {lean_rotate(sa, ca, rh, ts, h, tc), lean_rotate(ca, neg(sa), rh, ts, h, tc)};
}

// The bound of a lean sin or cos at x.
constexpr double lean_trig_bound(double x) noexcept {
    const double ax = x < 0 ? -x : x;
    return lean_trig::Abs + lean_trig::AbsX * (ax > 1 ? ax : 1.0) + fpk::kTiny;
}

template <dd_type D>
inline D sin_lean(D x, double& bound) noexcept {
    bound = lean_trig_bound(x.Hi);
    return sincos_lean(x).Sin;
}
template <dd_type D>
inline D cos_lean(D x, double& bound) noexcept {
    bound = lean_trig_bound(x.Hi);
    return sincos_lean(x).Cos;
}

// e^r − 1 = r + r²·P(r), P = 1/2! + r/3! + r²/4! + r³/5!.
inline constexpr fpk::poly<4> kLeanExpC = fpk::series<4>([](int k) { return fpk::term{1.0, fpk::factorial(k + 2)}; });

// e^x for |x| ≤ 700, x = k·ln 2/4096 + r as in exp: n's product rounds
// within 2^-29 of a step and |x.Lo| ≤ 2^-43, so |r| ≤ R. With
// r = rh + rl and T = 2^(i/64)·2^(j/4096) (a dd product, within
// 2·2^-104 + 6kU² of its value), e^x = 2^m·(T + T·(rh + tp)),
// tp = rh²·P(rh) + rl in double; T.Hi·rh is error-free and joins T.Hi in
// an exact fast two-sum. Relative to T:
// - rh + tp: rl·(e^rh·(e^rl − 1)/rl − 1) (kU·R²·(1 + R)); rh²'s rounding,
//   P's Horner error (at rh, exact) and truncation, tp's fma;
// - the low parts: 5 roundings within Lmax, the dropped T.Lo·tp, T's error
//   times 1 + R + TP;
// divided by the least e^r/T = 1 − R − TP, plus the reduction's error
// (reduce_abs: |k| ≤ 5910·max(1, |x|), c1 < 2^-42, no c3, the parts within
// 2^-127 of ln 2/4096) at |x| = 700, which moves e^x by as much,
// relatively.
struct lean_exp {
    static constexpr double            R  = 8.462e-5; // ≥ (½ + 2^-29)·ln 2/4096 + 2^-43
    static constexpr fpk::horner_bound HP = fpk::horner_error(kLeanExpC, R, 0);
    static constexpr double            TP = R * R * HP.Mag * (1 + 3 * kU) + kU * R * (1 + kU); // ≥ |tp|
    static constexpr double            ER =
        kU * R * R * (1 + R) + R * R * (kU * HP.Mag + HP.Err + fpk::pow_n(R, 4) / 720 * 1.0001) + kU * TP;
    static constexpr double Et   = up(2 * kTableErr + 6 * kU * kU);
    static constexpr double Lmax = kU * (1 + R) + kU * R + kU + TP + kU * R;
    static constexpr double Rel  = up((ER + Et * (1 + R + TP) + 5 * kU * Lmax + kU * TP) / (1 - R - TP) +
                                      700 * reduce_abs(5910, -42, -1074, 127, R));
    // exp2: y = x·ln 2 (|x| ≤ 1000, so |y| ≤ 694) as a dd product, within
    // 2^-104 + 6kU² of itself; it moves e^y by |y| times that.
    static constexpr double Rel2 = up(Rel + 694 * (kTableErr + 6 * kU * kU));
};

template <dd_type D>
[[gnu::always_inline]] inline D exp_lean_value(D x) noexcept {
    const auto [r, t, m] = exp_reduce(x);
    const double rh      = r.Hi;
    const double tp      = std::fma(fpk::rounded_product(rh, rh), fpk::horner(rh, kLeanExpC.C), r.Lo);
    const D      p       = two_prod(t.Hi, rh);
    const D      s       = fast_two_sum(t.Hi, p.Hi);
    const double lo      = std::fma(t.Hi, tp, std::fma(t.Lo, rh, s.Lo + (p.Lo + t.Lo)));
    return scale(fast_two_sum(s.Hi, lo), m);
}

template <dd_type D>
inline D exp_lean(D x, double& bound) noexcept {
    const D v = exp_lean_value(x);
    bound     = lean_exp::Rel * v.Hi + fpk::kTiny;
    return v;
}
template <dd_type D>
inline D exp2_lean(D x, double& bound) noexcept {
    const D v = exp_lean_value(mul(x, consts<D>::Ln2));
    bound     = lean_exp::Rel2 * v.Hi + fpk::kTiny;
    return v;
}
// 2·atanh s = 2s + 2s·z·P(z), P = 1/3 + z/5 + z²/7.
inline constexpr fpk::poly<3> kLeanLogC = fpk::series<3>([](int k) { return fpk::term{1.0, 2.0 * k + 3}; });

// ln x for normal x > 0, reduced as in log: ln x = m·ln 2 + ln c + 2·atanh s,
// |s| ≤ S since |f − c| ≤ 1/256 and f + c ≥ 2·0.75 − 1/256. s is a dd
// quotient: its numerator is exact, the denominator one dd sum (2kU² of
// it), and the division within about 15kU², so s is within 32kU²·|s|.
// m·Ln2.Hi, ln c's high part and 2s join in two exact two-sums; the tail
// t = 2·sh·zh·P(zh) (sh = s.Hi, zh = sh²) and the low parts are summed in
// double. All bounds absolute, |ln c| ≤ B = ln 1.5:
// - t: zh, sh·zh and the product with P round (3kU of |t|); P's Horner
//   error at zh; the truncation (2S⁹/9, widened 1%); and t taken at sh, not
//   s (|s.Lo| ≤ kU·S moves it by 2S²·kU·S);
// - 2s: 64kU²·S;
// - the low sum: 7 roundings within Lsum = 5kU·(B + 2S) + |t|, plus per
//   unit of |m|·ln 2 the same 5kU in Lsum (35kU²), the product
//   m·Ln2.Lo's rounding (kU²) and Ln2's 2^-104 (PerM);
// - ln c's table entry: 2^-104 of B.
struct lean_log {
    static constexpr double            S  = 0.002612; // ≥ (1/256)/1.4961
    static constexpr double            Z  = S * S;
    static constexpr double            B  = 0.4055; // ≥ ln 1.5
    static constexpr fpk::horner_bound HP = fpk::horner_error(kLeanLogC, Z, kU);
    static constexpr double            TT = 2 * S * Z * HP.Mag * (1 + 4 * kU); // ≥ |t|
    static constexpr double            ET =
        3 * kU * TT + 2 * S * Z * HP.Err + 2 * fpk::pow_n(S, 9) / 9 * 1.01 + 2 * Z * kU * S * 1.0001;
    static constexpr double Lsum = 5 * kU * (B + 2 * S) + TT;
    static constexpr double Abs  = up(ET + 64 * kU * kU * S + 7 * kU * Lsum + kTableErr * B);
    static constexpr double PerM = up(0.6932 * (35 * kU * kU + kU * kU + kTableErr));
};

template <dd_type D>
[[gnu::always_inline]] inline D log_lean_value(D x, double& bound) noexcept {
    const auto [m, s, lc] = log_reduce(x);
    const double sh = s.Hi, zh = sh * sh;
    const double t  = 2 * (fpk::rounded_product(sh, zh) * fpk::horner(zh, kLeanLogC.C));
    const double md = static_cast<double>(m);
    const D      a  = two_prod(md, consts<D>::Ln2.Hi);
    const D      s1 = two_sum(a.Hi, lc.Hi);
    const D      s2 = two_sum(s1.Hi, 2 * s.Hi);
    const double lo = (((s1.Lo + s2.Lo) + (a.Lo + md * consts<D>::Ln2.Lo)) + (lc.Lo + 2 * s.Lo)) + t;
    bound           = lean_log::Abs + lean_log::PerM * (md < 0 ? -md : md);
    return fast_two_sum(s2.Hi, lo);
}

template <dd_type D>
inline D log_lean(D x, double& bound) noexcept {
    return log_lean_value(x, bound);
}

// log2 and log10: ln x times the constant (within 2^-104) as a dd product
// (within 4kU²), so the bound scales by the constant (≤ 1.4427) and adds
// 2^-100 of the result.
template <dd_type D>
inline D log2_lean(D x, double& bound) noexcept {
    const D v = mul(log_lean_value(x, bound), consts<D>::Log2e);
    bound     = bound * 1.4427 + 0x1p-100 * (v.Hi < 0 ? -v.Hi : v.Hi);
    return v;
}
template <dd_type D>
inline D log10_lean(D x, double& bound) noexcept {
    const D v = mul(log_lean_value(x, bound), consts<D>::Log10e);
    bound     = bound * 0.4343 + 0x1p-100 * (v.Hi < 0 ? -v.Hi : v.Hi);
    return v;
}
//---------------------------------------------------------------------------
// Lean kernels built on the ones above. Error terms of the dd operations,
// relative to their exact results: a sum (add, sub, add_dominant) rounds
// within 4kU² of |a| + |b|; a product (mul, sqr) within 4kU²; a quotient
// (div: one reciprocal, a residual that cancels exactly, two digits) within
// 32kU²; sqrt (one Newton step from the correctly rounded root) within 7kU²
// plus half its argument's. x ± 1 is exact for 0.5 ≤ |x.Hi| ≤ 2 (Sterbenz:
// the low part then adds to an exact zero), and within 2kU²·(1 + |x|) of
// itself otherwise.
//---------------------------------------------------------------------------
inline constexpr double kU2 = kU * kU;

// tan = sin/cos from the lean sincos, each within e: |s/c − S/C| ≤
// e·(1 + |t|)/(|c| − e), plus the quotient's 32kU². No bound (infinity)
// where |c| ≤ e: the full kernel decides there.
template <dd_type D>
inline D tan_lean(D x, double& bound) noexcept {
    const sincos_t sc = sincos_lean(x);
    const double   e  = lean_trig_bound(x.Hi);
    const D        t  = div(sc.Sin, sc.Cos);
    const double   at = t.Hi < 0 ? -t.Hi : t.Hi, c = (sc.Cos.Hi < 0 ? -sc.Cos.Hi : sc.Cos.Hi) - 2 * e;
    bound = c > 0 ? e * (1 + at) / c * (1 + 0x1p-40) + 32 * kU2 * at : __builtin_inf();
    return t;
}

// sinh, cosh = (e ± 1/e)/2 with e = e^x within Rel: 1/e within Rel + 32kU²,
// the sum's 4kU² of e + 1/e; all within (Rel + 20kU²)·cosh x, and
// cosh ≤ |sinh| + 1. tanh = (e2 − 1)/(e2 + 1), e2 = e^(2x): e2's Rel moves it
// by 2·e2·Rel/(e2 + 1)² ≤ Rel/2; the two sums with 1 round within
// 2kU²·(e2 + 1), 2kU² of the quotient each; the quotient 32kU².
template <dd_type D>
inline D sinh_lean(D x, double& bound) noexcept {
    const D e = exp_lean_value(x);
    const D v = ldexp(sub(e, div(D{1, 0}, e)), -1);
    bound     = (lean_exp::Rel + 20 * kU2) * ((v.Hi < 0 ? -v.Hi : v.Hi) + 1) * (1 + 0x1p-40) + fpk::kTiny;
    return v;
}
template <dd_type D>
inline D cosh_lean(D x, double& bound) noexcept {
    const D e = exp_lean_value(x);
    const D v = ldexp(add(e, div(D{1, 0}, e)), -1);
    bound     = (lean_exp::Rel + 20 * kU2) * v.Hi * (1 + 0x1p-40) + fpk::kTiny;
    return v;
}
template <dd_type D>
inline D tanh_lean(D x, double& bound) noexcept {
    const D e2 = exp_lean_value(ldexp(x, 1));
    bound      = (lean_exp::Rel / 2 + 36 * kU2) * (1 + 0x1p-40);
    return div(add(e2, -1.0), add(e2, 1.0));
}

// The inverse hyperbolics as ln w, w within ε relative: within the lean
// log's bound plus ε·(1 + 2^-40).
// - asinh |x|: w = |x| + √(x² + 1): the square 4kU², + 1 2kU², the root
//   2kU² + 7kU², the sum 4kU²: ε = 17kU².
// - acosh x ≥ 1: w = x + √((x − 1)(x + 1)): x − 1 exact for x.Hi ≤ 2, else
//   within 2kU²·(1 + x)/(x − 1) ≤ 6kU²; x + 1 2kU²·(1 + x)/(x + 1) ≤ 2kU²;
//   the product 4kU², the root 6kU² + 7kU², the sum 4kU²: ε = 23kU².
// - atanh |x| < 1: ½·ln((1 + a)/(1 − a)): 1 − a exact for a.Hi ≥ 0.5, else
//   within 2kU²·(1 + a)/(1 − a) ≤ 6kU²; 1 + a 2kU²; the quotient 32kU²:
//   ε = 40kU², and the bound halves.
template <dd_type D>
inline D asinh_lean(D x, double& bound) noexcept {
    const bool negative = x.Hi < 0;
    const D    a        = negative ? neg(x) : x;
    const D    m        = log_lean_value(add(a, sqrt(add(sqr(a), 1.0))), bound);
    bound += 17 * kU2 * (1 + 0x1p-40);
    return negative ? neg(m) : m;
}
template <dd_type D>
inline D acosh_lean(D x, double& bound) noexcept {
    const D v = log_lean_value(add(x, sqrt(mul(add(x, -1.0), add(x, 1.0)))), bound);
    bound += 23 * kU2 * (1 + 0x1p-40);
    return v;
}
template <dd_type D>
inline D atanh_lean(D x, double& bound) noexcept {
    const bool negative = x.Hi < 0;
    const D    a        = negative ? neg(x) : x;
    const D    m        = ldexp(log_lean_value(div(add(a, 1.0), add(neg(a), 1.0)), bound), -1);
    bound               = (bound + 40 * kU2) * 0.5 * (1 + 0x1p-40);
    return negative ? neg(m) : m;
}

// atan t = t − t·z·P(z), P = 1/3 − z/5 + z²/7 − z³/9 + z⁴/11.
inline constexpr fpk::poly<5> kLeanAtanC =
    fpk::series<5>([](int k) { return fpk::term{k % 2 ? -1.0 : 1.0, 2.0 * k + 3}; });

// atan a for 0 ≤ a ≤ 1 (a.Hi ≤ 1), reduced as in atan_unit: c = j/32,
// t = (a − c)/(1 + a·c), |t| ≤ S. a − c is exact (Sterbenz, c ≥ 1/32, or
// c = 0); 1 + a·c within 3kU²; so t within 36kU². atan a = atan c + t + T,
// T = −th·zh·P(zh) in double (th = t.Hi, zh = th²); atan c + t.Hi in one
// exact fast two-sum (atan c ≥ atan(1/32) > S unless c = 0). Absolute:
// - T: three roundings (3kU of |T|), P's Horner error at zh, the
//   alternating series' first omitted term S¹³/13, and T taken at th, not
//   t (kU·S·S²);
// - t: 36kU²·S;
// - the low sum: 4 roundings within Lsum; atan c's table 2^-104.
struct lean_atan {
    static constexpr double            S    = 0.015626; // ≥ 1/64
    static constexpr double            Z    = S * S;
    static constexpr double            A    = 0.7854; // ≥ π/4
    static constexpr fpk::horner_bound HP   = fpk::horner_error(kLeanAtanC, Z, kU);
    static constexpr double            TT   = S * Z * HP.Mag * (1 + 4 * kU); // ≥ |T|
    static constexpr double            ET   = 3 * kU * TT + S * Z * HP.Err + fpk::pow_n(S, 13) / 13 + kU * S * Z;
    static constexpr double            Lsum = 2 * kU * (A + S) + kU * A + kU * S + TT;
    static constexpr double            Unit = up(ET + 36 * kU2 * S + 4 * kU * Lsum + kTableErr * A);
    // atan |x| > 1: π/2 − atan(1/|x|): the quotient 32kU² (slope ≤ 1), the
    // difference 4kU²·(π/2 + π/4), π/2's 2^-104.
    static constexpr double Atan = up(Unit + 32 * kU2 + 10 * kU2 + kTableErr * 1.571);
    // atan2: the quotient of the smaller by the larger 32kU²; ±π or ±π/2 and
    // the sum 4kU²·(π + π/4) and 2^-104 of π.
    static constexpr double Atan2 = up(Unit + 32 * kU2 + 16 * kU2 + kTableErr * 3.1416);
    // asin, acos = atan2 with √((1 − x)(1 + x)) as one argument: 1 ∓ x within
    // 6kU² (as x ± 1 above), the product 4kU², the root 8kU² + 7kU² — 15kU²
    // relative, which moves the angle by at most half of it.
    static constexpr double Asin = up(Atan2 + 8 * kU2);
};

template <dd_type D>
[[gnu::always_inline]] inline D atan_unit_lean(D a) noexcept {
    const double j  = __builtin_nearbyint(a.Hi * 32);
    const double c  = j * (1.0 / 32);
    const D      t  = div(add(a, -c), add_dominant(D{1, 0}, mul(a, c)));
    const double th = t.Hi, zh = th * th;
    const double T  = -(fpk::rounded_product(th, zh) * fpk::horner(zh, kLeanAtanC.C));
    const D      ac = consts<D>::Atan[static_cast<std::size_t>(j)];
    const D      s  = fast_two_sum(ac.Hi, th);
    return fast_two_sum(s.Hi, ((s.Lo + ac.Lo) + t.Lo) + T);
}

template <dd_type D>
inline D atan_lean(D x, double& bound) noexcept {
    const bool negative = x.Hi < 0;
    const D    a        = negative ? neg(x) : x;
    const D    r        = a.Hi <= 1 ? atan_unit_lean(a) : sub(consts<D>::HalfPi, atan_unit_lean(div(D{1, 0}, a)));
    bound               = lean_atan::Atan;
    return negative ? neg(r) : r;
}

// atan2 as in the full kernel, with the lean unit atan.
template <dd_type D>
inline D atan2_lean(D y, D x, double& bound) noexcept {
    using C         = consts<D>;
    bound           = lean_atan::Atan2;
    const double ay = y.Hi < 0 ? -y.Hi : y.Hi, ax_ = x.Hi < 0 ? -x.Hi : x.Hi;
    if (ay == 0 && ax_ == 0) {
        bound = 0;
        return D{0, 0};
    }
    if (ay <= ax_) {
        const D q = div(y, x);
        const D r = q.Hi < 0 ? neg(atan_unit_lean(neg(q))) : atan_unit_lean(q);
        if (x.Hi > 0)
            return r;
        return y.Hi < 0 ? sub(r, C::Pi) : add(r, C::Pi);
    }
    const D q = div(x, y);
    const D r = q.Hi < 0 ? neg(atan_unit_lean(neg(q))) : atan_unit_lean(q);
    return y.Hi > 0 ? sub(C::HalfPi, r) : sub(neg(C::HalfPi), r);
}

template <dd_type D>
inline D asin_lean(D x, double& bound) noexcept {
    const D v = atan2_lean(x, sqrt(mul(add(neg(x), 1.0), add(x, 1.0))), bound);
    bound     = lean_atan::Asin;
    return v;
}
template <dd_type D>
inline D acos_lean(D x, double& bound) noexcept {
    const D v = atan2_lean(sqrt(mul(add(neg(x), 1.0), add(x, 1.0))), x, bound);
    bound     = lean_atan::Asin;
    return v;
}
} // namespace beman::inside::math::detail::dd

#endif // !BEMAN_INSIDE_MATH_NO_FP

#endif // BEMAN_INSIDE_DETAIL_MATH_DD_HPP
