// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_DETAIL_RATIONAL_HPP
#define BEMAN_INSIDE_INSIDE_DETAIL_RATIONAL_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/detail/numbers.hpp>    // umax/imax, arithmetic, rational fwd
#include <beman/inside/inside/detail/float_bits.hpp> // abs_fraction, is_double_value
#include <beman/inside/inside/detail/error.hpp>      // errc, detail::raise, detail::constexpr_error
#include <beman/inside/inside/detail/wide_int.hpp>   // limb kernels for exact 128-bit cross products
#include <beman/inside/inside/detail/rounding.hpp>   // round_mode, rounds_away

#include <bit>
#include <expected> // std::expected, std::unexpected

#include <numeric>
#include <compare>
#include <limits>
#include <type_traits>
#include <utility>

namespace beman::inside::detail {
// |d| of a signed denominator (any imax, INT64_MIN too).
[[nodiscard]] constexpr umax abs_den(imax d) noexcept { return safe_abs(d); }

//---------------------------------------------------------------------------
// cancel_gcd — divides both parts by their gcd (the denominator signed)
//---------------------------------------------------------------------------
inline constexpr void cancel_gcd(umax& numerator, imax& denominator) {
    umax ad = abs_den(denominator);
    if (ad <= 1)
        return;
    if (std::has_single_bit(ad)) {
        // A power-of-two denominator: gcd(n, 2^k) = 2^ctz(n | 2^k), no loop.
        const int s = std::countr_zero(numerator | ad);
        numerator >>= s;
        ad >>= s;
        denominator = (denominator < 0) ? -static_cast<imax>(ad) : static_cast<imax>(ad);
        return;
    }
    auto g = std::gcd(numerator, ad);
    if (g <= 1)
        return;
    numerator /= g;
    ad /= g;
    denominator = static_cast<imax>((denominator < 0) ? umax{0} - ad : ad);
}

inline constexpr void cancel_gcd(umax& a, umax& b) {
    if (a <= 1 || b <= 1)
        return;
    if (std::has_single_bit(a) || std::has_single_bit(b)) {
        // A power of two on either side (every dyadic denominator):
        // gcd = 2^ctz(a | b), no loop, no division.
        const int s = std::countr_zero(a | b);
        a >>= s;
        b >>= s;
        return;
    }
    auto g = std::gcd(a, b);
    if (g <= 1)
        return;
    a /= g;
    b /= g;
}

[[nodiscard]] consteval rational operator+(rational, rational);
[[nodiscard]] consteval rational operator/(rational, rational);
[[nodiscard]] consteval rational operator-(rational, rational);

[[nodiscard]] consteval rational operator*(rational, rational);
[[nodiscard]] constexpr auto     operator<=>(rational, rational) -> std::strong_ordering;

//---------------------------------------------------------------------------
// rational — structural type for NTTP (public members only). Sign is encoded
// in the denominator (negative = negative rational); numerator is unsigned to
// represent e.g. umax itself. Default-constructed it is 0 (0/1). Every
// constructor stores the reduced form; writing Num/Den directly is unchecked
// (like inside::from_raw), but == and <=> still compare values. (As a template
// argument, a hand-built unreduced value is a different argument: template
// identity compares the members.)
//---------------------------------------------------------------------------
struct rational {
    umax Num{0};
    imax Den{1};

    constexpr rational() = default;
    // Exact for every double of magnitude 2^-10 or more; one finer than
    // 2^-62 keeps only its bits down to 2^-62 (truncated toward zero), as the
    // denominator has 63 bits. |value| ≥ 2^64 and non-finite values raise.
    constexpr rational(std::floating_point auto);
    constexpr rational(std::signed_integral auto, imax = 1);
    constexpr rational(std::unsigned_integral auto num, imax den = 1) : Num{num}, Den{den} { canonicalize(Num, Den); }

    // Two-unsigned overload: lets `rational{i, N}` accept two `size_t`
    // operands without forcing the caller to `static_cast<imax>` the
    // numerator. A den past imax has no signed denominator: overflow.
    template <std::unsigned_integral N, std::unsigned_integral D>
    constexpr rational(N num, D den) : Num{num}, Den{static_cast<imax>(den)} {
        if (!std::in_range<imax>(den))
            raise(errc::overflow, "rational: denominator past imax");
        canonicalize(Num, Den);
    }

    // The values, not the members: equal members (two reduced values) answer
    // at once; anything else compares the fractions (a hand-built 2/2 equals 1).
    [[nodiscard]] constexpr bool operator==(rational rhs) const {
        return (Num == rhs.Num && Den == rhs.Den) || (*this <=> rhs) == 0;
    }
    // Exact against a double too (rational{value} truncates one finer than
    // 2^-62): defined after compare_double.
    template <plain_number T>
    [[nodiscard]] constexpr bool operator==(T value) const;

    [[nodiscard]] constexpr rational operator-() const;

    template <std::unsigned_integral T>
    constexpr std::expected<T, errc> to() const;

    template <std::unsigned_integral T>
    explicit constexpr operator T() const {
        if (Den < 0)
            raise(errc::domain_error, "cannot convert negative rational to unsigned");
        return static_cast<T>(Num / abs_den(Den));
    }

    template <std::signed_integral T>
    explicit constexpr operator T() const {
        const umax q = Num / abs_den(Den);
        return static_cast<T>((Den < 0) ? umax{0} - q : q);
    }

    template <std::floating_point T>
    explicit constexpr operator T() const {
        T q = static_cast<T>(Num) / static_cast<T>(abs_den(Den));
        return (Den < 0) ? -q : q;
    }

    // allow unary+ for generic programming
    [[nodiscard]] constexpr rational operator+() const { return *this; }

    // Compound-assign: forward to the checked binary op and unwrap it; an
    // error (overflow, a zero divisor) is a compile error. Compile time only.
    consteval rational& operator+=(rational rhs);
    consteval rational& operator-=(rational rhs);
    consteval rational& operator*=(rational rhs);
    consteval rational& operator/=(rational rhs);

  private:
    // The two invalid denominators: 0 (n/0 is a division by zero, as in
    // parse and big_rational) and imax_min (does not fit: it cannot be
    // negated without UB, which every sign-flip in the file assumes is
    // well-defined).
    static constexpr void check_den(imax den) {
        if (den == 0)
            raise(errc::division_by_zero, "rational: zero denominator");
        if (den == std::numeric_limits<imax>::min())
            raise(errc::overflow, "rational: denominator imax_min (cannot be negated)");
    }

    // Domain check + canonical-zero + gcd reduction; used by the integral ctors.
    static constexpr void canonicalize(umax& num, imax& den) {
        check_den(den);
        if (num == 0)
            den = 1;
        cancel_gcd(num, den);
    }

    // Signed-encoded denominator for the signed ctor, validated BEFORE the
    // negation so `-den` is never UB.
    static constexpr imax signed_den_from(std::signed_integral auto num, imax den) {
        check_den(den);
        return (num < 0) ? -den : den;
    }
};

[[nodiscard]] constexpr std::expected<rational, errc> gcd(rational, rational);
[[nodiscard]] constexpr rational                      abs(rational);

[[nodiscard]] constexpr bool divides_evenly(rational, rational);

//---------------------------------------------------------------------------
// sign / named integer reductions — free functions over the public
// numerator/denominator (structural type), siblings of abs/gcd. Reductions
// are explicit, lossy alternatives to `static_cast`:
// trunc → 0; floor → -inf; ceil → +inf; round → half-away-from-zero.
//---------------------------------------------------------------------------
// -1 / 0 / +1 — single source of truth for the sign convention (sign lives in
// Den; canonical zero is {0, 1}).
[[nodiscard]] constexpr int sign(rational v) noexcept {
    if (v.Num == 0)
        return 0;
    return (v.Den < 0) ? -1 : 1;
}

// A rational from already-canonical parts (magnitude, signed denominator):
// no gcd, no domain checks.
[[nodiscard]] constexpr rational rational_from_parts(umax num, imax den) noexcept {
    rational r;
    r.Num = num;
    r.Den = den;
    return r;
}

// A checked op failed: during constant evaluation that is a compile error
// naming the cause; at runtime it is an error value.
// Loud == false is the quiet form behind try_add & co.: an error value even
// during constant evaluation, for code that must test whether an op fits.
template <fixed_string Msg, bool Loud = true>
constexpr std::unexpected<errc> fail(errc code) {
    if constexpr (Loud)
        if consteval {
            constexpr_error<Msg>();
        }
    return std::unexpected{code};
}

// The numerator with the value's sign, as I (callers ensure it fits). A
// builtin I negates in umax, so −2^63 (numerator 2^63) comes out as INT64_MIN.
template <class I = imax>
[[nodiscard]] constexpr I signed_numerator(rational v) noexcept {
    if constexpr (std::integral<I>)
        return static_cast<I>((v.Den < 0) ? umax{0} - v.Num : v.Num);
    else
        return v.Den < 0 ? -I{v.Num} : I{v.Num};
}

// v rounded to an integer by m (nearest: half away from zero), as a sign
// and a umax magnitude: q + 1 cannot overflow, since a nonzero remainder
// needs a denominator ≥ 2, so q ≤ umax/2.
struct rounded_integer {
    umax Magnitude;
    bool Negative;
};
[[nodiscard]] constexpr rounded_integer round_magnitude(rational v, round_mode m) {
    const umax ad  = abs_den(v.Den);
    const umax q   = v.Num / ad;
    const bool neg = v.Den < 0;
    return {q + rounds_away(m, neg, classify_remainder(m, v.Num % ad, ad), (q & 1) != 0), neg};
}

// Narrowed to imax (callers keep |v| within it).
[[nodiscard]] constexpr imax round_to_int(rational v, round_mode m) {
    const auto [mag, neg] = round_magnitude(v, m);
    return static_cast<imax>(neg ? umax{0} - mag : mag);
}

// Kept exact as a rational, for indices that may pass imax.
[[nodiscard]] constexpr rational round_to_integral(rational v, round_mode m) {
    const auto [mag, neg] = round_magnitude(v, m);
    return rational_from_parts(mag, (neg && mag != 0) ? imax{-1} : imax{1});
}

[[nodiscard]] constexpr imax trunc(rational v) { return round_to_int(v, round_mode::trunc); }
[[nodiscard]] constexpr imax floor(rational v) { return round_to_int(v, round_mode::floor); }
[[nodiscard]] constexpr imax ceil(rational v) { return round_to_int(v, round_mode::ceil); }
[[nodiscard]] constexpr imax round(rational v) { return round_to_int(v, round_mode::nearest); }

//---------------------------------------------------------------------------
// abs
//---------------------------------------------------------------------------
[[nodiscard]] constexpr rational abs(rational v) {
    if (v.Den < 0)
        v.Den = -v.Den;
    return v;
}

//---------------------------------------------------------------------------
// gcd
//---------------------------------------------------------------------------
// Returns errc::overflow if the combined denominator lcm = (a/gcd)·b would exceed
// imax_max (sign-bit reservation) — traps the mul_overflow then range-checks.
//---------------------------------------------------------------------------
[[nodiscard]] constexpr std::expected<rational, errc> gcd(rational lhs, rational rhs) {
    umax a = abs_den(lhs.Den);
    umax b = abs_den(rhs.Den);
    umax g = std::gcd(a, b);

    umax denominator;
    if (mul_overflow(a / g, b, &denominator))
        return std::unexpected{errc::overflow};
    if (!std::in_range<imax>(denominator))
        return std::unexpected{errc::overflow};

    auto numerator = std::gcd(lhs.Num, rhs.Num);
    return rational{numerator, denominator};
}

//---------------------------------------------------------------------------
// Floating points wider than double (long double), exactly: the sum of the
// doubles they split into, each the rounding error of the last.
//---------------------------------------------------------------------------

// |v|·2^wide_float_shift for a finite 2^-64 ≤ |v| < 2^64, truncated to an
// integer (< 2^244), and the sign of what the truncation dropped (0: exact,
// as for every long double of at most 116 significant bits).
inline constexpr int wide_float_shift = 180;
struct scaled_float {
    wide_uint<4> Magnitude;
    int          Tail;
};
template <std::floating_point T>
[[nodiscard]] constexpr scaled_float scaled_magnitude(T v) noexcept {
    using W = wide_sint<5>;
    W       acc{0};
    int     tail = 0;
    const T rem  = for_each_double_part(v < 0 ? -v : v, [&](double d) {
        const auto [mant, e] = double_parts(d);
        const int s          = e + wide_float_shift;
        if (s < 0) { // below 2^-180: the parts after it are smaller still
            tail = d < 0 ? -1 : 1;
            return false;
        }
        acc += d < 0 ? -(W{mant} << s) : W{mant} << s;
        return true;
    });
    if (tail == 0 && rem != 0)
        tail = rem < 0 ? -1 : 1;
    return {static_cast<wide_uint<4>>(acc), tail};
}

// rational{v} for a finite v wider than a double: exact when it fits, else
// (bits finer than 2^-62) truncated toward zero, as for a double; |v| ≥ 2^64
// raises.
template <std::floating_point T>
constexpr rational rational_of_wide_float(T v, bool& exact) {
    const T a = v < 0 ? -v : v;
    exact     = true;
    if (!(a < static_cast<T>(0x1p64)))
        raise(errc::overflow, "beman::inside::detail::rational: |long double| >= 2^64");
    if (a < static_cast<T>(0x1p-62)) {
        exact = a == 0;
        return rational_from_parts(0, 1);
    }
    const auto [m, tail] = scaled_magnitude(a);
    constexpr int drop   = wide_float_shift - 62; // keep bits down to 2^-62
    exact                = tail == 0 && (m << (wide_uint<4>::bits - drop)).is_zero();
    const auto q         = m >> drop; // ≥ 1 here
    const int  tz        = std::countr_zero(static_cast<umax>(q)) < 62 ? std::countr_zero(static_cast<umax>(q)) : 62;
    const auto num       = q >> tz;
    if (bit_width_of(num) > 64)
        raise(errc::overflow, "beman::inside::detail::rational: long double past the 64-bit rational");
    const imax den = imax{1} << (62 - tz);
    return rational_from_parts(static_cast<umax>(num), v < 0 ? -den : den);
}

//---------------------------------------------------------------------------
// rational::rational
//---------------------------------------------------------------------------
constexpr rational::rational(std::signed_integral auto num, imax den)
    : Num{safe_abs(num)}, Den{signed_den_from(num, den)} {
    canonicalize(Num, Den);
}

// rational{value}, and whether it is exact: a finite |value| < 2^64 finer
// than 2^-62 loses its low bits (truncated toward zero). abs_fraction's
// fraction is already reduced.
constexpr rational rational_of_double(double value, bool& exact) {
    exact = true;
    if (value == 0.0)
        return rational_from_parts(0, 1);
    const auto [num, den] = abs_fraction(value, exact);
    return rational_from_parts(num, value < 0 ? -static_cast<imax>(den) : static_cast<imax>(den));
}

constexpr rational::rational(std::floating_point auto value) {
    if (!is_finite(value))
        raise(errc::not_finite, "non-finite double");
    bool exact;
    if constexpr (wide_float<decltype(value)>)
        if (!is_double_value(value)) {
            *this = rational_of_wide_float(value, exact);
            return;
        }
    *this = rational_of_double(static_cast<double>(value), exact);
}

//---------------------------------------------------------------------------
// to
//---------------------------------------------------------------------------
template <std::unsigned_integral T>
constexpr std::expected<T, errc> rational::to() const {
    if (Den < 0)
        return std::unexpected{errc::domain_error};
    const umax q = Num / abs_den(Den);
    if (!std::in_range<T>(q))
        return std::unexpected{errc::overflow};
    return static_cast<T>(q);
}

// a + b exactly, for the checked add once a 64-bit step overflowed: the
// numerator and lcm in 3 limbs, reduced, then checked to fit (a sum can
// reduce back into range: (2^63+1)/2 + 1/6 = (3·2^62+2)/3).
template <bool Loud>
[[gnu::cold]] [[gnu::noinline]] constexpr std::expected<rational, errc> add_wide(rational a, rational b) {
    using W       = wide_sint<3>;
    const umax ad = abs_den(a.Den), bd = abs_den(b.Den), g = std::gcd(ad, bd);
    W          num = signed_numerator<W>(a) * W{bd / g} + signed_numerator<W>(b) * W{ad / g};
    W          den = W{ad} * W{bd / g};
    // (num is nonzero: a + b == 0 needs equal denominators, a 64-bit step.)
    const W x      = wide_gcd(abs(num), den);
    num            = num / x;
    den            = den / x;
    const bool neg = num.negative();
    const W    mag = neg ? -num : num;
    if (W{std::numeric_limits<umax>::max()} < mag || W{std::numeric_limits<imax>::max()} < den)
        return fail<"rational +: result out of the 64-bit range", Loud>(errc::overflow);
    const imax d = static_cast<imax>(static_cast<umax>(den));
    return rational_from_parts(static_cast<umax>(mag), neg ? -d : d);
}

// Reduce n/d, the sum of two reduced fractions over d = lcm of their
// denominators, whose gcd is g: gcd(n, d) = gcd(n, g) (Henrici), so a sum
// over coprime denominators is already reduced.
constexpr void reduce_sum(umax& n, umax& d, umax g) noexcept {
    if (g == 1)
        return;
    // A power-of-two g: the gcd is the lowest set bit of n | g.
    const umax h = std::has_single_bit(g) ? (n | g) & (umax{0} - (n | g)) : std::gcd(n, g);
    n /= h;
    d /= h;
}

//---------------------------------------------------------------------------
// rational_add / rational_mul / rational_div — the arithmetic
// bodies, library-internal (the public operators are consteval: a rational is
// compile-time vocabulary, run-time values are insides). Checked=true returns
// expected<rational, errc>, reporting overflow / division_by_zero (Loud: a
// compile error in constant evaluation); Checked=false silently overflows —
// the caller must guarantee its absence (a grid bound proves it).
//---------------------------------------------------------------------------
template <bool Checked, bool Loud = true>
inline constexpr auto rational_add(rational a, rational b) {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;

    // (a == −b needs no test: equal denominators, opposite signs → num == 0 below.)
    if (a.Num == 0)
        return ret_t{b};
    if (b.Num == 0)
        return ret_t{a};

    bool a_neg = a.Den < 0;
    bool b_neg = b.Den < 0;
    umax a_ad  = abs_den(a.Den);
    umax b_ad  = abs_den(b.Den);

    if (a_ad == b_ad) {
        if (a_neg == b_neg) {
            umax numerator;
            if constexpr (Checked) {
                if (add_overflow(a.Num, b.Num, &numerator)) {
                    return ret_t{fail<"rational +: numerator overflow (same denominator)", Loud>(errc::overflow)};
                }
            } else
                numerator = a.Num + b.Num;

            rational r;
            r.Num = numerator;
            r.Den = a.Den;
            cancel_gcd(r.Num, r.Den);
            return ret_t{r};
        }

        umax num   = (a.Num > b.Num) ? (a.Num - b.Num) : (b.Num - a.Num);
        bool r_neg = a_neg ? (a.Num > b.Num) : (b.Num > a.Num);
        if (num == 0)
            return ret_t{rational{0}};
        rational r;
        r.Num = num;
        r.Den = static_cast<imax>(r_neg ? umax{0} - a_ad : a_ad);
        cancel_gcd(r.Num, r.Den);
        return ret_t{r};
    }

    // Common denominator = lcm(a_ad, b_ad) = a_ad·(b_ad/g), not a_ad·b_ad: the
    // reduced cofactors (g = gcd) overflow far less often than the raw product.
    umax g      = std::gcd(a_ad, b_ad);
    umax a_ad_r = a_ad / g; // = a_ad / gcd; coprime with b_ad_r
    umax b_ad_r = b_ad / g;

    umax denominator;
    umax A;
    umax B;

    if constexpr (Checked) {
        if (mul_overflow(a_ad, b_ad_r, &denominator) || // = lcm(a_ad, b_ad)
            !std::in_range<imax>(denominator)) {
            return ret_t{fail<"rational +: denominator overflow", Loud>(errc::overflow)};
        }
        if (mul_overflow(a.Num, b_ad_r, &A) || mul_overflow(b.Num, a_ad_r, &B)) {
            // Mixed signs: |A − B| can fit umax even when a cross-product alone
            // does not (e.g. 1024 − m/2^54 forms 1024·2^54 == 2^64 before the
            // subtraction brings it back in range). Retry the difference in 128-bit before giving up.
            if (a_neg != b_neg) {
                const limb::pair<umax> A2       = limb::mul(a.Num, b_ad_r);
                const limb::pair<umax> B2       = limb::mul(b.Num, a_ad_r);
                const bool             a_bigger = A2.Hi != B2.Hi ? A2.Hi > B2.Hi : A2.Lo > B2.Lo;
                const limb::pair<umax> big = a_bigger ? A2 : B2, small = a_bigger ? B2 : A2;
                if (big.Hi - small.Hi - (big.Lo < small.Lo ? 1u : 0u) == 0) {
                    rational r;
                    r.Num = big.Lo - small.Lo;
                    r.Den = static_cast<imax>((a_neg ? a_bigger : !a_bigger) ? umax{0} - denominator : denominator);
                    cancel_gcd(r.Num, r.Den);
                    return ret_t{r};
                }
            }
            return ret_t{fail<"rational +: cross-multiplication overflow", Loud>(errc::overflow)};
        }
    } else {
        denominator = a_ad * b_ad_r;
        A           = a.Num * b_ad_r;
        B           = b.Num * a_ad_r;
    }

    if (a_neg == b_neg) {
        umax numerator;
        if constexpr (Checked) {
            if (add_overflow(A, B, &numerator)) {
                return ret_t{fail<"rational +: numerator sum overflow", Loud>(errc::overflow)};
            }
        } else
            numerator = A + B;

        // num, den both > 0 here, so the ctor's domain/zero checks are dead;
        // assemble directly and reduce.
        reduce_sum(numerator, denominator, g);
        return ret_t{
            rational_from_parts(numerator, a_neg ? -static_cast<imax>(denominator) : static_cast<imax>(denominator))};
    }

    // numerator == 0 (exact cancellation) is unreachable here: it would
    // require a == -b, which for canonical inputs has equal denominators and
    // took the equal-denominator branch above.
    umax       numerator = (A > B) ? (A - B) : (B - A);
    const bool r_neg     = a_neg ? (A > B) : (B > A);
    reduce_sum(numerator, denominator, g);
    return ret_t{
        rational_from_parts(numerator, r_neg ? -static_cast<imax>(denominator) : static_cast<imax>(denominator))};
}

template <bool Checked, bool Loud = true>
inline constexpr auto rational_mul(rational a, rational b) {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;

    if (a.Num == 0 || b.Num == 0)
        return ret_t{rational{0}};

    bool r_neg = (a.Den < 0) != (b.Den < 0);
    umax a_ad  = abs_den(a.Den);
    umax b_ad  = abs_den(b.Den);

    if (a_ad == 1 && b_ad == 1) {
        umax numerator;
        if constexpr (Checked) {
            if (mul_overflow(a.Num, b.Num, &numerator)) {
                return ret_t{fail<"rational *: numerator overflow", Loud>(errc::overflow)};
            }
        } else
            numerator = a.Num * b.Num;

        return ret_t{rational_from_parts(numerator, r_neg ? imax{-1} : imax{1})};
    }

    cancel_gcd(a.Num, b_ad);
    cancel_gcd(b.Num, a_ad);

    umax numerator;
    umax denominator;
    if constexpr (Checked) {
        if (mul_overflow(a.Num, b.Num, &numerator) || mul_overflow(a_ad, b_ad, &denominator) ||
            !std::in_range<imax>(denominator)) {
            return ret_t{fail<"rational *: numerator or denominator overflow", Loud>(errc::overflow)};
        }
    } else {
        numerator   = a.Num * b.Num;
        denominator = a_ad * b_ad;
    }

    // The cross-cancellations above guarantee gcd(numerator, denominator) == 1,
    // so bypass rational(num, den) and skip its redundant cancel_gcd.
    return ret_t{rational_from_parts(numerator, static_cast<imax>(r_neg ? umax{0} - denominator : denominator))};
}

//---------------------------------------------------------------------------
// rational_inv — multiplicative inverse (1 / a)
//---------------------------------------------------------------------------
// a is already in lowest terms (Num and |Den| coprime), so the swapped
// pair is too. Sign lives in the denominator and 1/(-x) has the same
// sign as -x, so the sign bit moves with the (now) denominator unchanged.
// Caller's contract: a != 0 and a's numerator fits imax.
inline constexpr rational rational_inv(rational a) { return rational_from_parts(abs_den(a.Den), signed_numerator(a)); }

// div(a, b): checked, cross-reduced; unchecked, a * inv(b) (caller's contract).
template <bool Checked, bool Loud = true>
inline constexpr auto rational_div(rational a, rational b) {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;

    if constexpr (Checked) {
        // Cross-reduce a.N/b.N and b.D/a.D first: b.N need not fit the
        // denominator alone (x/x with x > 2^63 is 1).
        if (b.Num == 0)
            return ret_t{fail<"rational /: division by zero", Loud>(errc::division_by_zero)};
        if (a.Num == 0)
            return ret_t{rational{0}};
        const umax g1 = std::gcd(a.Num, b.Num);
        const umax g2 = std::gcd(abs_den(a.Den), abs_den(b.Den));
        umax       num, den;
        if (mul_overflow(a.Num / g1, abs_den(b.Den) / g2, &num) ||
            mul_overflow(abs_den(a.Den) / g2, b.Num / g1, &den) || !std::in_range<imax>(den))
            return ret_t{fail<"rational /: numerator or denominator overflow", Loud>(errc::overflow)};
        const bool neg = (a.Den < 0) != (b.Den < 0);
        return ret_t{rational_from_parts(num, neg ? -static_cast<imax>(den) : static_cast<imax>(den))};
    } else
        return rational_mul<false>(a, rational_inv(b));
}

//---------------------------------------------------------------------------
// unchecked rational arithmetic — caller guarantees: no umax overflow on the
// products, no zero divisor/numerator, and the result Denominator fits in imax.
// The checked variants enforce all three; unchecked skips them.
//---------------------------------------------------------------------------
inline constexpr rational add_unchecked(rational a, rational b) { return rational_add<false>(a, b); }

inline constexpr rational mul_unchecked(rational a, rational b) { return rational_mul<false>(a, b); }

inline constexpr rational div_unchecked(rational a, rational b) { return rational_div<false>(a, b); }

//---------------------------------------------------------------------------
// operator-
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr rational rational::operator-() const {
    if (Num == 0)
        return *this;

    // Already in lowest terms; flip the sign-encoding directly, no cancel_gcd.
    return rational_from_parts(Num, -Den);
}

//---------------------------------------------------------------------------
// operator<=>
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr auto operator<=>(rational lhs, rational rhs) -> std::strong_ordering {
    const int lhs_sign = sign(lhs), rhs_sign = sign(rhs);
    if (lhs_sign != rhs_sign)
        return lhs_sign <=> rhs_sign;
    // Equal signs: the magnitudes cross-multiplied in 128 bits, reversed for
    // negative values. Each factor is below 2^64, so the products are exact
    // for every pair: no integer special case, no division. Two zeros give
    // two zero products: equal.
    const umax lhs_ad = abs_den(lhs.Den), rhs_ad = abs_den(rhs.Den);
    return lhs_sign < 0 ? limb::mul_compare(rhs.Num, lhs_ad, lhs.Num, rhs_ad)
                        : limb::mul_compare(lhs.Num, rhs_ad, rhs.Num, lhs_ad);
}

// lhs against a finite double finer than 2^-62 (which rational{rhs}
// truncates): |lhs| = N/D against |rhs| = s·2^-k (k > 62) as N·2^k against
// s·D, which stays within two 64-bit factors. Cold and out of line: the
// stores compare doubles with their limits.
[[nodiscard]] [[gnu::cold]] [[gnu::noinline]] constexpr std::strong_ordering compare_fine_double(rational lhs,
                                                                                                 double   rhs) {
    const int ls = sign(lhs), rs = rhs < 0 ? -1 : 1;
    if (ls != rs)
        return ls <=> rs;
    const auto [s, exp2] = abs_fields(rhs);
    const int  k         = -exp2; // rhs = s·2^-k, k > 62
    const umax n = lhs.Num, d = abs_den(lhs.Den);
    // s·D < 2^117: past that width N·2^k is larger.
    const auto mag = std::bit_width(n) + k > 117
                         ? std::strong_ordering::greater
                         : limb::mul_compare(n << (k - 63), umax{1} << 63, s, d); // N·2^(k−63) < 2^54
    return ls < 0 ? (0 <=> mag) : mag;
}

// lhs against a double, exactly (non-finite raises, as rational{rhs}). Every
// rational lies strictly inside ±2^64, so a larger double is decided by its sign.
[[nodiscard]] inline constexpr std::strong_ordering compare_double(rational lhs, double rhs) {
    if (is_finite(rhs) && past_64_bits(rhs)) [[unlikely]]
        return rhs < 0 ? std::strong_ordering::greater : std::strong_ordering::less;
    if (abs_fraction_exact(rhs) || not is_finite(rhs)) [[likely]]
        return lhs <=> rational{rhs};
    return compare_fine_double(lhs, rhs);
}

// lhs against a floating point of any width, exactly: one wider than double
// (long double) with bits past the double's compares as N·2^180 against its
// scaled magnitude times D.
template <std::floating_point T>
[[nodiscard]] constexpr std::strong_ordering compare_float(rational lhs, T rhs) {
    if constexpr (wide_float<T>)
        if (is_finite(rhs) && !is_double_value(rhs)) [[unlikely]] {
            const int ls = sign(lhs), rs = rhs < 0 ? -1 : 1;
            if (ls != rs)
                return ls <=> rs;
            const T              a   = rhs < 0 ? -rhs : rhs;
            std::strong_ordering mag = std::strong_ordering::equal;
            if (!(a < static_cast<T>(0x1p64))) // past every rational
                mag = std::strong_ordering::less;
            else if (a < static_cast<T>(0x1p-64)) // below every nonzero rational
                mag = std::strong_ordering::greater;
            else {
                using W              = wide_uint<5>;
                const auto [m, tail] = scaled_magnitude(a);
                mag                  = (W{lhs.Num} << wide_float_shift) <=> W{m} * W{abs_den(lhs.Den)};
                if (mag == 0)
                    mag = 0 <=> tail;
            }
            return ls < 0 ? (0 <=> mag) : mag;
        }
    return compare_double(lhs, static_cast<double>(rhs));
}

template <plain_number T>
[[nodiscard]] constexpr bool rational::operator==(T value) const {
    if constexpr (std::floating_point<T>)
        return compare_float(*this, value) == 0;
    else
        return operator==(rational{value});
}

template <plain_number T>
[[nodiscard]] inline constexpr auto operator<=>(T lhs, rational rhs) {
    if constexpr (std::floating_point<T>)
        return 0 <=> compare_float(rhs, lhs);
    else
        return rational{lhs} <=> rhs;
}

template <plain_number T>
[[nodiscard]] inline constexpr auto operator<=>(rational lhs, T rhs) {
    if constexpr (std::floating_point<T>)
        return compare_float(lhs, rhs);
    else
        return lhs <=> rational{rhs};
}

//---------------------------------------------------------------------------
// Binary operators: the rational ⋈ rational cores, the arithmetic-operand
// forms and the compound assignments. All consteval: a rational's arithmetic
// is compile-time vocabulary, like a grid number's, and an overflow is a
// compile error naming the operation, so the result is a plain rational. At
// run time exact values are insides, whose grids bound every result; the
// library's own run-time steps on rational raws are the named functions
// above (a grid proves the unchecked ones fit; a try_* step that misses
// falls back to the exact wide value).
//---------------------------------------------------------------------------
// rational_add fails on a 64-bit step's overflow; the sum may still fit once
// reduced, which add_wide (cold) settles.
[[nodiscard]] inline consteval rational operator+(rational lhs, rational rhs) {
    const auto r = rational_add<true, false>(lhs, rhs);
    return r ? *r : *add_wide<true>(lhs, rhs);
}

[[nodiscard]] inline consteval rational operator-(rational lhs, rational rhs) { return lhs + -rhs; }

[[nodiscard]] inline consteval rational operator*(rational lhs, rational rhs) { return *rational_mul<true>(lhs, rhs); }

[[nodiscard]] inline consteval rational operator/(rational lhs, rational rhs) { return *rational_div<true>(lhs, rhs); }

// Quiet checked ops: like the operators, but an overflow is an error value,
// in constant evaluation too (the operators make it a compile error), and
// they run at run time: the library's 64-bit fast steps, each with an exact
// fallback, and compile-time code that asks whether a result fits.
[[nodiscard]] inline constexpr std::expected<rational, errc> try_add(rational a, rational b) {
    const auto r = rational_add<true, false>(a, b);
    if (!r) [[unlikely]]
        return add_wide<false>(a, b);
    return r;
}
[[nodiscard]] inline constexpr std::expected<rational, errc> try_sub(rational a, rational b) { return try_add(a, -b); }
[[nodiscard]] inline constexpr std::expected<rational, errc> try_mul(rational a, rational b) {
    return rational_mul<true, false>(a, b);
}
[[nodiscard]] inline constexpr std::expected<rational, errc> try_div(rational a, rational b) {
    return rational_div<true, false>(a, b);
}

#define BEMAN_INSIDE_RATIONAL_OP(op)                                       \
    template <plain_number T>                                              \
    [[nodiscard]] inline consteval auto operator op(T lhs, rational rhs) { \
        return rational{lhs} op rhs;                                       \
    }                                                                      \
    template <plain_number T>                                              \
    [[nodiscard]] inline consteval auto operator op(rational lhs, T rhs) { \
        return lhs op rational{rhs};                                       \
    }                                                                      \
    inline consteval rational& rational::operator op## = (rational rhs) {  \
        *this = *this op rhs;                                              \
        return *this;                                                      \
    }                                                                      \
    template <plain_number T>                                              \
    inline consteval rational& operator op## = (rational & lhs, T rhs) {   \
        return lhs op## = rational{rhs};                                   \
    }

BEMAN_INSIDE_RATIONAL_OP(+)
BEMAN_INSIDE_RATIONAL_OP(-)
BEMAN_INSIDE_RATIONAL_OP(*)
BEMAN_INSIDE_RATIONAL_OP(/)
#undef BEMAN_INSIDE_RATIONAL_OP

//---------------------------------------------------------------------------
// divides_evenly
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr bool divides_evenly(rational dividend, rational divisor) {
    if (divisor == 0)
        return true; // convention: everything divides 0 evenly

    // dividend/divisor ∈ ℤ without forming the (possibly umax-overflowing)
    // quotient numerator. In lowest terms dividend = p/q, divisor = r/s; the
    // quotient p·s/(q·r) is integral iff q | s and r | p (rationals are
    // canonicalized, so gcd(p,q)=gcd(r,s)=1). All checks are on single fields.
    const umax p = dividend.Num, q = abs_den(dividend.Den);
    const umax r = divisor.Num, s = abs_den(divisor.Den);
    return (s % q == 0) && (p % r == 0);
}

// Checked builder for per<D> below: a static_assert here fires at the user's
// spelling (also through just<N, D>, which is N · per<D>).
template <umax D>
consteval rational make_per() {
    static_assert(D >= 1, "per<D> is the positive step 1/D; a continuous grid is spelled 0");
    static_assert(std::in_range<imax>(D), "per<D>: denominator too large");
    return rational{umax{1}, D};
}
} // namespace beman::inside::detail

namespace beman::inside {
// `rational` — an exact 64-bit fraction: the value of a literal (`0.1_r`), a
// continuous inside's raw, and a value read out of an inside. Its arithmetic
// is compile time only; at run time an exact value is an inside.
using detail::rational;

// The grid-building helpers:
//   per<D>       the step 1/D — the common notch (per<256> is Q·8);
// any other exact ratio is the point just<N, D> (inside/detail/core.hpp): a limit like
// just<-6, 5> for -1.2, or another step like just<360, 4096>
// (the _r literal, an exact decimal / hex value — 0.1_r is exactly 1/10 —
// is in inside/detail/number_text.hpp.)
// Integer steps are plain integers ({{0, 100}, 5}); grid::validate rejects a
// negative notch however it is spelled.
template <umax D>
inline constexpr detail::rational per = detail::make_per<D>();

} // namespace beman::inside

#endif // BEMAN_INSIDE_INSIDE_DETAIL_RATIONAL_HPP
