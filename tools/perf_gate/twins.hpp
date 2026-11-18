// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The ideal twins of perfmap.py: for each operation, the plain integer (or
// double) code a careful programmer writes for one fixed grid, with the same
// checks and the same rounding as the library. A twin reads the library's
// grids only at compile time (its constants: notch, Lower, the raw range) and
// works on raws, so its cost is the semantic minimum of the operation on
// that storage.
//
// A raw r of B means the value r·S + O: S = Notch, O = Lower on an index raw,
// S = 1, O = 0 on a value raw. Every operation below is a polynomial in the
// raws with rational coefficients, brought over one constant denominator D:
// the twin computes the integer numerator and divides by D (a constant, so a
// multiply), rounding as the target's mode says.
#ifndef INSIDE_PERF_GATE_TWINS_HPP
#define INSIDE_PERF_GATE_TWINS_HPP

#include <inside/inside.hpp>

#include <bit>
#include <cmath>
#include <cstdint>
#include <expected>
#include <limits>
#include <numeric>
#include <type_traits>

namespace twin {
namespace bi = ins;
namespace bd = ins::detail;
using gr     = bd::grid_number;
using gi     = bd::grid_int;
using i64    = std::int64_t;
using i128   = __int128;

enum class mode { trunc, floor, ceil, half_away, half_even };
enum class range { none, check, clamp, wrap };

// The mode of a policy, as the library reads it (rounding_of).
template <bi::policy_flag P>
inline constexpr mode mode_of = [] {
    if constexpr (!bi::has_flag(P, bd::snap))
        return mode::trunc; // not rounding: a store must be exact (checked)
    else
        switch (bd::rounding_of(P)) {
        case bd::round_mode::floor:
            return mode::floor;
        case bd::round_mode::ceil:
            return mode::ceil;
        case bd::round_mode::nearest:
            return mode::half_away;
        case bd::round_mode::half_even:
            return mode::half_even;
        default:
            return mode::trunc;
        }
}();
template <bi::policy_flag P>
inline constexpr bool rounds = bi::has_flag(P, bd::snap);
template <bi::policy_flag P>
inline constexpr range range_of = (P & bi::clamp)               ? range::clamp
                                  : (P & bi::wrap)              ? range::wrap
                                  : bi::has_flag(P, bi::unsafe) ? range::none
                                                                : range::check;

// The encoding of B: value = raw·S + O.
template <class B>
struct enc {
    using raw                   = bd::raw_t<B>;
    static constexpr bool index = bd::index_storage<B>;
    static constexpr gr   S     = index ? bi::notch_of<B> : gr{1};
    static constexpr gr   O     = index ? bi::lower_of<B> : gr{0};
    static constexpr i64  lo    = static_cast<i64>(bd::raw_lo_exact<B>);
    static constexpr i64  hi    = static_cast<i64>(bd::raw_hi_exact<B>);
};

consteval i64 to64(const gi& v) { return static_cast<i64>(v); }
consteval gi  num(const gr& r) { return bd::grid_numerator(r); }
consteval gi  den(const gr& r) { return bd::grid_denominator(r); }
consteval gr  lcm_den(const gr& a, const gr& b) { // lcm of the denominators
    const gi g = bd::gcd(den(a), den(b));
    return gr{den(a) / g * den(b)};
}

// A linear form a·x + b over D: coefficients integers.
struct lin {
    i64 A, B, D;
};
consteval lin make_lin(const gr& a, const gr& b) {
    const gr d = lcm_den(a, b);
    return {to64(num(a * d)), to64(num(b * d)), to64(num(d))};
}
struct bilin { // a·x + b·y + c over D
    i64 A, B, C, D;
};
consteval bilin make_bilin(const gr& a, const gr& b, const gr& c) {
    const gr d = lcm_den(lcm_den(a, b), c);
    return {to64(num(a * d)), to64(num(b * d)), to64(num(c * d)), to64(num(d))};
}

// n / d rounded by M; d > 0 (a constant at the call sites that matter):
// one division by d or 2d and a sign fix, as a careful hand-written rounding.
template <class W>
constexpr W floordiv(W n, W d) {
    const W q = n / d;
    return q - ((n % d) < 0);
}
template <mode M, class W>
constexpr W rdiv(W n, W d) {
    if constexpr (M == mode::floor)
        return floordiv(n, d);
    else if constexpr (M == mode::ceil)
        return -floordiv(-n, d);
    else if constexpr (M == mode::half_away) {
        const W a = n < 0 ? -n : n;
        const W q = (2 * a + d) / (2 * d);
        return n < 0 ? -q : q;
    } else if constexpr (M == mode::half_even) {
        const W t = 2 * n + d, dd = 2 * d; // floor(n/d + 1/2), then a tie to even
        W       q = floordiv(t, dd);
        if (t - q * dd == 0 && (q & 1))
            --q;
        return q;
    } else
        return n / d;
}
// n / d for a runtime d of either sign.
template <mode M, class W>
constexpr W rdiv_signed(W n, W d) {
    return d < 0 ? rdiv<M>(-n, -d) : rdiv<M>(n, d);
}

// The store of a slot q (already in raw units of B) under range handling R:
// the raw, or the error for check.
template <class B, range R, class W>
constexpr auto finish(W q) {
    using raw      = typename enc<B>::raw;
    constexpr W lo = enc<B>::lo, hi = enc<B>::hi;
    if constexpr (R == range::check) {
        if (q < lo || q > hi)
            return std::expected<raw, int>{std::unexpect, 3};
        return std::expected<raw, int>{static_cast<raw>(q)};
    } else if constexpr (R == range::clamp)
        return static_cast<raw>(q < lo ? lo : q > hi ? hi : q);
    else if constexpr (R == range::wrap) {
        constexpr W p = hi - lo + 1;
        W           o = (q - lo) % p;
        return static_cast<raw>((o < 0 ? o + p : o) + lo);
    } else
        return static_cast<raw>(q);
}

// An exact store under check: off the notch is an error too.
template <class B, range R, mode M, bool Rounds, class W>
constexpr auto store_num(W n, W d) {
    if constexpr (!Rounds && R == range::check) {
        if (n % d != 0)
            return std::expected<typename enc<B>::raw, int>{std::unexpect, 4};
        return finish<B, R>(n / d);
    } else
        return finish<B, R>(d == 1 ? n : rdiv<M>(n, d));
}

// Work type: int64 unless a bound passes 2^62.
template <bool Wide>
using work = std::conditional_t<Wide, i128, i64>;
consteval bool wide_needed(const gr& m) { return !(m < gr{gi{1} << 62}) || !(gr{-(gi{1} << 62)} < m); }

//---------------------------------------------------------------------------
// Stores
//---------------------------------------------------------------------------
// An integer n into B: raw = (n − O)/S. The range is tested on n first (its
// limits are constants), so the arithmetic after it stays in int64; a wrap
// folds in 128 bits.
template <class B, range R, mode M, bool Rounds>
constexpr auto store_int(i64 n) {
    constexpr lin l  = make_lin(gr{1} / enc<B>::S, -enc<B>::O / enc<B>::S);
    using raw        = typename enc<B>::raw;
    constexpr gr  lo = bi::lower_of<B>, hi = bi::upper_of<B>;
    constexpr i64 nlo =
        to64(bd::grid_numerator(lo) / bd::grid_denominator(lo)) + (lo < gr{0} ? 0 : (den(lo) == gi{1} ? 0 : 1));
    constexpr i64 nhi =
        to64(bd::grid_numerator(hi) / bd::grid_denominator(hi)) - (hi < gr{0} ? (den(hi) == gi{1} ? 0 : 1) : 0);
    if constexpr (R == range::wrap)
        return store_num<B, R, M, Rounds, i128>(i128{n} * l.A + l.B, i128{l.D});
    else {
        if constexpr (R == range::check) {
            if (n < nlo || n > nhi)
                return std::expected<raw, int>{std::unexpect, 3};
        } else if constexpr (R == range::clamp) {
            if (n < nlo)
                return static_cast<raw>(enc<B>::lo);
            if (n > nhi)
                return static_cast<raw>(enc<B>::hi);
        }
        return store_num<B, R, M, Rounds, i64>(n * l.A + l.B, i64{l.D});
    }
}
// A raw of A into B: raw_B = (rA·S_A + O_A − O_B)/S_B.
template <class B, class A, range R, mode M, bool Rounds>
constexpr auto store_inside(typename enc<A>::raw r) {
    constexpr lin  l    = make_lin(enc<A>::S / enc<B>::S, (enc<A>::O - enc<B>::O) / enc<B>::S);
    constexpr bool wide = wide_needed(gr{gi{l.A}} * gr{gi{enc<A>::hi}} + gr{gi{l.B}}) ||
                          wide_needed(gr{gi{l.A}} * gr{gi{enc<A>::lo}} + gr{gi{l.B}});
    using W             = work<wide>;
    return store_num<B, R, M, Rounds, W>(W(r) * l.A + l.B, W(l.D));
}
// A double into B, correctly rounded: x = v·Den(S)/Num(S) − O/S. With S = 1/D
// the product v·D is one rounding away from exact; fma gives its error e,
// which decides the floor at an integer and the side of a tie.
template <class B, range R, mode M>
inline auto store_double(double v) {
    constexpr double D    = static_cast<double>(to64(den(enc<B>::S)));
    constexpr double N    = static_cast<double>(to64(num(enc<B>::S)));
    constexpr double base = static_cast<double>(enc<B>::O / enc<B>::S);
    constexpr bool   pow2 = std::has_single_bit(static_cast<std::uint64_t>(to64(den(enc<B>::S))));
    double           x    = v * D;
    double           e    = 0;
    if constexpr (!pow2)
        e = std::fma(v, D, -x);
    if constexpr (N != 1)
        x /= N;
    x -= base;
    double q;
    if constexpr (M == mode::floor) {
        q = std::floor(x);
        if (!pow2 && q == x && e < 0)
            q -= 1;
    } else if constexpr (M == mode::ceil) {
        q = std::ceil(x);
        if (!pow2 && q == x && e > 0)
            q += 1;
    } else if constexpr (M == mode::half_away || M == mode::half_even) {
        q              = M == mode::half_away ? std::round(x) : std::nearbyint(x);
        const double f = x - std::floor(x);
        if (!pow2 && f == 0.5 && e != 0)
            q = e > 0 ? std::floor(x) + 1 : std::floor(x);
    } else {
        q = std::trunc(x);
        if (!pow2 && q == x && e != 0 && ((x > 0) == (e < 0)))
            q += x > 0 ? -1 : 1;
    }
    if (!(q == q))
        return finish<B, R>(static_cast<i64>(enc<B>::hi) + 1);
    return finish<B, R>(static_cast<i64>(q));
}

//---------------------------------------------------------------------------
// Arithmetic: the result raw of R from the operand raws.
//---------------------------------------------------------------------------
template <class R, class A, class Bt, int Sign>
constexpr auto add(typename enc<A>::raw a, typename enc<Bt>::raw b) {
    constexpr bilin l = make_bilin(enc<A>::S / enc<R>::S,
                                   gr{Sign} * enc<Bt>::S / enc<R>::S,
                                   (enc<A>::O + gr{Sign} * enc<Bt>::O - enc<R>::O) / enc<R>::S);
    using W           = std::conditional_t<(sizeof(typename enc<R>::raw) > 4), i64, std::int32_t>;
    const W n         = W(a) * l.A + W(b) * l.B + l.C;
    return static_cast<typename enc<R>::raw>(l.D == 1 ? n : n / l.D);
}
template <class R, class A, class Bt>
constexpr auto mul(typename enc<A>::raw a, typename enc<Bt>::raw b) {
    // (a·Sa + Oa)(b·Sb + Ob) − Or, over Sr
    const gr     sr = enc<R>::S;
    constexpr gr c2 = enc<A>::S * enc<Bt>::S / enc<R>::S;
    constexpr gr ca = enc<A>::S * enc<Bt>::O / enc<R>::S;
    constexpr gr cb = enc<A>::O * enc<Bt>::S / enc<R>::S;
    constexpr gr c0 = (enc<A>::O * enc<Bt>::O - enc<R>::O) / enc<R>::S;
    (void)sr;
    constexpr gr  d  = lcm_den(lcm_den(c2, ca), lcm_den(cb, c0));
    constexpr i64 K2 = to64(num(c2 * d)), KA = to64(num(ca * d)), KB = to64(num(cb * d)), K0 = to64(num(c0 * d)),
                  D = to64(num(d));
    using W         = std::conditional_t<(sizeof(typename enc<R>::raw) > 4), i128, i64>;
    W n             = W(a) * W(b) * K2;
    if constexpr (KA != 0)
        n += W(a) * KA;
    if constexpr (KB != 0)
        n += W(b) * KB;
    n += K0;
    return static_cast<typename enc<R>::raw>(D == 1 ? n : n / D);
}
template <class R, class A>
constexpr auto neg(typename enc<A>::raw a) {
    constexpr lin l = make_lin(-enc<A>::S / enc<R>::S, (-enc<A>::O - enc<R>::O) / enc<R>::S);
    const i64     n = i64(a) * l.A + l.B;
    return static_cast<typename enc<R>::raw>(l.D == 1 ? n : n / l.D);
}

// The value's numerator over a constant: v = (raw·A + B)/D.
template <class X>
struct value_form {
    static constexpr lin l = make_lin(enc<X>::S, enc<X>::O);
};

// a < b and a == b across grids: both over one denominator.
template <class A, class Bt>
struct cmp_form {
    static constexpr gr  d  = lcm_den(lcm_den(enc<A>::S, enc<A>::O), lcm_den(enc<Bt>::S, enc<Bt>::O));
    static constexpr i64 aA = to64(num(enc<A>::S * d)), aB = to64(num(enc<A>::O * d));
    static constexpr i64 bA = to64(num(enc<Bt>::S * d)), bB = to64(num(enc<Bt>::O * d));
};
template <class A, class Bt>
constexpr bool less(typename enc<A>::raw a, typename enc<Bt>::raw b) {
    using c = cmp_form<A, Bt>;
    return i64(a) * c::aA + c::aB < i64(b) * c::bA + c::bB;
}
template <class A, class Bt>
constexpr bool equal(typename enc<A>::raw a, typename enc<Bt>::raw b) {
    using c = cmp_form<A, Bt>;
    return i64(a) * c::aA + c::aB == i64(b) * c::bA + c::bB;
}

// div(a, b, mode) onto the integers: round((a·Sa + Oa)/(b·Sb + Ob)).
template <class A, class Bt, mode M, bool ZeroCheck>
constexpr auto div_int(typename enc<A>::raw a, typename enc<Bt>::raw b) {
    constexpr lin la = value_form<A>::l, lb = value_form<Bt>::l;
    const i64     n = (i64(a) * la.A + la.B) * lb.D;
    const i64     d = (i64(b) * lb.A + lb.B) * la.D;
    if constexpr (ZeroCheck)
        if (d == 0)
            return std::expected<i64, int>{std::unexpect, 2};
    if constexpr (ZeroCheck)
        return std::expected<i64, int>{rdiv_signed<M>(n, d)};
    else
        return rdiv_signed<M>(n, d);
}
// a % b, truncated: the remainder's raw on the result grid (value = r).
template <class A, class Bt>
constexpr i64 mod_int(typename enc<A>::raw a, typename enc<Bt>::raw b) {
    constexpr lin la = value_form<A>::l, lb = value_form<Bt>::l;
    const i64     n = (i64(a) * la.A + la.B) * lb.D;
    const i64     d = (i64(b) * lb.A + lb.B) * la.D;
    return n % d;
}
// The exact quotient as a reduced fraction (the raw of a continuous grid).
struct fraction {
    i64 Num, Den;
};
template <class A, class Bt, bool ZeroCheck>
constexpr auto div_exact(typename enc<A>::raw a, typename enc<Bt>::raw b) {
    constexpr lin la = value_form<A>::l, lb = value_form<Bt>::l;
    i64           n = (i64(a) * la.A + la.B) * lb.D;
    i64           d = (i64(b) * lb.A + lb.B) * la.D;
    if constexpr (ZeroCheck)
        if (d == 0)
            return std::expected<fraction, int>{std::unexpect, 2};
    if (d < 0)
        n = -n, d = -d;
    const i64 g = std::gcd(n, d);
    if constexpr (ZeroCheck)
        return std::expected<fraction, int>{fraction{n / g, d / g}};
    else
        return fraction{n / g, d / g};
}
// A value onto Out's grid (mul<Out>, div<Out>): n/d onto raw units of Out.
template <class Out, range R, mode M, class W>
constexpr auto onto(W n, W d) { // value n/d, d > 0
    // raw = (n/d − O)/S = (n·Sd − O·d·…)/…: over constants
    constexpr lin l = make_lin(gr{1} / enc<Out>::S, -enc<Out>::O / enc<Out>::S);
    return finish<Out, R>(rdiv<M>(n * l.A + d * l.B, d * l.D));
}
template <class Out, class A, class Bt, range R, mode M>
constexpr auto mul_onto(typename enc<A>::raw a, typename enc<Bt>::raw b) {
    constexpr lin la = value_form<A>::l, lb = value_form<Bt>::l;
    using W          = i128;
    const W       n  = (W(a) * la.A + la.B) * (W(b) * lb.A + lb.B);
    constexpr W   d  = W(la.D) * lb.D;
    constexpr lin lo = make_lin(gr{1} / enc<Out>::S, -enc<Out>::O / enc<Out>::S);
    return finish<Out, R>(rdiv<M>(n * lo.A + d * lo.B, d * lo.D));
}
template <class Out, class A, class Bt, range R, mode M>
constexpr auto div_onto(typename enc<A>::raw a, typename enc<Bt>::raw b) {
    constexpr lin la = value_form<A>::l, lb = value_form<Bt>::l;
    constexpr lin lo = make_lin(gr{1} / enc<Out>::S, -enc<Out>::O / enc<Out>::S);
    const i64     n  = (i64(a) * la.A + la.B) * lb.D;
    const i64     d  = (i64(b) * lb.A + lb.B) * la.D; // > 0 at the call sites
    return finish<Out, R>(rdiv<M>(n * lo.A + d * lo.B, d * lo.D));
}

//---------------------------------------------------------------------------
// Conversions out
//---------------------------------------------------------------------------
template <class X, class T, mode M>
constexpr auto to_int(typename enc<X>::raw r) {
    constexpr lin l  = value_form<X>::l;
    const i64     v  = l.D == 1 ? i64(r) * l.A + l.B : rdiv<M>(i64(r) * l.A + l.B, l.D);
    constexpr i64 tl = std::numeric_limits<T>::min(), th = std::numeric_limits<T>::max();
    if (v < tl || v > th)
        return std::expected<T, int>{std::unexpect, 3};
    return std::expected<T, int>{static_cast<T>(v)};
}
template <class X>
constexpr double to_double(typename enc<X>::raw r) {
    constexpr lin l = value_form<X>::l;
    if constexpr (std::has_single_bit(static_cast<std::uint64_t>(l.D)))
        return static_cast<double>(i64(r) * l.A + l.B) * (1.0 / static_cast<double>(l.D));
    else
        return static_cast<double>(i64(r) * l.A + l.B) / static_cast<double>(l.D);
}
template <class X>
constexpr fraction numerator_denominator(typename enc<X>::raw r) {
    constexpr lin l = value_form<X>::l;
    const i64     n = i64(r) * l.A + l.B;
    if constexpr (std::has_single_bit(static_cast<std::uint64_t>(l.D))) {
        const int s = std::countr_zero(static_cast<std::uint64_t>(n) | static_cast<std::uint64_t>(l.D));
        return {n >> s, l.D >> s};
    } else {
        const i64 g = std::gcd(n, l.D);
        return {n / g, l.D / g};
    }
}
} // namespace twin

#endif // INSIDE_PERF_GATE_TWINS_HPP
