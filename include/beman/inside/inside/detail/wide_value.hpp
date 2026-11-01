// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_DETAIL_WIDE_VALUE_HPP
#define BEMAN_INSIDE_INSIDE_DETAIL_WIDE_VALUE_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/generic.hpp>
#include <beman/inside/inside/detail/number_text.hpp> // scan_number
#include <beman/inside/inside/detail/wide_int.hpp>

#include <algorithm> // std::max
#include <numeric>
#include <iterator>
#include <compare>
#include <expected>
#include <optional>
#include <utility>

//---------------------------------------------------------------------------
// wide_value — exact values of insides the 64-bit paths cannot hold: a wide
// index raw (more than 2^64 slots), or grid numbers past 64 bits.
//
// On an anchored grid Lower/Notch is an integer m (slot_base), so a value is
// its value index J = m + raw times Notch, exactly; an unanchored grid counts
// in its value unit gcd(Notch, Lower) instead. wide_rational<K> carries a value
// as an unreduced fraction of K-limb integers; comparisons cross-multiply, and
// a store divides by the target notch and rounds by the policy's mode.
//
// K is sized from the grids involved (exact_limbs): every exact computation is
// at most a product of two values over a third notch, so three times the
// widest value suffices. Binary operations widen to the wider operand.
//---------------------------------------------------------------------------
namespace beman::inside::detail {

template <std::size_t A, std::size_t B>
constexpr std::strong_ordering operator<=>(const wide_rational<A>& a, const wide_rational<B>& b) noexcept {
    using F = wide_rational<std::max(A, B)>;
    const F x{a}, y{b};
    return x.Num * y.Den <=> y.Num * x.Den;
}
template <std::size_t A, std::size_t B>
constexpr bool operator==(const wide_rational<A>& a, const wide_rational<B>& b) noexcept {
    return (a <=> b) == 0;
}
template <std::size_t A, std::size_t B>
constexpr auto operator+(const wide_rational<A>& a, const wide_rational<B>& b) noexcept {
    using F = wide_rational<std::max(A, B)>;
    const F x{a}, y{b};
    return F{x.Num * y.Den + y.Num * x.Den, x.Den * y.Den};
}
template <std::size_t A, std::size_t B>
constexpr auto operator*(const wide_rational<A>& a, const wide_rational<B>& b) noexcept {
    using F = wide_rational<std::max(A, B)>;
    const F x{a}, y{b};
    return F{x.Num * y.Num, x.Den * y.Den};
}
// Pre: b != 0.
template <std::size_t A, std::size_t B>
constexpr auto operator/(const wide_rational<A>& a, const wide_rational<B>& b) noexcept {
    using F = wide_rational<std::max(A, B)>;
    const F x{a}, y{b};
    const F q{x.Num * y.Den, x.Den * y.Num};
    return q.Den.negative() ? F{-q.Num, -q.Den} : q;
}
template <std::size_t K>
constexpr wide_rational<K> operator-(const wide_rational<K>& a) noexcept {
    return {-a.Num, a.Den};
}

template <std::size_t K = exact_min_limbs>
constexpr wide_rational<K> exact_of(const rational& r) noexcept {
    return {static_cast<wide_sint<K>>(grid_numerator(r)), static_cast<wide_sint<K>>(grid_denominator(r))};
}

template <std::size_t K = exact_min_limbs, std::integral T>
constexpr wide_rational<K> exact_of(T v) noexcept {
    return {wide_sint<K>{v}, wide_sint<K>{1}};
}

// A grid number (a limit or notch, of any size) as an exact value.
template <std::size_t K>
constexpr wide_rational<K> exact_of_grid(const grid_rational& r) noexcept {
    return {static_cast<wide_sint<K>>(grid_numerator(r)), static_cast<wide_sint<K>>(grid_denominator(r))};
}

// a / b for grid numbers, known at compile time to be an integer.
constexpr grid_int exact_quotient(const grid_rational& a, const grid_rational& b) noexcept {
    return grid_numerator(a) * grid_denominator(b) / (grid_denominator(a) * grid_numerator(b));
}

// m = Lower/Notch, the value index of slot 0 (0 for a continuous grid). An
// integer only on an anchored grid: the paths that use it require one.
template <insidable B>
inline constexpr grid_int slot_base = notched<B> ? exact_quotient(lower_of<B>, notch_of<B>) : grid_int{0};

// Bits that bound every value of B's grid: |value| < 2^grid_magnitude_bits.
template <insidable B>
inline constexpr int grid_magnitude_bits = [] {
    auto bits = [](const grid_rational& r) {
        const grid_int n = grid_numerator(r);
        return bit_width_of(abs(n)) - (bit_width_of(grid_denominator(r)) - 1) + 1;
    };
    const int a = bits(lower_of<B>), b = bits(upper_of<B>);
    return a > b ? a : b;
}();

// Bits of |v| for a grid integer.
constexpr int grid_bits(const grid_int& v) { return bit_width_of(abs(v)); }

// Bits of B's values as fractions J·n/d: numerator and denominator together.
template <insidable B>
inline constexpr int exact_value_bits = [] {
    if constexpr (point_storage<B>) {
        return grid_bits(grid_numerator(lower_of<B>)) + grid_bits(grid_denominator(lower_of<B>));
    } else if constexpr (fraction_storage<B>)
        return 2 * decltype(raw_t<B>::Num)::bits;
    else if constexpr (!wide_valued<B>)
        return 128; // a 64-bit rational
    else {
        return grid_magnitude_bits<B> + grid_bits(grid_denominator(notch_of<B>)) +
               grid_bits(grid_numerator(notch_of<B>));
    }
}();

// Limbs for exact computations on values of the given insides.
template <insidable... Bs>
inline constexpr std::size_t exact_limbs = [] {
    int bits = 0;
    ((bits = exact_value_bits<Bs> > bits ? exact_value_bits<Bs> : bits), ...);
    const std::size_t k = limbs_for_bits(3 * bits + 8);
    return k > exact_min_limbs ? k : exact_min_limbs;
}();

template <typename W, integer_storage X>
constexpr W value_index(const X& x) noexcept;

template <insidable B>
constexpr auto exact_of(const B& b) {
    constexpr std::size_t K = exact_limbs<B>;
    using I                 = wide_sint<K>;
    if constexpr (point_storage<B>)
        return exact_of_grid<K>(lower_of<B>);
    else if constexpr (rational_storage<B>)
        return exact_of<K>(b.raw());
    else if constexpr (fraction_storage<B>)
        return wide_rational<K>{b.raw()};
    else if constexpr (anchored<B>) // an integer raw: its value index J times the notch
        return wide_rational<K>{value_index<I>(b) * static_cast<I>(grid_numerator(notch_of<B>)),
                                static_cast<I>(grid_denominator(notch_of<B>))};
    else // an unanchored index raw: Lower + raw·Notch over their common denominator
        return exact_of_grid<K>(lower_of<B>) +
               wide_rational<K>{static_cast<I>(b.raw()) * static_cast<I>(grid_numerator(notch_of<B>)),
                                static_cast<I>(grid_denominator(notch_of<B>))};
}

// f in lowest terms.
template <std::size_t K>
constexpr wide_rational<K> reduced(const wide_rational<K>& f) noexcept {
    using I = wide_sint<K>;
    I a = abs(f.Num), b = f.Den;
    if (bit_width_of(a) <= 64 && bit_width_of(b) <= 64) { // the common case: one-limb Euclid
        const umax an = static_cast<umax>(a), bn = static_cast<umax>(b), g = std::gcd(an, bn);
        if (g == 1)
            return f;
        const I n{an / g};
        return {f.Num.negative() ? -n : n, I{bn / g}};
    }
    const I g = wide_gcd(a, b);
    if (g == I{1})
        return f;
    return {f.Num / g, f.Den / g};
}

// x rounded by M onto the fractions with denominator at most D (D ≥ 1):
// the two Farey neighbours l ≤ x ≤ u of order D, found by walking the
// Stern–Brocot tree in batched (semiconvergent) steps, then picked by M.
// floor / ceil / trunc take a side; nearest the closer one, a tie away from
// zero; half_even the closer one, a tie to the even numerator (else the
// smaller denominator). Products are exact in twice x's width.
template <round_mode M, std::size_t K>
constexpr wide_rational<K> round_to_denominator(const wide_rational<K>& x, const wide_sint<K>& D) noexcept {
    using I        = wide_sint<2 * K + 2>;
    const bool neg = x.Num.negative();
    const I    n{neg ? -x.Num : x.Num}, d{x.Den}, dmax{D};
    I          lp = n / d, lq{1}, up = lp + I{1}, uq{1}; // floor and ceil of |x|
    if (lp * d == n)
        return x; // an integer
    for (;;) {
        const I mp = lp + up, mq = lq + uq;
        if (dmax < mq)
            break;
        if (n * mq < mp * d) { // |x| below the mediant: bring u down
            const I a = up * d - n * uq, b = n * lq - lp * d;
            I       k = (a - I{1}) / b;
            if (const I kd = (dmax - uq) / lq; kd < k)
                k = kd;
            up = up + k * lp;
            uq = uq + k * lq;
        } else if (mp * d < n * mq) { // above: bring l up
            const I a = n * lq - lp * d, b = up * d - n * uq;
            I       k = (a - I{1}) / b;
            if (const I kd = (dmax - lq) / uq; kd < k)
                k = kd;
            lp = lp + k * up;
            lq = lq + k * uq;
        } else
            return x; // |x| is the mediant: on the bound already
    }
    // Pick on |x|: floor of a negative x is the ceil of |x|, and so on.
    bool upper;
    if constexpr (M == round_mode::floor)
        upper = neg;
    else if constexpr (M == round_mode::ceil)
        upper = !neg;
    else if constexpr (M == round_mode::trunc)
        upper = false;
    else {
        const I below = (n * lq - lp * d) * uq, above = (up * d - n * uq) * lq; // distances · d·lq·uq
        if (below != above)
            upper = above < below;
        else if constexpr (M == round_mode::nearest)
            upper = true; // away from zero
        else
            upper = (up.Word[0] & 1u) == 0 ? true : (lp.Word[0] & 1u) == 0 ? false : uq < lq;
    }
    const I p = upper ? up : lp, q = upper ? uq : lq;
    return {static_cast<wide_sint<K>>(neg ? -p : p), static_cast<wide_sint<K>>(q)};
}

// v in lowest terms as the fraction raw F, or nothing when it needs more
// limbs than F has.
template <typename F, std::size_t K>
constexpr std::optional<F> continuous_raw_of(const wide_rational<K>& v) noexcept {
    const auto r      = reduced(v);
    using I           = decltype(F::Num);
    constexpr int cap = I::bits - 1; // magnitude bits
    if (bit_width_of(abs(r.Num)) > cap || bit_width_of(r.Den) > cap)
        return std::nullopt;
    return F{static_cast<I>(r.Num), static_cast<I>(r.Den)};
}

// b's value in lowest terms, in the fewest limbs that hold every value of B.
template <insidable B>
constexpr auto reduced_exact(const B& b) {
    constexpr std::size_t K = limbs_for_bits(exact_value_bits<B> + 1);
    const auto            r = reduced(exact_of(b));
    return wide_rational<K>{static_cast<wide_sint<K>>(r.Num), static_cast<wide_sint<K>>(r.Den)};
}

// A double whose magnitude is at least 2^64 (so an integer), as an exact
// value for a store into L. Past L's magnitude only its side matters, so a
// value just beyond the grid stands in.
template <insidable L>
constexpr wide_rational<exact_limbs<L>> exact_of_large(double d) noexcept {
    using I           = wide_sint<exact_limbs<L>>;
    constexpr int k   = grid_magnitude_bits<L> > 64 ? grid_magnitude_bits<L> : 64;
    const bool    neg = d < 0;
    const double  m   = neg ? -d : d;
    if (!(m < ldexp(1.0, k)))
        return {neg ? -(I{1} << (k + 1)) : I{1} << (k + 1), I{1}};
    const auto [mant, e] = double_parts(m);
    const I v            = I{mant} << e; // e ≥ 12
    return {neg ? -v : v, I{1}};
}

// A finite double's exact value: m·2^e, with 2^1024 and 2^-1074 in reach.
constexpr wide_rational<18> exact_of_double(double d) noexcept {
    using I              = wide_sint<18>;
    const auto [mant, e] = double_parts(d);
    const I n            = e >= 0 ? I{mant} << e : I{mant};
    return {d < 0 ? -n : n, e >= 0 ? I{1} : I{1} << -e};
}

// A wider floating point's exact value (long double): the sum of the doubles
// it splits into, each the rounding error of the last, over 2^1130 (below
// the least subnormal's 2^-1074 · 2^-53 mantissa scale). Past the double
// range a stand-in: ±2^1100 beyond every limit, or ±2^-1130 for a part below
// the subnormals (below every notch, so it only rounds).
template <std::floating_point T>
constexpr wide_rational<36> exact_of_long_double(T v) noexcept {
    using I             = wide_sint<36>;
    constexpr int shift = 1130;
    I             num{0};
    bool          past = false;
    const T       rem  = for_each_double_part(v, [&](double d) {
        if (!is_finite(d))
            return !(past = true);
        const auto [mant, e] = double_parts(d);
        const I m            = I{mant} << (e + shift);
        num += d < 0 ? -m : m;
        return true;
    });
    if (past)
        return {rem < 0 ? -(I{1} << 1100) : I{1} << 1100, I{1}};
    if (rem != 0)
        num += rem < 0 ? I{-1} : I{1};
    return reduced(wide_rational<36>{num, I{1} << shift});
}

// The nearest T to an exact value (ties to even), rounded once from the
// exact quotient: its top digits + 2 bits and a sticky bit, then scaled by
// exact powers of two. For a T wider than double (long double), whose
// digits a double would drop. Past T's range ±infinity, below half its least
// subnormal ±0.
template <std::floating_point T, std::size_t K>
constexpr T nearest_float(const wide_rational<K>& f) noexcept {
    using U           = wide_uint<2 * K + 2>; // n·2^s with s up to digits + 2 + the denominator's bits
    using lim         = std::numeric_limits<T>;
    constexpr int p   = lim::digits;
    const bool    neg = f.Num.negative() != f.Den.negative();
    const U       n{static_cast<wide_uint<K>>(abs(f.Num))};
    const U       d{static_cast<wide_uint<K>>(abs(f.Den))};
    if (n.is_zero())
        return neg ? -T{0} : T{0};
    // q = ⌊n·2^s / d⌋ has p + 2 or p + 3 bits: |v| = (q + sticky)·2^-s.
    const int  s  = p + 2 - (bit_width_of(n) - bit_width_of(d));
    const U    nn = s >= 0 ? n << s : n, dd = s >= 0 ? d : d << -s;
    const U    q      = nn / dd;
    const bool sticky = !(nn - q * dd).is_zero();
    const int  bw     = bit_width_of(q);
    const int  e      = bw - 1 - s; // |v| in [2^e, 2^(e+1))
    if (e > lim::max_exponent - 1)
        return neg ? -lim::infinity() : lim::infinity();
    // Below the normal range a subnormal keeps fewer digits.
    const int keep = e >= lim::min_exponent - 1 ? p : p - (lim::min_exponent - 1 - e);
    if (keep < 0)
        return neg ? -T{0} : T{0};
    const int drop = bw - keep; // ≥ 2
    U         m    = q >> drop;
    const U   low = q - (m << drop), half = U{1} << (drop - 1);
    if (half < low || (low == half && (sticky || static_cast<umax>(m) % 2 != 0)))
        m += U{1};
    if (bit_width_of(m) - 1 - s + drop > lim::max_exponent - 1) // rounded up past the largest finite
        return neg ? -lim::infinity() : lim::infinity();
    // m ≤ 2^p is exact in T, limb by limb; the scaling by 2^(drop − s) in
    // steps of 2^±64 stays exact (every step lies between m and the result).
    T x = std::accumulate(std::rbegin(m.Word), std::rend(m.Word), T{0}, [](T acc, umax w) {
        return acc * static_cast<T>(0x1p64) + static_cast<T>(w);
    });
    for (int k = drop - s; k != 0;) {
        const int step = k > 64 ? 64 : k < -64 ? -64 : k;
        const T   t    = pow_by_squaring(step < 0 ? static_cast<T>(0.5) : static_cast<T>(2),
                                         static_cast<unsigned>(step < 0 ? -step : step));
        x *= t;
        k -= step;
    }
    return neg ? -x : x;
}

// Truncation toward zero, as an integer.
template <std::size_t K>
constexpr wide_sint<K> trunc(const wide_rational<K>& f) noexcept {
    return f.Num / f.Den;
}

// The reduced value as a 64-bit rational, or overflow when it does not fit.
template <std::size_t K>
constexpr std::expected<rational, errc> try_rational(const wide_rational<K>& f) noexcept {
    using I        = wide_sint<K>;
    const auto r   = reduced(f);
    const bool neg = r.Num.negative();
    const I    a   = neg ? -r.Num : r.Num;
    if (a > I{std::numeric_limits<umax>::max()} || r.Den > I{std::numeric_limits<imax>::max()})
        return std::unexpected{errc::overflow};
    const imax den = static_cast<imax>(r.Den);
    return rational{static_cast<umax>(a), neg ? -den : den};
}

// Text → exact value, for values that outgrow the 64-bit rational parse
// (parse falls back here on errc::overflow): the grammar of parse_text
// (scan_number: radix prefixes, ' separators, '.', e / p exponents, N/D),
// built in K limbs. Anything K limbs cannot hold reports overflow.
template <std::size_t K>
constexpr std::expected<wide_rational<K>, errc> parse_exact(const char* first, const char* last) noexcept {
    using I           = wide_sint<K>;
    constexpr I limit = std::numeric_limits<I>::max() / I{32}; // room for ·16 + a digit
    const auto  big   = [&](const I& x) { return limit < x; };
    auto        one   = [&](const char* f, const char* l) -> std::expected<wide_rational<K>, errc> {
        I                    num{0}, den{1};
        int                  frac_zeros = 0; // deferred, as in parse_number: trailing zeros cost nothing
        const scanned_number sc         = scan_number(f, l, [&](int d, bool in_frac, int b) {
            const I base{b};
            if (in_frac && d == 0) {
                ++frac_zeros;
                return true;
            }
            for (; frac_zeros > 0; --frac_zeros) {
                if (big(num) || big(den))
                    return false;
                num = num * base;
                den = den * base;
            }
            if (big(num) || big(den))
                return false;
            num = num * base + I{d};
            if (in_frac)
                den = den * base;
            return true;
        });
        if (sc.Fail != parse_fail::none)
            return std::unexpected{errc::invalid_format};
        if (sc.Stopped)
            return std::unexpected{errc::overflow};
        if (num.is_zero())
            return wide_rational<K>{I{0}, I{1}};
        if (sc.HasP) { // ·2^±Exp
            I& t = sc.ExpNeg ? den : num;
            if (bit_width_of(t) + sc.Exp > I::bits - 2)
                return std::unexpected{errc::overflow};
            t = t << sc.Exp;
        } else if (sc.HasE) // ·10^±Exp
            for (int e = 0; e < sc.Exp; ++e) {
                I& t = sc.ExpNeg ? den : num;
                if (big(t))
                    return std::unexpected{errc::overflow};
                t = t * I{10};
            }
        return wide_rational<K>{num, den};
    };
    return parse_fraction(first, last, one, [](const wide_rational<K>& d) { return d.Num.is_zero(); });
}

// The text parse's width: wide enough for about 1200 decimal digits or a
// 10^±1200 exponent, past every grid's own values.
inline constexpr std::size_t parse_limbs = 64;

// Bits of B's finest denominator: every grid value and every midpoint
// between two lies 0 or at least 2^−(bits + 1) from 0.
template <insidable B>
inline constexpr int finest_den_bits = bit_width_of(den_bound(grid_of<B>));

// Bounds on log2|v| of one number in the literal grammar (lo ≤ log2|v| < hi),
// from its digits and exponent alone; empty for malformed text or zero.
struct log2_bounds {
    long Lo, Hi;
};
constexpr std::optional<log2_bounds> number_log2_bounds(const char* first, const char* last) noexcept {
    long                 sig = 0, frac = 0; // significant digits, digits after the point
    const scanned_number sc = scan_number(first, last, [&](int d, bool in_frac, int) {
        if (sig > 0 || d != 0)
            ++sig;
        if (in_frac)
            ++frac;
        return true;
    });
    if (sc.Fail != parse_fail::none || sig == 0)
        return std::nullopt;
    // |v| = M·base^−frac·(10 or 2)^±Exp, base^(sig−1) ≤ M < base^sig.
    auto up = [&](long n) { return sc.Base == 10 ? (n * 3322 + 999) / 1000 : sc.Base == 16 ? 4 * n : n; };
    auto dn = [&](long n) { return sc.Base == 10 ? n * 3321 / 1000 : sc.Base == 16 ? 4 * n : n; };
    long lo = dn(sig - 1) - up(frac), hi = up(sig) - dn(frac);
    if (sc.HasP) {
        const long e = sc.ExpNeg ? -long{sc.Exp} : long{sc.Exp};
        lo += e;
        hi += e;
    } else if (sc.HasE) {
        const long e = long{sc.Exp};
        lo += sc.ExpNeg ? -(e * 3322 + 999) / 1000 : e * 3321 / 1000;
        hi += sc.ExpNeg ? -(e * 3321 / 1000) : (e * 3322 + 999) / 1000;
    }
    return log2_bounds{lo, hi};
}

// A number past the wide parse (1e-5000, 1e5000, either as N/D) still has
// a store. One nearer 0 than every value and midpoint of B rounds and
// range-checks like any other such value of its sign: ±2^−(finest_den_bits
// + 2) stands in. One past every limit of B overflows, or clamps onto the
// bound, like any other: ±2^(exact_value_bits + 2) stands in — unless Wrap,
// whose fold needs the exact value. Empty unless the text's magnitude,
// bounded from its digits and exponents, is that small or that large.
template <insidable B, std::size_t K, bool Wrap = false>
constexpr std::optional<wide_rational<K>> stand_in(const char* first, const char* last) noexcept {
    constexpr int G            = finest_den_bits<B> + 2;
    constexpr int H            = exact_value_bits<B> + 2;
    const auto [neg, f, slash] = split_fraction(first, last);
    auto b                     = number_log2_bounds(f, slash);
    if (!b)
        return std::nullopt;
    if (slash != last) {
        const auto d = number_log2_bounds(slash + 1, last);
        if (!d)
            return std::nullopt; // malformed, or N/0: the parse reports it
        b = log2_bounds{b->Lo - d->Hi, b->Hi - d->Lo};
    }
    using I = wide_sint<K>;
    if constexpr (G < 64 * static_cast<int>(K) - 2)
        if (b->Hi <= -G)
            return wide_rational<K>{neg ? I{-1} : I{1}, I{1} << G};
    if constexpr (!Wrap && H < 64 * static_cast<int>(K) - 2)
        if (b->Lo >= H)
            return wide_rational<K>{neg ? -(I{1} << H) : I{1} << H, I{1}};
    return std::nullopt;
}

// n / d (d != 0) rounded to an integer by M; the sign rules of div_rounded.
// Exact: d divides n (from the same division).
template <std::size_t K>
struct rounded_quotient {
    wide_sint<K> Quot;
    bool         Exact;
};
template <round_mode M, std::size_t K>
constexpr rounded_quotient<K> rounded_div_exact(const wide_sint<K>& n, const wide_sint<K>& d) noexcept {
    using I        = wide_sint<K>;
    auto [q, r]    = I::divmod(n, d); // toward zero
    const bool neg = n.negative() != d.negative();
    const I    ar = abs(r), ad = abs(d);
    if (!rounds_away(M, neg, classify_remainder(M, ar, ad), (q.Word[0] & 1u) != 0))
        return {q, r.is_zero()};
    return {neg ? q - I{1} : q + I{1}, false};
}
template <round_mode M, std::size_t K>
constexpr wide_sint<K> rounded_div(const wide_sint<K>& n, const wide_sint<K>& d) noexcept {
    return rounded_div_exact<M>(n, d).Quot;
}

// The value index of f on L's lattice (f / Notch) rounded by M, minus the
// slot base: L's slot offset. Exact is false when f lies between notches.
template <std::size_t K>
struct exact_index_result {
    wide_sint<K> Index;
    bool         Exact;
};

template <insidable L, round_mode M, std::size_t K>
constexpr auto exact_index(const wide_rational<K>& f) noexcept {
    constexpr std::size_t KK = std::max(K, exact_limbs<L>);
    using I                  = wide_sint<KK>;
    const wide_rational<KK> g{f};
    if constexpr (anchored<L>) {
        const I    n = g.Num * static_cast<I>(grid_denominator(notch_of<L>));
        const I    d = g.Den * static_cast<I>(grid_numerator(notch_of<L>)); // > 0
        const auto q = rounded_div_exact<M>(n, d);                          // one division for both
        return exact_index_result<KK>{q.Quot - static_cast<I>(slot_base<L>), q.Exact};
    } else {
        // The offset (f − Lower)/Notch rounded in value space (rounds_up).
        const wide_rational<KK> o = g + -exact_of_grid<KK>(lower_of<L>);
        const I                 n = o.Num * static_cast<I>(grid_denominator(notch_of<L>));
        const I                 d = o.Den * static_cast<I>(grid_numerator(notch_of<L>)); // > 0 (Den > 0)
        const auto [q, r]         = floor_divmod(n, d);
        const bool up             = rounds_up(M,
                                              g.Num.negative(),
                                              classify_remainder(M, r, d),
                                              ((q + static_cast<I>(lower_index_wide<L>)).Word[0] & 1u) != 0);
        return exact_index_result<KK>{up ? q + I{1} : q, r.is_zero()};
    }
}

// x rounded by P's rounding to a fraction of L's bound (a bounded continuous
// grid: denominators ≤ MaxDenominator), limits aside.
template <insidable L, typename P, std::size_t K>
constexpr wide_rational<K> round_to_bound(const wide_rational<K>& x) {
    return round_to_denominator<rounding_for<L, P>>(
        x, static_cast<wide_sint<K>>(grid_numerator(grid_of<L>.MaxDenominator)));
}

template <insidable L, typename P>
constexpr bool exact_rounds_into_range(const rational& v, rational& out) {
    if constexpr (!notched<L>) { // a bounded continuous grid: onto its bound
        // Near a limit it fits, unless the bound itself is past imax.
        const auto r = try_rational(round_to_bound<L, P>(exact_of(v)));
        if (!r)
            return false;
        out = *r;
        return includes(interval_of<L>, out);
    } else {
        const auto j = exact_index<L, rounding_for<L, P>>(exact_of(v)).Index; // L's slot offset
        using I      = std::remove_cvref_t<decltype(j)>;
        if (j.negative() || I{max_index_of<L>} < j)
            return false;
        out = (lower64<L> + (rational{static_cast<umax>(j)} * notch64<L>).value()).value(); // L's point
        return true;
    }
}

// The raw of slot offset `offset` (0 .. step count) in L's storage. W is
// any signed integer holding the offset and the value index J = offset +
// Lower/Notch: imax, a wide_int, a wide_sint.
template <insidable L, typename W>
constexpr raw_t<L> raw_of_slot(const W& offset) noexcept {
    if constexpr (point_storage<L>)
        return raw_t<L>{};
    else if constexpr (index_storage<L>)
        return static_cast<raw_t<L>>(offset);
    else { // integer value raw on a whole-number grid: raw == J (Notch 1)
        static_assert(integer_value_storage<L>, "raw_of_slot: a slot needs a notched grid");
        return static_cast<raw_t<L>>(offset + static_cast<W>(slot_base<L>));
    }
}

// The raw a default-constructed B holds: 0 when its grid holds 0, else
// Lower. (A lattice holds 0 only when anchored: 0's slot offset −Lower/Notch
// is then an integer.)
template <insidable B>
inline constexpr raw_t<B> default_raw = []() -> raw_t<B> {
    constexpr bool zero = grid_of<B>.representable(grid_rational{0});
    if constexpr (point_storage<B>)
        return {};
    else if constexpr (rational_storage<B>)
        return zero ? rational{0} : lower64<B>;
    else if constexpr (fraction_storage<B>) {
        if constexpr (zero)
            return {};
        else
            return raw_t<B>{exact_of_grid<continuous_limbs<grid_of<B>>>(lower_of<B>)};
    } else if constexpr (zero)
        return raw_of_slot<B>(-slot_base<B>);
    else
        return raw_of_slot<B>(grid_int{0});
}();

//---------------------------------------------------------------------------
// Wrapping value-index arithmetic — the integer path of + and ×.
//
// wrap_work_t<Result> is unsigned and as wide as Result's raw (at least 64
// bits). + − × wrap modulo 2^bits, so any expression whose true value is a
// Result raw comes out exact, however far its intermediates wrapped. Each
// operand enters as its value index J = value/Notch (an index raw plus
// Lower/Notch, a value raw as is — value storage has notch 1).
//---------------------------------------------------------------------------
template <insidable Result>
using wrap_work_t = std::conditional_t<wide_index_storage<Result>, raw_t<Result>, umax>;

// An operand's value-index range in `Unit`s: Lower/Unit .. Upper/Unit.
template <insidable X, grid_rational Unit>
inline constexpr grid_int units_lo = exact_quotient(lower_of<X>, Unit);
template <insidable X, grid_rational Unit>
inline constexpr grid_int units_hi = exact_quotient(upper_of<X>, Unit);

// Whether from_value_in_units<Result, Unit> divides: the result is counted
// in a unit finer than its notch (or off its lattice's multiples).
template <insidable Result, grid_rational Unit>
inline constexpr bool divides_units = !(point_storage<Result> || (anchored<Result> && Unit == notch_of<Result>));

// The work type of an integer + or ×: signed imax when every value index
// involved (each operand in its unit, the result in its notch, and the
// result's slot offsets) provably fits — then nothing wraps, and the
// compiler keeps the value ranges, as the builtin paths always did — else
// the wrapping type, or, when a division follows (which a wrapped value
// cannot take), an exact signed one.
// URes: the unit the result is counted in (its notch, or a finer unit
// that also divides its Lower when the grids do not pass through 0); its
// slot offsets in that unit are what from_value_in_units divides.
template <insidable     Result,
          insidable     L,
          grid_rational UL,
          insidable     R,
          grid_rational UR,
          grid_rational URes = notch_of<Result>>
inline constexpr int index_work_bits =
    signed_value_bits({units_lo<L, UL>,
                       units_hi<L, UL>,
                       units_lo<R, UR>,
                       units_hi<R, UR>,
                       units_lo<Result, URes>,
                       units_hi<Result, URes>,
                       grid_of<Result>.step_count() * exact_quotient(notch_of<Result>, URes)});
template <insidable     Result,
          insidable     L,
          grid_rational UL,
          insidable     R,
          grid_rational UR,
          grid_rational URes = notch_of<Result>>
using index_work_t = std::conditional_t<(index_work_bits<Result, L, UL, R, UR, URes> <= 63),
                                        imax,
                                        std::conditional_t<divides_units<Result, URes>,
                                                           work_int_t<index_work_bits<Result, L, UL, R, UR, URes>>,
                                                           wrap_work_t<Result>>>;

template <typename W, integer_storage X>
constexpr W value_index(const X& x) noexcept {
    if constexpr (index_storage<X> && std::is_integral_v<W>) {
        // In W's unsigned twin: a raw past W's max (a step count past 2^63
        // from a negative base) wraps back onto the index, which fits W.
        using U = std::make_unsigned_t<W>;
        return static_cast<W>(static_cast<U>(static_cast<W>(slot_base<X>)) + static_cast<U>(x.raw()));
    } else if constexpr (index_storage<X>)
        return static_cast<W>(slot_base<X>) + static_cast<W>(x.raw());
    else
        return static_cast<W>(x.raw());
}

// The unit an integer path counts X's values in: its value unit (the notch
// on an anchored grid), or |c| for a point c.
template <insidable X>
inline constexpr grid_rational unit_of = point_grid<X> ? abs(lower_of<X>) : grid_of<X>.value_unit();

// Two integer raws whose values, counted in the gcd of their value units,
// fit imax.
template <insidable L, insidable R>
inline constexpr bool units_cmp_fits = [] {
    if constexpr (!integer_storage<L> || !integer_storage<R>)
        return false;
    else {
        constexpr grid_rational U = gcd(unit_of<L>, unit_of<R>);
        return signed_value_bits({units_lo<L, U>, units_hi<L, U>, units_lo<R, U>, units_hi<R, U>}) <= 63;
    }
}();

// x's value in units of `Unit` (an integer: the unit divides x's notch and
// Lower, or x's value for a point).
template <typename W, grid_rational Unit, insidable X>
constexpr W value_in_units(const X& x) noexcept {
    // A point (Lower == Upper) holds its value in the type.
    if constexpr (point_grid<X>) {
        constexpr grid_int q = exact_quotient(lower_of<X>, Unit);
        return static_cast<W>(q);
    } else if constexpr (!anchored<X>) {
        // An index raw (integer value storage has integer values): Lower in
        // units, plus raw notches of `scale` units each.
        // raw·scale, the offset from Lower, can pass W while the value fits:
        // an integral W computes in its unsigned twin, as value_index does.
        constexpr grid_int base = exact_quotient(lower_of<X>, Unit), scale = exact_quotient(notch_of<X>, Unit);
        if constexpr (std::is_integral_v<W>) {
            using U = std::make_unsigned_t<W>;
            return static_cast<W>(static_cast<U>(static_cast<W>(base)) +
                                  static_cast<U>(x.raw()) * static_cast<U>(static_cast<W>(scale)));
        } else
            return static_cast<W>(base) + static_cast<W>(x.raw()) * static_cast<W>(scale);
    } else {
        constexpr grid_int scale = exact_quotient(notch_of<X>, Unit);
        if constexpr (scale == grid_int{1})
            return value_index<W>(x);
        else
            return value_index<W>(x) * static_cast<W>(scale);
    }
}

// The Result whose value index is j (taken modulo 2^bits).
template <insidable Result, typename W>
constexpr Result from_value_index(const W& j) noexcept {
    if constexpr (point_storage<Result>)
        return Result::from_raw(raw_t<Result>{});
    else if constexpr (index_storage<Result>)
        return Result::from_raw(static_cast<raw_t<Result>>(j - static_cast<W>(slot_base<Result>)));
    else
        return Result::from_raw(static_cast<raw_t<Result>>(j));
}

// The Result whose value is j `Unit`s (on its lattice by construction; an
// exact division when Unit is finer than its notch).
template <insidable Result, grid_rational Unit, typename W>
constexpr Result from_value_in_units(const W& j) noexcept {
    if constexpr (!divides_units<Result, Unit>)
        return from_value_index<Result>(j);
    else if constexpr (index_storage<Result>) {
        constexpr grid_int base  = exact_quotient(lower_of<Result>, Unit),
                           scale = exact_quotient(notch_of<Result>, Unit);
        return Result::from_raw(static_cast<raw_t<Result>>((j - static_cast<W>(base)) / static_cast<W>(scale)));
    } else // integer values counted in Unit = 1/k
        return Result::from_raw(
            static_cast<raw_t<Result>>(j / static_cast<W>(exact_quotient(grid_rational{1}, Unit))));
}

// Exact result of grid arithmetic: the value is on the result lattice and
// inside its interval by construction, so it maps straight to a raw.
template <insidable Result, std::size_t K>
constexpr Result exact_result(const wide_rational<K>& v) noexcept {
    return Result::from_raw(raw_of_slot<Result>(exact_index<Result, round_mode::trunc>(v).Index));
}
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_INSIDE_DETAIL_WIDE_VALUE_HPP
