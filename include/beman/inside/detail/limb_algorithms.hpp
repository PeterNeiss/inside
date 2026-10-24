// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Internal — include "beman/inside/inside.hpp" (the umbrella), not this directly.
#ifndef BEMAN_INSIDE_DETAIL_LIMB_ALGORITHMS_HPP
#define BEMAN_INSIDE_DETAIL_LIMB_ALGORITHMS_HPP

#include <beman/inside/detail/config.hpp>
BEMAN_INSIDE_WARNINGS_PUSH

#include <beman/inside/numbers.hpp> // umax

#include <algorithm>
#include <bit>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>

//---------------------------------------------------------------------------
// The library's own integer algorithms: the limb primitives and array
// kernels shared by wide_int, big_rational's magnitudes, the adaptive math
// tier's fixed-point arrays and the text output; and three value algorithms
// for any of its integer types (Euclid's gcd, stripping a factor, a power by
// squaring).
//---------------------------------------------------------------------------
namespace beman::inside::detail {
namespace limb {
template <std::unsigned_integral L>
inline constexpr int bits = std::numeric_limits<L>::digits;

template <std::unsigned_integral L>
struct pair {
    L Hi;
    L Lo;
};

// a + b + carry; carry is 0 or 1 in and out.
template <std::unsigned_integral L>
constexpr L add_carry(L a, L b, L& carry) noexcept {
    const L s  = static_cast<L>(a + b);
    const L s2 = static_cast<L>(s + carry);
    carry      = static_cast<L>((s < a) + (s2 < s));
    return s2;
}

// a − b − borrow; borrow is 0 or 1 in and out.
template <std::unsigned_integral L>
constexpr L sub_borrow(L a, L b, L& borrow) noexcept {
    const L d  = static_cast<L>(a - b);
    const L d2 = static_cast<L>(d - borrow);
    borrow     = static_cast<L>((a < b) + (d < borrow));
    return d2;
}

// Full product a·b as {hi, lo}. Native where the target has unsigned
// __int128, _umul128 at run time on MSVC x64; else a schoolbook 32-bit split.
template <std::unsigned_integral L>
constexpr pair<L> mul(L a, L b) noexcept {
    if constexpr (bits<L> <= 32) {
        const std::uint64_t p = std::uint64_t{a} * b;
        return {static_cast<L>(p >> bits<L>), static_cast<L>(p)};
    } else {
        static_assert(bits<L> == 64, "wide_int: limbs are at most 64 bits");
#if BEMAN_INSIDE_HAS_INT128
        const unsigned __int128 p = static_cast<unsigned __int128>(a) * b;
        return {static_cast<L>(p >> 64), static_cast<L>(p)};
#else
    #if BEMAN_INSIDE_HAS_UMUL128
        if !consteval {
            unsigned long long hi = 0;
            const L            lo = _umul128(a, b, &hi);
            return {static_cast<L>(hi), lo};
        }
    #endif
        const L al = a & 0xffffffffu, ah = a >> 32;
        const L bl = b & 0xffffffffu, bh = b >> 32;
        const L ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;
        const L mid = (ll >> 32) + (lh & 0xffffffffu) + (hl & 0xffffffffu);
        return {hh + (lh >> 32) + (hl >> 32) + (mid >> 32), (ll & 0xffffffffu) | (mid << 32)};
#endif
    }
}

// a·b <=> c·d, exactly (the native 128-bit compare where available).
constexpr std::strong_ordering mul_compare(umax a, umax b, umax c, umax d) noexcept {
#if BEMAN_INSIDE_HAS_INT128
    return static_cast<unsigned __int128>(a) * b <=> static_cast<unsigned __int128>(c) * d;
#else
    const pair<umax> x = mul(a, b), y = mul(c, d);
    return x.Hi != y.Hi ? x.Hi <=> y.Hi : x.Lo <=> y.Lo;
#endif
}

// {hi, lo} / d and the remainder. Requires hi < d, so the quotient fits L.
template <std::unsigned_integral L>
constexpr pair<L> div(L hi, L lo, L d) noexcept // {quotient, remainder}
{
    if constexpr (bits<L> <= 32) {
        const std::uint64_t n = (std::uint64_t{hi} << bits<L>) | lo;
        return {static_cast<L>(n / d), static_cast<L>(n % d)};
    } else {
        if (hi == 0)
            return {static_cast<L>(lo / d), static_cast<L>(lo % d)}; // one machine division
#if BEMAN_INSIDE_HAS_INT128
        const unsigned __int128 n = (static_cast<unsigned __int128>(hi) << 64) | lo;
        return {static_cast<L>(n / d), static_cast<L>(n % d)};
#else
    #if BEMAN_INSIDE_HAS_UMUL128
        if !consteval {
            unsigned long long r = 0;
            const L            q = _udiv128(hi, lo, d, &r);
            return {q, static_cast<L>(r)};
        }
    #endif
        // Restoring shift-subtract; `top` keeps the bit shifted out of r.
        L r = hi, q = 0;
        for (int i = 63; i >= 0; --i) {
            const bool top = (r >> 63) != 0;
            r              = (r << 1) | ((lo >> i) & 1u);
            q <<= 1;
            if (top || r >= d) {
                r -= d;
                q |= 1u;
            }
        }
        return {q, r};
#endif
    }
}


//---------------------------------------------------------------------------
// Array kernels over (pointer, length), least significant limb first, in
// the style of GMP's mpn layer. An output may alias an input of the same
// position where noted.
//---------------------------------------------------------------------------
// r[0, n) = a + b (r may alias a or b); the carry out.
template <std::unsigned_integral L>
constexpr L add_n(L* r, const L* a, const L* b, std::size_t n) noexcept {
    L c = 0;
    for (std::size_t i = 0; i < n; ++i)
        r[i] = add_carry(a[i], b[i], c);
    return c;
}
// r[0, n) = a − b (r may alias a or b); the borrow out.
template <std::unsigned_integral L>
constexpr L sub_n(L* r, const L* a, const L* b, std::size_t n) noexcept {
    L br = 0;
    for (std::size_t i = 0; i < n; ++i)
        r[i] = sub_borrow(a[i], b[i], br);
    return br;
}
// r[0, an) = a + b[0, bn) for an ≥ bn; the carry out.
template <std::unsigned_integral L>
constexpr L add(L* r, const L* a, std::size_t an, const L* b, std::size_t bn) noexcept {
    L c = add_n(r, a, b, bn);
    for (std::size_t i = bn; i < an; ++i)
        r[i] = add_carry(a[i], L{0}, c);
    return c;
}
// r[0, an) = a − b[0, bn) for an ≥ bn; the borrow out.
template <std::unsigned_integral L>
constexpr L sub(L* r, const L* a, std::size_t an, const L* b, std::size_t bn) noexcept {
    L br = sub_n(r, a, b, bn);
    for (std::size_t i = bn; i < an; ++i)
        r[i] = sub_borrow(a[i], L{0}, br);
    return br;
}
// r[0, n) = a·q (r may alias a); the top limb.
template <std::unsigned_integral L>
constexpr L mul_1(L* r, const L* a, std::size_t n, L q) noexcept {
    L carry = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const pair<L> t = mul(a[i], q);
        L             c = 0;
        r[i]            = add_carry(t.Lo, carry, c);
        carry           = static_cast<L>(t.Hi + c);
    }
    return carry;
}
// r[0, n) += a·q; the carry out of the top limb.
template <std::unsigned_integral L>
constexpr L addmul_1(L* r, const L* a, std::size_t n, L q) noexcept {
    L carry = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const pair<L> p  = mul(q, a[i]);
        L             c1 = 0, c2 = 0;
        L             s  = add_carry(r[i], p.Lo, c1);
        s                = add_carry(s, carry, c2);
        r[i]             = s;
        carry            = static_cast<L>(p.Hi + c1 + c2); // fits: (b−1)² + 2(b−1) < b²
    }
    return carry;
}
// r[0, an + bn) = a·b (schoolbook; r aliases neither).
template <std::unsigned_integral L>
constexpr void mul_basecase(L* r, const L* a, std::size_t an, const L* b, std::size_t bn) noexcept {
    for (std::size_t i = 0; i < an + bn; ++i)
        r[i] = 0;
    for (std::size_t i = 0; i < an; ++i)
        r[i + bn] = addmul_1(r + i, b, bn, a[i]);
}
// r[0, n) = a·b mod B^n, the low half of the product (r aliases neither,
// and starts at 0). Past two limbs a's zero limbs are skipped: a small
// value's high limbs add nothing.
template <std::unsigned_integral L>
constexpr void mul_low(L* r, const L* a, const L* b, std::size_t n) noexcept {
    for (std::size_t i = 0; i < n; ++i)
        if (n <= 2 || a[i] != 0)
            addmul_1(r + i, b, n - i, a[i]);
}
// q[0, n) = a / d, top down (q may alias a); a mod d.
template <std::unsigned_integral L>
constexpr L div_1(L* q, const L* a, std::size_t n, L d) noexcept {
    L rem = 0;
    for (std::size_t i = n; i-- > 0;) {
        const pair<L> t = div(rem, a[i], d);
        q[i]            = t.Hi;
        rem             = t.Lo;
    }
    return rem;
}
// r[0, rn) = a[0, an) >> s, the limbs past an read as fill (0, or a sign).
template <std::unsigned_integral L>
constexpr void shift_right(L* r, std::size_t rn, const L* a, std::size_t an, int s, L fill = 0) noexcept {
    constexpr int     B    = bits<L>;
    const std::size_t ws   = static_cast<std::size_t>(s / B);
    const int         bs   = s % B;
    const auto        word = [&](std::size_t k) { return k < an ? a[k] : fill; };
    for (std::size_t i = 0; i < rn; ++i) {
        L w = static_cast<L>(word(i + ws) >> bs);
        if (bs != 0)
            w |= static_cast<L>(word(i + ws + 1) << (B - bs));
        r[i] = w;
    }
}
// r[0, rn) = a[0, an) << s, the bits past rn dropped. r starts at 0 (its
// limbs below the shift are not written) and aliases nothing.
template <std::unsigned_integral L>
constexpr void shift_left(L* r, std::size_t rn, const L* a, std::size_t an, int s) noexcept {
    constexpr int     B    = bits<L>;
    const std::size_t ws   = static_cast<std::size_t>(s / B);
    const int         bs   = s % B;
    const auto        word = [&](std::size_t k) { return k < an ? a[k] : L{0}; };
    for (std::size_t i = ws; i < rn; ++i) {
        L w = static_cast<L>(word(i - ws) << bs);
        if (bs != 0 && i > ws)
            w |= static_cast<L>(word(i - ws - 1) >> (B - bs));
        r[i] = w;
    }
}
// The significant length of a[0, n): n without its top zero limbs.
template <std::unsigned_integral L>
constexpr std::size_t top_size(const L* a, std::size_t n) noexcept {
    while (n > 0 && a[n - 1] == 0)
        --n;
    return n;
}

// Knuth's algorithm D (TAOCP 4.3.1). u has m limbs, v has n limbs with
// v[n−1] != 0 and m >= n. Writes the m−n+1 quotient limbs to q and the n
// remainder limbs to r. The caller provides scratch: un (m+1) and vn (n).
template <std::unsigned_integral L>
constexpr void divmod(const L* u, std::size_t m, const L* v, std::size_t n, L* q, L* r, L* un, L* vn) noexcept {
    constexpr int B = bits<L>;
    if (n == 1) { // (written out: through div_1, GCC loses that this returns for n == 1)
        L rem = 0;
        for (std::size_t i = m; i-- > 0;) {
            const pair<L> d = div(rem, u[i], v[0]);
            q[i]            = d.Hi;
            rem             = d.Lo;
        }
        r[0] = rem;
        return;
    }
    // Normalise: shift so the divisor's top limb has its high bit set.
    const int  s   = std::countl_zero(v[n - 1]);
    const auto shl = [&](L hi, L lo) -> L {
        return s == 0 ? hi : static_cast<L>(static_cast<L>(hi << s) | static_cast<L>(lo >> (B - s)));
    };
    for (std::size_t i = n - 1; i > 0; --i)
        vn[i] = shl(v[i], v[i - 1]);
    vn[0] = static_cast<L>(v[0] << s);
    un[m] = (s == 0) ? L{0} : static_cast<L>(u[m - 1] >> (B - s));
    for (std::size_t i = m - 1; i > 0; --i)
        un[i] = shl(u[i], u[i - 1]);
    un[0] = static_cast<L>(u[0] << s);

    for (std::size_t j = m - n + 1; j-- > 0;) {
        // Estimate q̂ from the top two dividend limbs, then correct it with the
        // next limb (at most two steps).
        L    qhat, rhat;
        bool rhat_big = false; // r̂ ≥ base: skip the test
        if (un[j + n] == vn[n - 1]) {
            qhat     = static_cast<L>(~L{0});
            rhat     = static_cast<L>(un[j + n - 1] + vn[n - 1]);
            rhat_big = rhat < vn[n - 1];
        } else {
            const pair<L> d = div(un[j + n], un[j + n - 1], vn[n - 1]);
            qhat            = d.Hi;
            rhat            = d.Lo;
        }
        while (!rhat_big) {
            const pair<L> p = mul(qhat, vn[n - 2]);
            if (p.Hi < rhat || (p.Hi == rhat && p.Lo <= un[j + n - 2]))
                break;
            --qhat;
            const L old = rhat;
            rhat        = static_cast<L>(rhat + vn[n - 1]);
            rhat_big    = rhat < old;
        }
        // Multiply and subtract q̂·vn from un[j .. j+n].
        L carry = 0, borrow = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const pair<L> p  = mul(qhat, vn[i]);
            L             c  = 0;
            const L       lo = add_carry(p.Lo, carry, c);
            carry            = static_cast<L>(p.Hi + c);
            un[i + j]        = sub_borrow(un[i + j], lo, borrow);
        }
        un[j + n] = sub_borrow(un[j + n], carry, borrow);
        if (borrow != 0) // q̂ was one too large: add back
        {
            --qhat;
            L c = 0;
            for (std::size_t i = 0; i < n; ++i)
                un[i + j] = add_carry(un[i + j], vn[i], c);
            un[j + n] = static_cast<L>(un[j + n] + c);
        }
        q[j] = qhat;
    }
    // Denormalise the remainder.
    for (std::size_t i = 0; i + 1 < n; ++i)
        r[i] = (s == 0) ? un[i] : static_cast<L>(static_cast<L>(un[i] >> s) | static_cast<L>(un[i + 1] << (B - s)));
    r[n - 1] = static_cast<L>(un[n - 1] >> s);
}
} // namespace limb

// gcd(a, b) of non-negative a and b by Euclid (0 when both are), with
// rem(a, b) the remainder.
template <class I, class Rem = std::modulus<>>
constexpr I euclid_gcd(I a, I b, Rem rem = {}) {
    const auto zero = [](const I& v) {
        if constexpr (requires { v.is_zero(); })
            return v.is_zero();
        else
            return v.empty();
    };
    while (!zero(b)) {
        I t = rem(a, b);
        a   = std::move(b);
        b   = std::move(t);
    }
    return a;
}

// x divided by p as often as p divides it (x ≠ 0); how often. A wide x
// (one with Word limbs) divides limb-wise; any other integer type by its
// own % and /.
template <class I>
constexpr imax strip_factor(I& x, umax p) {
    imax k = 0;
    if constexpr (requires { x.Word; }) {
        constexpr std::size_t N = sizeof(x.Word) / sizeof(x.Word[0]);
        if (p == 2) {
            std::size_t w = 0;
            while (w < N && x.Word[w] == 0)
                ++w;
            const int s = static_cast<int>(w) * 64 + (w < N ? std::countr_zero(x.Word[w]) : 0);
            x           = x >> s;
            return s;
        }
        for (;;) {
            umax       q[N];
            const umax r = limb::div_1(q, x.Word, N, p);
            if (r != 0)
                return k;
            std::copy_n(q, N, x.Word);
            ++k;
        }
    } else {
        const I d{p};
        for (; x % d == I{0}; x = x / d)
            ++k;
        return k;
    }
}

// x^n for n ≥ 0 by squaring, with mul(a, b) the product and one the unit.
// For exact multiplication only: a rounded mul would round differently
// from the repeated product.
template <class T, class Mul>
constexpr T pow_by_squaring(T x, unsigned n, Mul mul, T one) {
    T r = std::move(one);
    while (n != 0) {
        if (n & 1u)
            r = mul(r, x);
        n >>= 1;
        if (n != 0)
            x = mul(x, x);
    }
    return r;
}
} // namespace beman::inside::detail

BEMAN_INSIDE_WARNINGS_POP

#endif // BEMAN_INSIDE_DETAIL_LIMB_ALGORITHMS_HPP
