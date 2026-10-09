// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_MATH_ADAPTIVE_HPP
#define BEMAN_INSIDE_DETAIL_MATH_ADAPTIVE_HPP

#include <beman/inside/inside.hpp>

#include <array>
#include <bit>

#include <cstddef>
#include <expected>
#include <optional>
#include <utility>

//---------------------------------------------------------------------------
// The adaptive math engine's foundation: correctly rounded results on any
// output grid, at a precision chosen from that grid.
//
// A core computes f(x) in signed fixed point Q.W (an integer Y standing for
// Y·2^-W) and says how far off it may be: the true value lies within
// Error·2^-W of Y (an `approx`). `decide` maps both ends of that interval onto
// the output grid under the grid's rounding mode. When they land on the same
// slot, that slot is the correctly rounded result — whichever core, tier or
// precision produced it. When they straddle a rounding boundary, the driver
// (`evaluate`) runs the core again at twice the precision (Ziv's strategy).
//
// Transcendental results land exactly on a boundary only at a few exact
// inputs (exp(0), log(1), ...); the cores return those with Error 0, so for
// every other input the escalation ends. A cap stops it in any case; past the
// cap the result is the slot nearest Y.
//
// Everything here is constexpr and integer-only (wide_int), so a result is the
// same at compile time, at runtime, with or without an FPU.
//---------------------------------------------------------------------------
namespace beman::inside::math::detail::ax {
using namespace ::beman::inside::detail;

// Signed fixed point with at least Bits value bits (the word of a Q.W value
// whose magnitude stays below 2^(Bits − W)).
template <int Bits>
using fixed_t = wide_sint<limbs_for_bits(Bits + 1)>;

template <typename T>
inline constexpr std::size_t limbs_of = sizeof(T) / sizeof(umax);

// The same value in a word of twice the limbs (for products and shifted
// dividends).
template <std::size_t K>
using double_t = wide_sint<2 * K>;

//---------------------------------------------------------------------------
// Q.W arithmetic. Products and quotients truncate toward zero: each is
// within one unit 2^-W of the true value, and a series' shrinking terms
// reach 0.
//---------------------------------------------------------------------------
template <std::size_t K>
constexpr wide_sint<K> mul_q(const wide_sint<K>& a, const wide_sint<K>& b, int W) noexcept {
    if constexpr (K == 1) {
        // One limb: the 128-bit product of the magnitudes, shifted.
        const bool             na = a.negative(), nb = b.negative();
        const umax             ma = na ? umax{0} - a.Word[0] : a.Word[0];
        const umax             mb = nb ? umax{0} - b.Word[0] : b.Word[0];
        const limb::pair<umax> p  = limb::mul(ma, mb);
        const umax             m  = W == 0 ? p.Lo : W < 64 ? (p.Lo >> W) | (p.Hi << (64 - W)) : p.Hi >> (W - 64);
        wide_sint<1>           r;
        r.Word[0] = (na != nb) ? umax{0} - m : m;
        return r;
    } else if constexpr (K == 2) {
        // Two limbs: the four partial products into four limbs, then the shift.
        const bool             na = a.negative(), nb = b.negative();
        const wide_uint<2>     ma{na ? -a : a}, mb{nb ? -b : b};
        const limb::pair<umax> p00 = limb::mul(ma.Word[0], mb.Word[0]);
        const limb::pair<umax> p01 = limb::mul(ma.Word[0], mb.Word[1]);
        const limb::pair<umax> p10 = limb::mul(ma.Word[1], mb.Word[0]);
        const limb::pair<umax> p11 = limb::mul(ma.Word[1], mb.Word[1]);
        umax                   c1 = 0, c2 = 0, c3 = 0, c4 = 0;
        const umax             w0 = p00.Lo;
        umax                   w1 = limb::add_carry(p00.Hi, p01.Lo, c1);
        w1                        = limb::add_carry(w1, p10.Lo, c2);
        umax w2                   = limb::add_carry(p11.Lo, p01.Hi, c3);
        w2                        = limb::add_carry(w2, p10.Hi, c4);
        umax c5                   = 0;
        w2                        = limb::add_carry(w2, c1 + c2, c5);
        const umax        w3      = p11.Hi + c3 + c4 + c5;
        const umax        w[4]    = {w0, w1, w2, w3};
        const std::size_t ws      = static_cast<std::size_t>(W / 64);
        const int         bs      = W % 64;
        wide_uint<2>      m;
        for (std::size_t i = 0; i < 2; ++i) {
            const umax lo = i + ws < 4 ? w[i + ws] : 0;
            const umax hi = i + ws + 1 < 4 ? w[i + ws + 1] : 0;
            m.Word[i]     = bs == 0 ? lo : (lo >> bs) | (hi << (64 - bs));
        }
        const wide_sint<2> r{m};
        return na != nb ? -r : r;
    } else {
        // Schoolbook on the magnitudes into 2K limbs, then the shift.
        const bool         na = a.negative(), nb = b.negative();
        const wide_uint<K> ma{na ? -a : a}, mb{nb ? -b : b};
        umax               prod[2 * K]{};
        for (std::size_t i = 0; i < K; ++i) {
            umax carry = 0;
            for (std::size_t j = 0; j < K; ++j) {
                const limb::pair<umax> t  = limb::mul(ma.Word[i], mb.Word[j]);
                umax                   c1 = 0, c2 = 0;
                umax                   v = limb::add_carry(prod[i + j], t.Lo, c1);
                v                        = limb::add_carry(v, carry, c2);
                prod[i + j]              = v;
                carry                    = t.Hi + c1 + c2;
            }
            prod[i + K] = carry;
        }
        const std::size_t ws = static_cast<std::size_t>(W / 64);
        const int         bs = W % 64;
        wide_uint<K>      m;
        for (std::size_t i = 0; i < K; ++i) {
            const umax lo = i + ws < 2 * K ? prod[i + ws] : 0;
            const umax hi = i + ws + 1 < 2 * K ? prod[i + ws + 1] : 0;
            m.Word[i]     = bs == 0 ? lo : (lo >> bs) | (hi << (64 - bs));
        }
        const wide_sint<K> r{m};
        return na != nb ? -r : r;
    }
}

// v / d for a small positive d, truncating toward zero (series terms).
template <std::size_t K>
constexpr wide_sint<K> div_small(const wide_sint<K>& v, umax d) noexcept;

// a / b at scale 2^W. Pre: b != 0.
template <std::size_t K>
constexpr wide_sint<K> div_q(const wide_sint<K>& a, const wide_sint<K>& b, int W) noexcept {
    using D = double_t<K>;
    return static_cast<wide_sint<K>>((D{a} << W) / D{b});
}

// 2^W as a Q.W one.
template <std::size_t K>
constexpr wide_sint<K> one_q(int W) noexcept {
    return wide_sint<K>{1} << W;
}

// An error bound v·2^-n, rounded down (callers add 1): 0 once n ≥ 64, where
// the shift itself would be undefined.
constexpr umax shr_bound(umax v, int n) noexcept { return n < 64 ? v >> n : 0; }

// v rounded to the nearest multiple of 2^-sh (half away from zero), shifted
// down by sh: changes the scale from 2^(W+sh) to 2^W within ½ unit.
template <std::size_t K>
constexpr wide_sint<K> round_shift(const wide_sint<K>& v, int sh) noexcept {
    if (sh <= 0)
        return v << (-sh);
    const wide_sint<K> half = wide_sint<K>{1} << (sh - 1);
    return v.negative() ? -((-v + half) >> sh) : (v + half) >> sh;
}

//---------------------------------------------------------------------------
// Exact values into fixed point.
//---------------------------------------------------------------------------
// n/d (d > 0) rounded to nearest, half away from zero; one-limb divisions
// when d fits a limb.
template <std::size_t K>
constexpr wide_sint<K> nearest_div(const wide_sint<K>& n, const wide_sint<K>& d) noexcept {
    if (bit_width_of(d) <= 64) {
        const umax dd     = d.Word[0];
        const bool neg    = n.negative();
        const auto [q, r] = divmod_small(wide_uint<K>{neg ? -n : n}, dd);
        wide_sint<K> m{q};
        if (r >= dd - r)
            m += wide_sint<K>{1}; // 2r ≥ d
        return neg ? -m : m;
    }
    return rounded_div<round_mode::nearest>(n, d);
}

// x·2^W rounded to the nearest integer (half away from zero): within ½ unit.
template <int W, std::size_t K, std::size_t E>
constexpr wide_sint<K> to_q(const exact_frac<E>& x) noexcept {
    constexpr int shift = W >= 0 ? W : -W;
    using I             = wide_sint<exact_max<E, K> + limbs_for_bits(shift) + 1>;
    I n{x.Num}, d{x.Den};
    if constexpr (W >= 0)
        n = n << W;
    else
        d = d << shift;
    return static_cast<wide_sint<K>>(nearest_div(n, d));
}

// ⌊log2 |x|⌋ for x != 0.
template <std::size_t E>
constexpr int floor_log2(const exact_frac<E>& x) noexcept {
    using I    = wide_sint<E>;
    const I  n = x.Num.negative() ? -x.Num : x.Num;
    const I& d = x.Den;
    int      e = bit_width_of(n) - bit_width_of(d);
    // n/d ≥ 2^e  ⇔  n ≥ d·2^e (or n·2^-e ≥ d).
    const bool ge = e >= 0 ? n >= (d << e) : (n << (-e)) >= d;
    return ge ? e : e - 1;
}

//---------------------------------------------------------------------------
// Output precision. out_bits: fractional bits that resolve Out's notch
// (2^-out_bits ≤ notch/2). mag_bits: integer bits of Out's largest value.
//---------------------------------------------------------------------------
template <insidable Out>
inline constexpr int out_bits = [] {
    if constexpr (!notched<Out>)
        return 64;
    else {
        // notch = p/q: need 2^-b ≤ p/(2q), i.e. b ≥ log2(2q/p).
        const grid_wide p = wide_numerator(notch_of<Out>), q = wide_denominator(notch_of<Out>);
        const int       b = bit_width_of(q) - bit_width_of(p) + 2;
        return b > 0 ? b : 0;
    }
}();

template <insidable Out>
inline constexpr int mag_bits = grid_magnitude_bits<Out> > 0 ? grid_magnitude_bits<Out> : 0;

//---------------------------------------------------------------------------
// approx — a core's result: the true value lies in
// [(Value − Error)·2^-Scale, (Value + Error)·2^-Scale]. Error 0 means exact.
//---------------------------------------------------------------------------
template <std::size_t K>
struct approx {
    wide_sint<K> Value;
    int          Scale;
    umax         Error;
};

//---------------------------------------------------------------------------
// decide — the slot offset of an approx on Out's grid under Out's rounding
// mode, when both ends of its error interval round to the same slot.
//---------------------------------------------------------------------------
template <insidable Out>
inline constexpr round_mode out_rounding = rounding_of(policy_of<Out>);

// Limbs for the exact value Y/2^W of a K-limb approx, with room for the
// product with Out's notch in exact_index.
template <insidable Out, std::size_t K>
inline constexpr std::size_t decide_limbs = K + 1 + exact_limbs<Out>;

template <insidable Out, std::size_t K>
struct decision {
    bool                            Decided;
    wide_sint<decide_limbs<Out, K>> Index; // Out's slot offset (may be out of range)
};

template <insidable Out, std::size_t K>
constexpr exact_frac<decide_limbs<Out, K>> scaled_value(const wide_sint<K>& y, int W) noexcept {
    using I = wide_sint<decide_limbs<Out, K>>;
    if (W < 0)
        return {I{y} << (-W), I{1}}; // a scale past the units: Y·2^|W|
    return {I{y}, I{1} << W};
}

// The fast index: a notch p/q with p and q in 64 bits and a scale S ≥ 1.
// value/notch = Y·q/(p·2^S): split Y·q = A·2^S + R, then A = B·p + r with
// one-limb divisions; the rounding reads r and R exactly (see below).
template <insidable Out>
inline constexpr bool notch_fits64 = [] {
    if constexpr (!notched<Out>)
        return false;
    else {
        const grid_wide p = wide_numerator(notch_of<Out>), q = wide_denominator(notch_of<Out>);
        return !(grid_wide{std::numeric_limits<umax>::max()} < p) &&
               !(grid_wide{std::numeric_limits<umax>::max()} < q);
    }
}();

template <std::size_t K>
constexpr wide_sint<K> div_small(const wide_sint<K>& v, umax d) noexcept {
    const bool         neg = v.negative();
    const wide_sint<K> q{divmod_small(wide_uint<K>{neg ? -v : v}, d).Quotient};
    return neg ? -q : q;
}

// The rounding step of fast_index: the magnitude's quotient b by p, its
// remainder r, and how the shifted-out part R compares with half a unit
// (−1 below, 0 equal, 1 above; R == 0 known separately).
template <round_mode M>
constexpr bool round_up(bool neg, umax b0, umax r, umax p, bool low_zero, int low_vs_half) noexcept {
    remainder_class c = (r == 0 && low_zero) ? remainder_class::zero : remainder_class::below_half;
    if constexpr (M == round_mode::nearest || M == round_mode::half_even) {
        const umax d = p - r; // r + R against p/2, R the shifted-out part
        if (r > d || (r == d && !low_zero))
            c = remainder_class::above_half;
        else if (r == d)
            c = remainder_class::half;
        else if (r + 1 == d)
            c = low_vs_half < 0    ? remainder_class::below_half
                : low_vs_half == 0 ? remainder_class::half
                                   : remainder_class::above_half;
    }
    return rounds_away(M, neg, c, (b0 & 1u) != 0);
}

// Out's slot offset of y·2^-S (S ≥ 1) rounded by M, as value-index rounding
// (the sign rules of rounded_div).
template <insidable Out, round_mode M, std::size_t K>
constexpr wide_sint<K + 2> fast_index(const wide_sint<K>& y, int S) noexcept {
    // On limb arrays: |y|·q, the part above 2^S divided by p, and the part
    // below read as the half-unit bit (bit S−1) plus a sticky bit for the rest.
    using J                   = wide_sint<K + 2>;
    constexpr std::size_t N   = K + 1;
    constexpr umax        p   = static_cast<umax>(wide_numerator(notch_of<Out>));
    constexpr umax        q   = static_cast<umax>(wide_denominator(notch_of<Out>));
    const bool            neg = y.negative();
    const wide_uint<K>    mag{neg ? -y : y};
    umax                  a[N]{};
    umax                  carry = 0;
    for (std::size_t i = 0; i < K; ++i) {
        const limb::pair<umax> t = limb::mul(mag.Word[i], q);
        umax                   c = 0;
        a[i]                     = limb::add_carry(t.Lo, carry, c);
        carry                    = t.Hi + c;
    }
    a[K]     = carry;
    auto bit = [&](int i) -> bool {
        const std::size_t w = static_cast<std::size_t>(i / 64);
        return w < N && ((a[w] >> (i % 64)) & 1u) != 0;
    };
    // Any bit set below position n?
    auto sticky = [&](int n) -> bool {
        const std::size_t full = static_cast<std::size_t>(n / 64);
        for (std::size_t w = 0; w < full && w < N; ++w)
            if (a[w] != 0)
                return true;
        const int rest = n % 64;
        return full < N && rest != 0 && (a[full] & ((umax{1} << rest) - 1)) != 0;
    };
    const bool half_bit = bit(S - 1), below_half = sticky(S - 1);
    const bool low_zero = !half_bit && !below_half;
    const int  vs_half  = !half_bit ? -1 : below_half ? 1 : 0;
    // hi = a >> S
    umax              hi[N]{};
    const std::size_t ws = static_cast<std::size_t>(S / 64);
    const int         bs = S % 64;
    for (std::size_t i = 0; i + ws < N; ++i) {
        const umax lo = a[i + ws];
        const umax up = i + ws + 1 < N ? a[i + ws + 1] : 0;
        hi[i]         = bs == 0 ? lo : (lo >> bs) | (up << (64 - bs));
    }
    umax r = 0;
    if constexpr (p != 1)
        for (std::size_t i = N; i-- > 0;) {
            const limb::pair<umax> d = limb::div(r, hi[i], p);
            hi[i]                    = d.Hi;
            r                        = d.Lo;
        }
    J m{0};
    for (std::size_t i = 0; i < N; ++i)
        m.Word[i] = hi[i];
    if (round_up<M>(neg, hi[0], r, p, low_zero, vs_half))
        m += J{1};
    constexpr J base = static_cast<J>(slot_base<Out>);
    return (neg ? -m : m) - base;
}

// The one-pass decision, for a notch p/q in 64 bits, a scale S ≥ 1 and an
// error e > 0. On the magnitude u = |y|·q (plus half a slot, p·2^(S−1),
// for the nearest modes) the slot cells are [j·D, (j+1)·D), D = p·2^S.
// When both ends u ∓ e·q lie strictly inside one cell, every value of the
// interval rounds to the same slot under every rule, ties included, so
// this is the slot fast_index gives at each end: from one product, two
// shifts and one short division by p. Otherwise false, and decide asks
// fast_index at both ends.
template <insidable Out, round_mode M, std::size_t K>
constexpr bool decide_fast(const wide_sint<K>& y, umax e, int S, wide_sint<K + 2>& index) noexcept {
    using J                       = wide_sint<K + 2>;
    constexpr std::size_t N       = K + 1; // |y|·q fits K + 1 limbs
    constexpr umax        p       = static_cast<umax>(wide_numerator(notch_of<Out>));
    constexpr umax        q       = static_cast<umax>(wide_denominator(notch_of<Out>));
    constexpr bool        nearest = M == round_mode::nearest || M == round_mode::half_even;
    const bool            neg     = y.negative();
    const wide_uint<K>    mag{neg ? -y : y};
    umax                  u[N]{};
    {
        umax carry = 0;
        for (std::size_t i = 0; i < K; ++i) {
            const limb::pair<umax> t = limb::mul(mag.Word[i], q);
            umax                   c = 0;
            u[i]                     = limb::add_carry(t.Lo, carry, c);
            carry                    = t.Hi + c;
        }
        u[K] = carry;
    }
    if constexpr (nearest) // + p·2^(S−1)
    {
        const std::size_t w = static_cast<std::size_t>((S - 1) / 64);
        const int         b = (S - 1) % 64;
        if (w >= N)
            return false;
        umax carry = 0;
        u[w]       = limb::add_carry(u[w], p << b, carry);
        if (w + 1 < N)
            u[w + 1] = limb::add_carry(u[w + 1], b == 0 ? 0 : p >> (64 - b), carry);
        for (std::size_t i = w + 2; i < N; ++i)
            u[i] = limb::add_carry(u[i], umax{0}, carry);
        if (carry != 0)
            return false;
    }
    // The ends u − E and u + E, E = e·q.
    const limb::pair<umax> E = limb::mul(e, q);
    umax                   lo[N], hi[N];
    {
        umax borrow = 0, carry = 0;
        for (std::size_t i = 0; i < N; ++i) {
            const umax ei = i == 0 ? E.Lo : i == 1 ? E.Hi : 0;
            lo[i]         = limb::sub_borrow(u[i], ei, borrow);
            hi[i]         = limb::add_carry(u[i], ei, carry);
        }
        if (borrow != 0 || carry != 0)
            return false; // near 0, or past the limbs
    }
    // Shifted down by S: the cell index times p, plus the offset within.
    const std::size_t ws          = static_cast<std::size_t>(S / 64);
    const int         bs          = S % 64;
    bool              low_nonzero = false; // lo below 2^S
    for (std::size_t i = 0; i < ws && i < N; ++i)
        low_nonzero = low_nonzero || lo[i] != 0;
    if (ws < N && bs != 0)
        low_nonzero = low_nonzero || (lo[ws] & ((umax{1} << bs) - 1)) != 0;
    umax hl[N]{}, hh[N]{};
    for (std::size_t i = 0; i + ws < N; ++i) {
        const umax l0 = lo[i + ws], l1 = i + ws + 1 < N ? lo[i + ws + 1] : 0;
        const umax h0 = hi[i + ws], h1 = i + ws + 1 < N ? hi[i + ws + 1] : 0;
        hl[i] = bs == 0 ? l0 : (l0 >> bs) | (l1 << (64 - bs));
        hh[i] = bs == 0 ? h0 : (h0 >> bs) | (h1 << (64 - bs));
    }
    // δ = hh − hl, one limb at most.
    umax delta;
    {
        umax borrow = 0, d[N];
        for (std::size_t i = 0; i < N; ++i)
            d[i] = limb::sub_borrow(hh[i], hl[i], borrow);
        for (std::size_t i = 1; i < N; ++i)
            if (d[i] != 0)
                return false;
        delta = d[0];
    }
    // B = ⌊hl/p⌋, r = hl mod p: same cell when r + δ < p; strictly inside
    // when lo is not on the cell's lower edge.
    umax r = 0;
    if constexpr (p != 1)
        for (std::size_t i = N; i-- > 0;) {
            const limb::pair<umax> d = limb::div(r, hl[i], p);
            hl[i]                    = d.Hi;
            r                        = d.Lo;
        }
    if (!(delta < p - r))
        return false;
    if (r == 0 && !low_nonzero)
        return false;
    // Strictly inside: ⌊u/D⌋ = B and ⌈u/D⌉ = B + 1. Rounding the magnitude
    // up is ceil for a positive value and floor for a negative one. Then
    // the offset ±B − base, on the limbs.
    umax up = 0;
    if constexpr (!nearest && M != round_mode::trunc)
        up = (M == round_mode::ceil) != neg ? 1 : 0;
    constexpr J base  = static_cast<J>(slot_base<Out>);
    umax        carry = up, sign_borrow = 0, borrow = 0;
    for (std::size_t i = 0; i < N + 1; ++i) {
        umax w = limb::add_carry(i < N ? hl[i] : umax{0}, umax{0}, carry); // B (+1)
        if (neg)
            w = limb::sub_borrow(umax{0}, w, sign_borrow);
        index.Word[i] = limb::sub_borrow(w, static_cast<umax>(base.Word[i]), borrow);
    }
    return true;
}

template <insidable Out, std::size_t K>
constexpr decision<Out, K> decide(const approx<K>& a) noexcept {
    constexpr round_mode M = out_rounding<Out>;
    using I                = wide_sint<decide_limbs<Out, K>>;
    const wide_sint<K> e{a.Error};
    auto               index = [&](const wide_sint<K>& y) -> I {
        if constexpr (notch_fits64<Out>)
            if (a.Scale >= 1)
                return static_cast<I>(fast_index<Out, M>(y, a.Scale));
        return static_cast<I>(exact_index<Out, M>(scaled_value<Out>(y, a.Scale)).Index);
    };
    if (a.Error == 0)
        return {true, index(a.Value)};
    const I lo = index(a.Value - e), hi = index(a.Value + e);
    // Both ends past the same end of Out's range also decide: the policy
    // (clamp, or an overflow report) sees the same thing for every value there.
    constexpr I count = static_cast<I>(grid_of<Out>.slot_count());
    const bool  below = lo.negative() && hi.negative();
    const bool  above = count < lo && count < hi;
    return {lo == hi || below || above, lo};
}

// decide_fast where it applies: the slot in K + 2 limbs, which store takes
// directly (decide's index is sized for exact_index's worst case).
template <insidable Out, std::size_t K>
constexpr bool quick_slot(const approx<K>& a, wide_sint<K + 2>& index) noexcept {
    if constexpr (notch_fits64<Out>)
        return a.Scale >= 1 && a.Error != 0 && decide_fast<Out, out_rounding<Out>>(a.Value, a.Error, a.Scale, index);
    else
        return false;
}

// The slot nearest the approx's midpoint under Out's rounding (the capped
// escalation's answer).
template <insidable Out, std::size_t K>
constexpr auto nearest_index(const approx<K>& a) noexcept {
    using I = wide_sint<decide_limbs<Out, K>>;
    return static_cast<I>(exact_index<Out, out_rounding<Out>>(scaled_value<Out>(a.Value, a.Scale)).Index);
}

//---------------------------------------------------------------------------
// store — Out at slot offset `index`. In range it is a raw; out of range the
// grid point goes through the policy cascade (clamp, wrap or report), as any
// assignment. Policy is Out's, optionally with an errc sink.
//---------------------------------------------------------------------------
template <insidable Out, std::size_t K, typename P>
constexpr Out store(const wide_sint<K>& index, P&& policy) {
    using I = wide_sint<K>;
    if constexpr (integer_storage<Out>) {
        constexpr I count = static_cast<I>(grid_of<Out>.slot_count());
        if (!index.negative() && !(count < index)) [[likely]]
            return Out::from_raw(raw_of_slot<Out>(index));
    } else if constexpr (fp_storage<Out> && !wide_valued<Out>) {
        // A floating-point raw holds the grid point itself: on its double- (or
        // float-) exact grid, value index × notch is exact in that type.
        constexpr I count = static_cast<I>(grid_of<Out>.slot_count());
        if (!index.negative() && !(count < index)) [[likely]] {
            constexpr double notch = static_cast<double>(static_cast<imax>(wide_numerator(notch_of<Out>))) /
                                     static_cast<double>(static_cast<imax>(wide_denominator(notch_of<Out>)));
            const imax j = static_cast<imax>(index) + static_cast<imax>(slot_base<Out>);
            return Out::from_raw(static_cast<raw_t<Out>>(static_cast<double>(j) * notch));
        }
    } else if constexpr (rational_storage<Out> && notch_fits64<Out>) {
        // A rational raw holds the grid point j·p/q itself: built directly
        // (the constructor reduces it) when j·p fits 64 bits.
        constexpr I    count = static_cast<I>(grid_of<Out>.slot_count());
        constexpr imax p     = static_cast<imax>(wide_numerator(notch_of<Out>));
        constexpr imax q     = static_cast<imax>(wide_denominator(notch_of<Out>));
        if (!index.negative() && !(count < index) && bit_width_of(index) < 63) [[likely]] {
            imax j = 0, num = 0;
            if (!__builtin_add_overflow(static_cast<imax>(index), static_cast<imax>(slot_base<Out>), &j) &&
                !__builtin_mul_overflow(j, p, &num))
                return Out::from_raw(rational{num, q});
        }
    }
    // The grid point (index + Lower/Notch)·Notch, exact: stored through Out's
    // assignment (a floating-point or rational raw holds it exactly).
    constexpr std::size_t KK = exact_max<K, exact_limbs<Out>>;
    using J                  = wide_sint<KK>;
    const exact_frac<KK> v{(J{index} + static_cast<J>(slot_base<Out>)) * static_cast<J>(wide_numerator(notch_of<Out>)),
                           static_cast<J>(wide_denominator(notch_of<Out>))};
    Out                  out{};
    assign_exact<rational>(out, v, policy, no_action{});
    return out;
}

template <insidable Out, std::size_t K>
constexpr Out store(const wide_sint<K>& index) {
    return store<Out>(index, make_policy<policy_of<Out>>());
}

// An exact value through Out's policy (an output without slots, or an exact
// result: rounding an exact value by the mode is correct rounding).
template <insidable Out, std::size_t K, typename P>
constexpr Out store_exact(const exact_frac<K>& v, P&& policy) {
    // At least the exact paths' minimum width (try_rational's 64-bit bounds
    // need it).
    const exact_frac<exact_max<K, exact_min_limbs>> w{v};
    Out                                             out{};
    assign_exact<rational>(out, w, policy, no_action{});
    return out;
}

// An approx's midpoint as an exact value.
template <std::size_t K>
constexpr auto midpoint(const approx<K>& a) noexcept {
    constexpr std::size_t KK = exact_max<K + 1, exact_min_limbs>;
    using I                  = wide_sint<KK + 8>;
    return a.Scale >= 0 ? exact_frac<KK + 8>{I{a.Value}, I{1} << a.Scale}
                        : exact_frac<KK + 8>{I{a.Value} << (-a.Scale), I{1}};
}

//---------------------------------------------------------------------------
// evaluate — the Ziv driver. Core is a callable object with a member
// template `run<W>()` returning an approx within about 2^-W, and optionally
// `exact()` returning a std::optional exact_frac when the result is an
// exact rational (taken as is). It starts at W0 and doubles W while the
// result is undecided, up to Cap.
//---------------------------------------------------------------------------
template <insidable Out, int W, int Cap, typename Core, typename P>
constexpr Out evaluate_from(const Core& core, P&& policy) {
    const auto a = core.template run<W>();
    if constexpr (!notched<Out>) {
        // A 64-bit value: keep the denominator a 64-bit power of two.
        constexpr int A = 60 - mag_bits<Out> > 1 ? 60 - mag_bits<Out> : 1;
        if (a.Scale <= A)
            return store_exact<Out>(midpoint(a), policy);
        using I        = decltype(a.Value);
        const int sh   = a.Scale - A;
        const I   half = I{1} << (sh - 1);
        const I   v    = a.Value.negative() ? -((-a.Value + half) >> sh) : (a.Value + half) >> sh;
        return store_exact<Out>(midpoint(approx<limbs_of<I>>{v, A, 0}), policy);
    } else {
        if (wide_sint<limbs_of<decltype(a.Value)> + 2> j; quick_slot<Out>(a, j)) [[likely]]
            return store<Out>(j, policy);
        const auto d = decide<Out>(a);
        if (d.Decided)
            return store<Out>(d.Index, policy);
        if constexpr (2 * W > Cap)
            return store<Out>(nearest_index<Out>(a), policy);
        else
            return evaluate_from<Out, 2 * W, Cap>(core, policy);
    }
}

// The precision cap for a start precision W0: two doublings. A result still
// undecided at 4·W0 lies within 2^-4W0 of a rounding boundary — for an
// irrational value that is the rare case the cap gives up on (nearest slot).
constexpr int precision_cap(int W0) noexcept { return 4 * W0; }

template <insidable Out, int W0, typename Core, typename P>
constexpr Out evaluate(const Core& core, P&& policy) {
    if constexpr (requires { core.exact(); })
        if (const auto e = core.exact())
            return store_exact<Out>(*e, policy);
    return evaluate_from<Out, W0, precision_cap(W0)>(core, policy);
}

//---------------------------------------------------------------------------
// The table tier: for an input with few slots, every result is computed at
// compile time (the same driver, so the same correctly rounded slots) and a
// call is one load. Only when every result lands inside Out's range — a
// result the policy would clamp, wrap or report keeps the computed path.
// BEMAN_INSIDE_MATH_TABLE_SLOTS sets the largest input (0 turns tables off).
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_MATH_TABLE_SLOTS
    #define BEMAN_INSIDE_MATH_TABLE_SLOTS 256
#endif

// Out's slot offset for core's result (escalating as evaluate does), or
// −1 when it falls outside Out's range.
template <insidable Out, int W, int Cap, typename Core>
constexpr imax slot_from(const Core& core) {
    const auto a        = core.template run<W>();
    auto       in_range = [](const auto& i) -> imax {
        using I           = std::remove_cvref_t<decltype(i)>;
        constexpr I count = static_cast<I>(grid_of<Out>.slot_count());
        return (i.negative() || count < i) ? imax{-1} : static_cast<imax>(i);
    };
    if (wide_sint<limbs_of<decltype(a.Value)> + 2> j; quick_slot<Out>(a, j))
        return in_range(j);
    const auto d = decide<Out>(a);
    if constexpr (2 * W <= Cap)
        if (!d.Decided)
            return slot_from<Out, 2 * W, Cap>(core);
    return in_range(d.Decided ? d.Index : nearest_index<Out>(a));
}

template <insidable Out, int W0, typename Core>
constexpr imax slot_of(const Core& core) {
    if constexpr (requires { core.exact(); })
        if (const auto e = core.exact()) {
            const auto i      = exact_index<Out, out_rounding<Out>>(*e).Index;
            using I           = std::remove_cvref_t<decltype(i)>;
            constexpr I count = static_cast<I>(grid_of<Out>.slot_count());
            return (i.negative() || count < i) ? imax{-1} : static_cast<imax>(i);
        }
    return slot_from<Out, W0, precision_cap(W0)>(core);
}

template <insidable In>
inline constexpr bool table_input =
    !wide_valued<In> && notched<In> && grid_of<In>.slot_count() < grid_wide{BEMAN_INSIDE_MATH_TABLE_SLOTS};

// Outputs whose raw a table can hold: an integer index or value, or a
// floating-point raw (the grid point as a double or float).
template <insidable Out>
inline constexpr bool table_output = (integer_storage<Out> || fp_storage<Out>) && notched<Out> && !wide_valued<Out>;

// The slot offset of an input value (0 … slot count).
template <insidable In>
constexpr std::size_t offset_of(const In& x) noexcept {
    if constexpr (rational_storage<In>) {
        // A rational raw holds the value r, a multiple of the notch n: its den
        // divides n's, so r/n = num(r)·(den(n)/den(r))/num(n) exactly.
        const rational     r  = x.raw();
        constexpr rational n  = to_rational(notch_of<In>);
        const imax         rd = r.Denominator < 0 ? -r.Denominator : r.Denominator;
        const __int128     rn =
            r.Denominator < 0 ? -static_cast<__int128>(r.Numerator) : static_cast<__int128>(r.Numerator);
        const __int128 j = rn * (n.Denominator / rd) / static_cast<__int128>(n.Numerator);
        return static_cast<std::size_t>(static_cast<imax>(j) - static_cast<imax>(slot_base<In>));
    } else if constexpr (fp_storage<In>) {
        // A floating-point raw holds the value on a dyadic grid: (value −
        // Lower)·2^k is exact.
        constexpr double lower = static_cast<double>(to_rational(lower_of<In>));
        constexpr double inv   = static_cast<double>(to_rational(grid_rational{1} / notch_of<In>));
        return static_cast<std::size_t>((static_cast<double>(x.raw()) - lower) * inv);
    } else
        return static_cast<std::size_t>(value_index<imax>(x) - static_cast<imax>(slot_base<In>));
}

// In's value at slot I (rational and floating-point raws hold the value).
template <insidable In>
constexpr In slot_input(std::size_t i) noexcept {
    if constexpr (rational_storage<In>)
        return In::from_raw(to_rational(lower_of<In>) + rational{static_cast<imax>(i)} * to_rational(notch_of<In>));
    else if constexpr (fp_storage<In>) // the value, exact on the dyadic grid
        return In::from_raw(
            static_cast<raw_t<In>>(static_cast<double>(to_rational(lower_of<In>)) +
                                   static_cast<double>(i) * static_cast<double>(to_rational(notch_of<In>))));
    else
        return In::from_raw(raw_from_offset<In>(static_cast<umax>(i)));
}

// Out's slot for In's slot I, or −1 past Out's range: one constant
// expression per entry, so each has the compiler's whole evaluation budget.
template <insidable Out, insidable In, int W0, auto MakeCore, std::size_t I>
inline constexpr imax table_slot = slot_of<Out, W0>(MakeCore(slot_input<In>(I)));

// Out's raw for slot offset I, as store gives it to every other tier.
template <insidable Out>
constexpr raw_t<Out> table_raw(imax i) noexcept {
    if constexpr (integer_storage<Out>)
        return raw_from_offset<Out>(static_cast<umax>(i));
    else
        return store<Out>(wide_sint<2>{static_cast<umax>(i)}).raw();
}

// Out's raws for every In slot; Valid when all lie in Out's range. MakeCore
// builds the core from an In value.
template <insidable Out, insidable In, int W0, auto MakeCore>
struct result_table {
    static constexpr std::size_t N = static_cast<std::size_t>(static_cast<imax>(grid_of<In>.slot_count())) + 1;
    struct data {
        std::array<raw_t<Out>, N> Raw;
        bool                      Valid;
    };
    static constexpr data Table = []<std::size_t... I>(std::index_sequence<I...>) {
        constexpr bool valid = ((table_slot<Out, In, W0, MakeCore, I> >= 0) && ...);
        if constexpr (!valid)
            return data{{}, false};
        else
            return data{{table_raw<Out>(table_slot<Out, In, W0, MakeCore, I>)...}, true};
    }(std::make_index_sequence<N>{});
};

template <insidable Out, int W0, typename Core>
constexpr Out evaluate(const Core& core) {
    return evaluate<Out, W0>(core, make_policy<policy_of<Out>>());
}

// The checked form: a result Out's policy reports (out of range without
// clamp) comes back as its errc instead.
template <insidable Out, int W0, typename Core>
constexpr std::expected<Out, errc> evaluate_checked(const Core& core) {
    errc      ec{};
    const Out out = evaluate<Out, W0>(core, make_policy<policy_of<Out>>(ec));
    if (ec != errc{})
        return std::unexpected{ec};
    return out;
}

//---------------------------------------------------------------------------
// Constants at any precision, computed at compile time with wide integer
// series. Each is within 1 unit 2^-W of the true value.
//---------------------------------------------------------------------------
// Guard bits for a series of about W/2 truncated terms.
constexpr int series_guard(int W) noexcept { return std::bit_width(static_cast<unsigned>(W)) + 4; }

// Σ_k s^k / ((2k+1)·n^(2k+1)) at scale 2^G, with s = +1 (atanh(1/n)) or
// −1 (atan(1/n)). Each term truncates once (p) and once (p/(2k+1)).
template <std::size_t K>
constexpr wide_sint<K> inverse_series(int n, int sign, int G) noexcept {
    using I = wide_sint<K>;
    I p     = div_small(I{1} << G, static_cast<umax>(n)); // 2^G / n^(2k+1)
    I sum{0};
    for (int k = 0; !p.is_zero(); ++k) {
        const I t = div_small(p, static_cast<umax>(2 * k + 1));
        sum       = (k % 2 != 0 && sign < 0) ? sum - t : sum + t;
        p         = div_small(p, static_cast<umax>(n) * static_cast<umax>(n));
    }
    return sum;
}

// ln 2 = 2·atanh(1/3).
template <int W>
inline constexpr fixed_t<W + 2> ln2_q = [] {
    constexpr int G = W + series_guard(W);
    using I         = fixed_t<G + 4>;
    const I v       = I{2} * inverse_series<limbs_of<I>>(3, +1, G);
    return static_cast<fixed_t<W + 2>>(round_shift(v, G - W));
}();

// π = 16·atan(1/5) − 4·atan(1/239) (Machin).
template <int W>
inline constexpr fixed_t<W + 3> pi_q = [] {
    constexpr int G = W + series_guard(W) + 4;
    using I         = fixed_t<G + 6>;
    const I v       = I{16} * inverse_series<limbs_of<I>>(5, -1, G) - I{4} * inverse_series<limbs_of<I>>(239, -1, G);
    return static_cast<fixed_t<W + 3>>(round_shift(v, G - W));
}();

// ln 10 = 3·ln 2 + ln(5/4) = 6·atanh(1/3) + 2·atanh(1/9).
template <int W>
inline constexpr fixed_t<W + 4> ln10_q = [] {
    constexpr int G = W + series_guard(W) + 2;
    using I         = fixed_t<G + 6>;
    const I v       = I{6} * inverse_series<limbs_of<I>>(3, +1, G) + I{2} * inverse_series<limbs_of<I>>(9, +1, G);
    return static_cast<fixed_t<W + 4>>(round_shift(v, G - W));
}();

//---------------------------------------------------------------------------
// Exact integer helpers.
//---------------------------------------------------------------------------
// ⌊√t⌋ and ⌊∛t⌋ for one limb: Newton from above.
constexpr umax isqrt64(umax t) noexcept {
    if (t < 2)
        return t;
    umax x = umax{1} << ((std::bit_width(t) + 1) / 2);
    for (;;) {
        const umax y = (x + t / x) >> 1;
        if (y >= x)
            return x;
        x = y;
    }
}
// ⌊∛t⌋ digit by digit, no division (Hacker's Delight 11-2): each step
// brings down three bits and appends one bit to the root.
constexpr umax icbrt64(umax t) noexcept {
    umax y = 0;
    for (int s = 63; s >= 0; s -= 3) {
        y <<= 1;
        const umax b = 3 * y * (y + 1) + 1;
        if ((t >> s) >= b) {
            t -= b << s;
            y += 1;
        }
    }
    return y;
}

// ⌊√t⌋ for two limbs: Newton from above on 128-bit words, seeded from the
// top 62 bits; each quotient t/x ≤ √t fits one limb.
constexpr umax isqrt128(unsigned __int128 t) noexcept {
    if ((t >> 64) == 0)
        return isqrt64(static_cast<umax>(t));
    const int         bw = 128 - std::countl_zero(static_cast<umax>(t >> 64));
    const int         sh = (bw - 62 + 1) & ~1; // even, leaves 61 or 62 bits
    unsigned __int128 x  = static_cast<unsigned __int128>(isqrt64(static_cast<umax>(t >> sh)) + 1) << (sh / 2);
    for (;;) {
        const unsigned __int128 y = (x + t / x) >> 1;
        if (!(y < x))
            return static_cast<umax>(x);
        x = y;
    }
}

// ⌊√n⌋ for n ≥ 0. Two limbs or fewer directly; wider, the root of the top
// 126 bits (an even shift) seeds Newton from above with about 62 correct
// bits, each step doubling them; once they cover the root, squares
// correct the last unit (Newton from above never lands below ⌊√n⌋).
template <std::size_t K>
constexpr wide_sint<K> isqrt(const wide_sint<K>& n) noexcept {
    using I = wide_sint<K>;
    if (n.is_zero())
        return n;
    const int bw = bit_width_of(n);
    if (bw <= 64)
        return I{isqrt64(static_cast<umax>(n))};
    if constexpr (K >= 2) {
        const int               sh  = bw > 126 ? (bw - 126 + 1) & ~1 : 0;
        const I                 top = n >> sh;
        const unsigned __int128 t =
            (static_cast<unsigned __int128>(static_cast<umax>(top.Word[1])) << 64) | static_cast<umax>(top.Word[0]);
        const umax r = isqrt128(t);
        if (sh == 0)
            return I{r};
        I x = I{r} + I{1};
        x   = x << (sh / 2); // ≥ √n, within 2^-61 of it
        for (int bits = 61; bits < bw / 2 + 2; bits *= 2)
            x = (x + n / x) >> 1;
        while (n < x * x)
            x -= I{1};
        return x;
    } else
        return I{isqrt64(static_cast<umax>(n))};
}

// ⌊∛t⌋ for two limbs: Newton from above on 128-bit words, seeded from the
// top 63 bits.
constexpr umax icbrt128(unsigned __int128 t) noexcept {
    if ((t >> 64) == 0)
        return icbrt64(static_cast<umax>(t));
    const int         bw = 128 - std::countl_zero(static_cast<umax>(t >> 64));
    const int         sh = (bw - 63 + 2) / 3 * 3; // a multiple of 3, leaves at most 63 bits
    unsigned __int128 x  = static_cast<unsigned __int128>(icbrt64(static_cast<umax>(t >> sh)) + 1) << (sh / 3);
    for (;;) {
        const unsigned __int128 y = (2 * x + t / (x * x)) / 3;
        if (!(y < x))
            return static_cast<umax>(x);
        x = y;
    }
}

// ⌊∛n⌋ for n ≥ 0, as isqrt: two limbs or fewer directly; wider, the root
// of the top 126 bits (a shift by a multiple of 3) seeds Newton from above
// with about 41 correct bits, and cubes correct the last unit.
template <std::size_t K>
constexpr wide_sint<K> icbrt(const wide_sint<K>& n) noexcept {
    using I = wide_sint<K>;
    if (n.is_zero())
        return n;
    const int bw = bit_width_of(n);
    if (bw <= 64)
        return I{icbrt64(static_cast<umax>(n))};
    if constexpr (K >= 2) {
        const int               sh  = bw > 126 ? (bw - 126 + 2) / 3 * 3 : 0;
        const I                 top = n >> sh;
        const unsigned __int128 t =
            (static_cast<unsigned __int128>(static_cast<umax>(top.Word[1])) << 64) | static_cast<umax>(top.Word[0]);
        const umax r = icbrt128(t);
        if (sh == 0)
            return I{r};
        I x = I{r} + I{1};
        x   = x << (sh / 3); // ≥ ∛n, within 2^-40 of it
        for (int bits = 40; bits < bw / 3 + 2; bits *= 2)
            x = (I{2} * x + n / (x * x)) / I{3};
        while (n < x * x * x)
            x -= I{1};
        return x;
    } else
        return I{icbrt64(static_cast<umax>(n))};
}

template <std::size_t K>
constexpr bool is_zero(const exact_frac<K>& f) noexcept {
    return f.Num.is_zero();
}
template <std::size_t K>
constexpr bool is_one(const exact_frac<K>& f) noexcept {
    return f.Num == f.Den;
}
template <std::size_t K>
constexpr bool is_integer(const exact_frac<K>& f) noexcept {
    return (f.Num % f.Den).is_zero();
}
template <std::size_t K>
constexpr exact_frac<K> abs(const exact_frac<K>& f) noexcept {
    return {f.Num.negative() ? -f.Num : f.Num, f.Den};
}
// 1/f for f != 0.
template <std::size_t K>
constexpr exact_frac<K> inverse(const exact_frac<K>& f) noexcept {
    return f.Num.negative() ? exact_frac<K>{-f.Den, -f.Num} : exact_frac<K>{f.Den, f.Num};
}

// x·2^W rounded to the nearest integer, for a W known only at runtime. KI
// limbs must hold Num·2^W and Den·2^-W.
template <std::size_t K, std::size_t KI, std::size_t E>
constexpr wide_sint<K> to_q_at(const exact_frac<E>& x, int W) noexcept {
    using I = wide_sint<KI>;
    I n{x.Num}, d{x.Den};
    if (W >= 0)
        n = n << W;
    else
        d = d << (-W);
    return static_cast<wide_sint<K>>(nearest_div(n, d));
}

//---------------------------------------------------------------------------
// Polynomial kernels on Q.S values: Taylor polynomials in Horner form with
// coefficients rounded to scale S at compile time (no divisions at
// runtime). Each returns the value and an error bound in units 2^-S: the
// coefficients are within ½ unit, each Horner step truncates once, and the
// argument's own error passes through the derivative; the bounds below
// round those sums up generously.
//---------------------------------------------------------------------------
template <std::size_t K>
struct fx {
    wide_sint<K> Value;
    umax         Error;
};

// A positive double as mantissa·2^exponent, so a term bound far below the
// double range (2^-S for S in the thousands) can still be compared.
struct scaled_bound {
    double         Mant = 1;
    int            Exp  = 0;
    constexpr void mul(double f) noexcept {
        Mant *= f;
        while (Mant < 0.5) {
            Mant *= 2;
            --Exp;
        }
        while (Mant >= 1) {
            Mant /= 2;
            ++Exp;
        }
    }
    constexpr bool below(int e) const noexcept { return Exp < e; } // < 2^e
};

// Terms for Σ_k x^(Step·k + Off)/Den(k) with |x| ≤ R to reach 2^-(S+3):
// the first omitted term is below that, and so is the rest (each term is
// at most half the one before it).
enum class series { exp, sin, cos, atanh, sinh, cosh, log1p };

template <series Kind, int S>
inline constexpr int series_terms = [] {
    constexpr double R = (Kind == series::exp || Kind == series::sinh || Kind == series::cosh) ? 0.36
                         : Kind == series::atanh                                               ? 0.18
                         : Kind == series::log1p                                               ? 0.025
                                                                                               : 0.8;
    int              n = 0;
    for (;; ++n) {
        // the term of index n + 1 (the first one left out)
        const int    k = n + 1;
        scaled_bound u;
        if constexpr (Kind == series::exp)
            for (int i = 1; i <= k; ++i)
                u.mul(R / i);
        else if constexpr (Kind == series::sin || Kind == series::sinh)
            for (int i = 1; i <= 2 * k + 1; ++i)
                u.mul(R / i);
        else if constexpr (Kind == series::cos || Kind == series::cosh)
            for (int i = 1; i <= 2 * k; ++i)
                u.mul(R / i);
        else if constexpr (Kind == series::log1p) {
            for (int i = 0; i < k + 1; ++i)
                u.mul(R);
            u.mul(1.0 / (k + 1));
        } else {
            for (int i = 0; i < 2 * k + 1; ++i)
                u.mul(R);
            u.mul(1.0 / (2 * k + 1));
        }
        if (u.below(-(S + 3)))
            return n;
    }
}();

// Coefficients at scale S, within ½ unit: exp 1/k!, sin (−1)^k/(2k+1)!,
// cos (−1)^k/(2k)!, sinh 1/(2k+1)!, cosh 1/(2k)!, atanh 1/(2k+1),
// atan (−1)^k/(2k+1), log1p (−1)^k/(k+1).
template <series Kind, int S, bool Alternate = false>
inline constexpr auto series_coef = [] {
    constexpr int N = series_terms<Kind, S>;
    constexpr int G = 16; // guard bits for the running quotient
    using I         = fixed_t<S + G + 2>;
    std::array<fixed_t<S + 2>, N + 1> c{};
    I                                 v = I{1} << (S + G);
    for (int k = 0; k <= N; ++k) {
        if constexpr (Kind == series::atanh)
            v = div_small(I{1} << (S + G), static_cast<umax>(2 * k + 1));
        else if constexpr (Kind == series::log1p)
            v = div_small(I{1} << (S + G), static_cast<umax>(k + 1));
        else if (k > 0) {
            const int d = Kind == series::exp                             ? k
                          : (Kind == series::sin || Kind == series::sinh) ? (2 * k) * (2 * k + 1)
                                                                          : (2 * k - 1) * (2 * k);
            v           = div_small(v, static_cast<umax>(d));
        }
        const bool neg =
            (Kind == series::sin || Kind == series::cos || Kind == series::log1p || Alternate) && k % 2 != 0;
        const auto r                   = static_cast<fixed_t<S + 2>>(round_shift(v, G));
        c[static_cast<std::size_t>(k)] = neg ? -r : r;
    }
    return c;
}();

// Σ c_k x^k by Horner at scale S.
template <series Kind, int S, bool Alternate, std::size_t K>
constexpr wide_sint<K> horner(const wide_sint<K>& x) noexcept {
    constexpr auto& c = series_coef<Kind, S, Alternate>;
    wide_sint<K>    p = static_cast<wide_sint<K>>(c[c.size() - 1]);
    for (std::size_t k = c.size() - 1; k-- > 0;)
        p = static_cast<wide_sint<K>>(c[k]) + mul_q(p, x, S);
    return p;
}

// e^r for |r| ≤ 0.36, r within dr units.
template <int S, std::size_t K>
constexpr fx<K> exp_series(const wide_sint<K>& r, umax dr) noexcept {
    return {horner<series::exp, S, false>(r), 2 * dr + 8};
}

// sin r and cos r for |r| ≤ 0.8, r within dr units.
template <int S, std::size_t K>
constexpr fx<K> sin_series(const wide_sint<K>& r, umax dr) noexcept {
    const wide_sint<K> z = mul_q(r, r, S);
    return {mul_q(r, horner<series::sin, S, false>(z), S), 3 * dr + 12};
}
template <int S, std::size_t K>
constexpr fx<K> cos_series(const wide_sint<K>& r, umax dr) noexcept {
    return {horner<series::cos, S, false>(mul_q(r, r, S)), 4 * dr + 12};
}

// log(j/32) and 32/j for j = 22 … 46 at scale S (within 1 and ½ unit).
inline constexpr int log_tab_lo = 22, log_tab_hi = 46;

template <int S>
inline constexpr auto log_table = [] {
    constexpr int T = S + 16;
    using I         = fixed_t<T + 8>;
    struct entry {
        fixed_t<S + 2> Log;
        fixed_t<S + 8> Inv;
    };
    std::array<entry, log_tab_hi - log_tab_lo + 1> t{};
    for (int j = log_tab_lo; j <= log_tab_hi; ++j) {
        // log(j/32) = 2·atanh(z), z = (j − 32)/(j + 32), |z| ≤ 0.19.
        const I z  = div_small(I{j - 32} << T, static_cast<umax>(j + 32));
        const I z2 = mul_q(z, z, T);
        I       p  = z, sum{0};
        for (int k = 0; !p.is_zero(); ++k) {
            sum += div_small(p, static_cast<umax>(2 * k + 1));
            p = mul_q(p, z2, T);
        }
        auto& e = t[static_cast<std::size_t>(j - log_tab_lo)];
        e.Log   = static_cast<fixed_t<S + 2>>(round_shift(sum << 1, T - S));
        e.Inv   = static_cast<fixed_t<S + 8>>(round_shift(div_small(I{32} << T, static_cast<umax>(j)), T - S));
    }
    return t;
}();

// log m for m in [0.7, 1.42], m within dm units, with no division: j =
// round(32m), u = m·(32/j) − 1 (|u| ≤ 1/44), log m = log(j/32) + log(1 + u).
template <int S, std::size_t K>
constexpr fx<K> log_series(const wide_sint<K>& m, umax dm) noexcept {
    using I       = wide_sint<K>;
    int j         = static_cast<int>(static_cast<imax>(round_shift(m, S - 5)));
    j             = j < log_tab_lo ? log_tab_lo : j > log_tab_hi ? log_tab_hi : j;
    const auto& e = log_table<S>[static_cast<std::size_t>(j - log_tab_lo)];
    const I     u = mul_q(m, static_cast<I>(e.Inv), S) - one_q<K>(S); // within 1.5·dm + 2
    const I     l = mul_q(u, horner<series::log1p, S, false>(u), S);
    return {static_cast<I>(e.Log) + l, 3 * dm + 12};
}

// atan u for |u| ≤ 1/16 (the atanh table, sized for 0.18, with
// alternating signs), u within du units.
template <int S, std::size_t K>
constexpr fx<K> atan_series(const wide_sint<K>& u, umax du) noexcept {
    return {mul_q(u, horner<series::atanh, S, true>(mul_q(u, u, S)), S), du + 6};
}

// √a for a ≥ 0 at scale S: ⌊√(a·2^S)⌋; an a within da units gives a root
// within da/2 + 1 units (a ≥ 1/4).
template <std::size_t K>
constexpr wide_sint<K> sqrt_q(const wide_sint<K>& a, int S) noexcept {
    using D = double_t<K>;
    return static_cast<wide_sint<K>>(isqrt(D{a} << S));
}

// 1/√m for m in [1/4, 4) at scale S, within 6 units, by multiplications
// only: a 29-bit seed from the top bits (isqrt64), then Newton steps
// y ← y·(3 − m·y²)/2, each doubling the correct bits.
template <int S, std::size_t K>
constexpr wide_sint<K> rsqrt_q(const wide_sint<K>& m) noexcept {
    using R = fixed_t<S + 34>;
    const R       mr{m};
    const umax    mm    = static_cast<umax>(S >= 60 ? mr >> (S - 60) : mr << (60 - S)); // m·2^60 < 2^62
    const umax    sd    = isqrt64(mm);                                                  // √m·2^30, ≥ 2^29
    R             y     = div_small(R{1} << (S + 30), sd);
    const R       three = R{3} << S;
    constexpr int steps = [] {
        int n = 0;
        for (int p = 29; p < S + 4; p *= 2)
            ++n;
        return n;
    }();
    for (int i = 0; i < steps; ++i)
        y = mul_q(y, three - mul_q(mr, mul_q(y, y, S), S), S) >> 1;
    return static_cast<wide_sint<K>>(y);
}

// atan(j/8) for j = 0 … 8 at scale S, within 1 unit: two half-angle steps
// t ← t/(1 + √(1+t²)) bring j/8 below 1/4, then the series with no table.
template <int S>
inline constexpr auto atan_eighths = [] {
    constexpr int T                 = S + 16;
    using I                         = fixed_t<T + 8>;
    constexpr std::size_t         K = limbs_of<I>;
    std::array<fixed_t<S + 2>, 9> a{};
    const I                       one = one_q<K>(T);
    for (int j = 0; j <= 8; ++j) {
        I t = (I{j} << T) / I{8};
        for (int i = 0; i < 3; ++i)
            t = div_q(t, one + sqrt_q(one + mul_q(t, t, T), T), T);
        // |t| ≤ tan(π/32) < 0.1; a plain alternating series.
        const I t2 = mul_q(t, t, T);
        I       p  = t, sum{0};
        for (int k = 0; !p.is_zero(); ++k) {
            const I term = div_small(p, static_cast<umax>(2 * k + 1));
            sum          = (k % 2 == 0) ? sum + term : sum - term;
            p            = mul_q(p, t2, T);
        }
        a[static_cast<std::size_t>(j)] = static_cast<fixed_t<S + 2>>(round_shift(sum << 3, T - S));
    }
    return a;
}();

// atan t for |t| ≤ 1, t within dt units: c = j/8 nearest t, then
// atan t = atan c + atan((t − c)/(1 + t·c)) with |(t − c)/(1 + t·c)| ≤ 1/16.
template <int S, std::size_t K>
constexpr fx<K> atan_fixed(const wide_sint<K>& t, umax dt) noexcept {
    using I          = wide_sint<K>;
    const I     one  = one_q<K>(S);
    const I     j    = round_shift(t, S - 3); // round(8t), in [−8, 8]
    const int   jj   = static_cast<int>(static_cast<imax>(j));
    const I     c    = j << (S - 3);                          // exact
    const I     u    = div_q(t - c, one + mul_q(t, c, S), S); // within dt + 2
    const fx<K> a    = atan_series<S>(u, dt + 2);
    const I     base = static_cast<I>(atan_eighths<S>[static_cast<std::size_t>(jj < 0 ? -jj : jj)]);
    return {(jj < 0 ? -base : base) + a.Value, a.Error + 1};
}

// log a for a ≥ 1 at scale S (so b ≥ 0), a within da units: a = m·2^b with
// m in [0.7, 1.42], log a = log m + b·ln 2.
template <int S, std::size_t K>
constexpr fx<K> log_fixed(const wide_sint<K>& a, umax da) noexcept {
    using I = wide_sint<K>;
    int b   = bit_width_of(a) - 1 - S; // 2^b ≤ a/2^S < 2^(b+1)
    I   m   = b >= 0 ? a >> b : a << (-b);
    if (mul_q(m, m, S) > (one_q<K>(S) << 1)) {
        ++b;
        m = b >= 0 ? a >> b : a << (-b);
    }
    const umax  dm = shr_bound(da, b) + 1;
    const fx<K> l  = log_series<S>(m, dm);
    return {l.Value + I{b} * static_cast<I>(ln2_q<S>), l.Error + static_cast<umax>(b < 0 ? -b : b) + 1};
}

// 1/ln 2 and 2/π at scale S (within 1 unit), for range reduction by a
// product.
template <int S>
inline constexpr fixed_t<S + 2> log2e_q = [] {
    using I = fixed_t<2 * S + 24>;
    return static_cast<fixed_t<S + 2>>((I{1} << (2 * S + 16)) / static_cast<I>(ln2_q<S + 16>));
}();
template <int S>
inline constexpr fixed_t<S + 2> log10e_q = [] {
    using I = fixed_t<2 * S + 24>;
    return static_cast<fixed_t<S + 2>>((I{1} << (2 * S + 16)) / static_cast<I>(ln10_q<S + 16>));
}();
template <int S>
inline constexpr fixed_t<S + 2> two_over_pi_q = [] {
    using I = fixed_t<2 * S + 24>;
    return static_cast<fixed_t<S + 2>>((I{1} << (2 * S + 17)) / static_cast<I>(pi_q<S + 16>));
}();

// e^t for t at scale S, within dt units: e^r·2^k for t = k·ln 2 + r, as an
// approx at scale S − k. k comes from t·(1/ln 2), so |r| ≤ ln 2/2 plus a
// hair (the series covers 0.36). Past KMax the value lies above 2^(KMax+1):
// returned as exactly 2^(KMax + 2), which every output with magnitude below
// 2^KMax places past its Upper. Below 2^-(S+1) it is returned as the
// interval [0, 2^-(S+1)].
template <int S, std::size_t K>
constexpr approx<K> exp_fixed(const wide_sint<K>& t, umax dt, int KMax) noexcept {
    using I   = wide_sint<K>;
    const I k = round_shift(mul_q(t, static_cast<I>(log2e_q<S>), S), S);
    if (I{KMax} < k)
        return {I{1}, -(KMax + 2), 0};
    if (k < I{-(S + 2)})
        return {I{1}, S + 2, 1};
    const fx<K> e  = exp_series<S>(t - k * static_cast<I>(ln2_q<S>), 0);
    const int   kk = static_cast<int>(static_cast<imax>(k));
    const umax  ak = static_cast<umax>(kk < 0 ? -kk : kk);
    return {e.Value, S - kk, e.Error + 2 * (dt + ak + 1)};
}
} // namespace beman::inside::math::detail::ax

#endif // BEMAN_INSIDE_DETAIL_MATH_ADAPTIVE_HPP
