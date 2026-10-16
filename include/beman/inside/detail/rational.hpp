// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_RATIONAL_HPP
#define BEMAN_INSIDE_DETAIL_RATIONAL_HPP

#include <beman/inside/math.hpp>              // umax/imax, arithmetic, rational fwd
#include <beman/inside/detail/float_bits.hpp> // frexp/ldexp, abs_fraction, is_double_value
#include <beman/inside/lift.hpp>              // lift, is_expected_v, unwrap_t
#include <beman/inside/detail/overflow.hpp>   // add/sub/mul_overflow
#include <beman/inside/detail/debug.hpp>      // errc, detail::raise, detail::constexpr_error
#include <beman/inside/detail/wide_int.hpp>   // limb kernels for exact 128-bit cross products
#include <beman/inside/detail/rounding.hpp>   // round_mode, rounds_away

#include <bit>
#include <expected> // std::expected, std::unexpected

#include <numeric>
#include <compare>
#include <limits>
#include <tuple>
#include <type_traits>

namespace beman::inside::detail {
[[nodiscard]] constexpr umax abs_den(imax d) noexcept {
    return (d >= 0) ? static_cast<umax>(d) : umax{0} - static_cast<umax>(d);
}

//---------------------------------------------------------------------------
// trim
//---------------------------------------------------------------------------
inline constexpr void trim(umax& numerator, imax& denominator) {
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
    denominator = (denominator < 0) ? -ad : ad;
}

inline constexpr void trim(umax& a, umax& b) {
    if (a <= 1 || b <= 1)
        return;
    auto g = std::gcd(a, b);
    if (g <= 1)
        return;
    a /= g;
    b /= g;
}

[[nodiscard]] constexpr std::expected<rational, errc> operator+(const rational&, const rational&);
[[nodiscard]] constexpr std::expected<rational, errc> operator/(const rational&, const rational&);
[[nodiscard]] constexpr std::expected<rational, errc> operator-(const rational&, const rational&);

[[nodiscard]] constexpr std::expected<rational, errc> operator*(const rational&, const rational&);
[[nodiscard]] constexpr auto                          operator<=>(rational, rational) -> std::strong_ordering;

//---------------------------------------------------------------------------
// Overflow / malformed-literal signalling
//---------------------------------------------------------------------------
// Failure aborts constant evaluation via `detail::constexpr_error<Msg>()` — a
// non-constexpr [[noreturn]] helper carrying the message in an NTTP (literal
// parsers and the checked paths' `fail<"...">` under `if consteval`),
// hard-failing the build with the text in the diagnostic; at runtime those
// paths fall through to `std::unexpected`. No `throw`, so it is -fno-exceptions clean.

//---------------------------------------------------------------------------
// rational — structural type for NTTP (public members only). Sign is encoded
// in the denominator (negative = negative rational); numerator is unsigned to
// represent e.g. umax itself.
//---------------------------------------------------------------------------
struct rational {
    umax Numerator;
    imax Denominator;

    constexpr rational() = default;
    // Exact for every double of magnitude 2^-10 or more; one finer than
    // 2^-62 keeps only its bits down to 2^-62 (truncated toward zero), as the
    // denominator has 63 bits. |value| ≥ 2^64 and non-finite values raise.
    constexpr rational(std::floating_point auto);
    constexpr rational(std::signed_integral auto, imax = 1);
    constexpr rational(std::unsigned_integral auto num, imax den = 1) : Numerator{num}, Denominator{den} {
        canonicalize(Numerator, Denominator);
    }

    // Two-unsigned overload: lets `rational{i, N}` accept two `size_t`
    // operands without forcing the caller to `static_cast<imax>` the
    // numerator. A den past imax has no signed denominator: overflow.
    template <std::unsigned_integral N, std::unsigned_integral D>
    constexpr rational(N num, D den) : Numerator{num}, Denominator{static_cast<imax>(den)} {
        if (static_cast<umax>(den) > static_cast<umax>(std::numeric_limits<imax>::max()))
            raise(errc::overflow, "rational: denominator past imax");
        canonicalize(Numerator, Denominator);
    }

    // Implicit unwrap of a checked result, so coefficient expressions read as
    // plain arithmetic (`rational two_pi = 2 * pi;`); an error (overflow) is a
    // compile error in constant evaluation, a throw at runtime. same_as-constrained
    // (not a plain `rational(expected<rational, errc>)`) because expected's own
    // converting ctor is gated on `is_constructible_v<rational, U>` — a
    // non-template overload would make that trait depend on itself.
    template <class O>
        requires std::same_as<std::remove_cvref_t<O>, std::expected<rational, errc>>
    constexpr rational(O&& o) : rational(o.value()) {}

    // operator== by default for structural type
    [[nodiscard]] constexpr bool operator==(const rational&) const = default;
    // Exact against a double too (rational{value} truncates one finer than
    // 2^-62): defined after compare_double.
    template <arithmetic T>
    [[nodiscard]] constexpr bool operator==(T value) const;

    [[nodiscard]] constexpr rational operator-() const;

    template <std::unsigned_integral T>
    constexpr std::expected<T, errc> to() const;

    template <std::unsigned_integral T>
    explicit constexpr operator T() const {
        if (Denominator < 0)
            raise(errc::domain_error, "cannot convert negative rational to unsigned");
        return Numerator / abs_den(Denominator);
    }

    template <std::signed_integral T>
    explicit constexpr operator T() const {
        umax q = Numerator / abs_den(Denominator);
        return (Denominator < 0) ? -q : q;
    }

    template <std::floating_point T>
    explicit constexpr operator T() const {
        T q = static_cast<T>(Numerator) / static_cast<T>(abs_den(Denominator));
        return (Denominator < 0) ? -q : q;
    }

    // allow unary+ for generic programming
    [[nodiscard]] constexpr rational operator+() const { return *this; }

    // Compound-assign: forward to the checked binary op and unwrap via .value()
    // — overflow surfaces as std::bad_expected_access (no error channel here).
    constexpr rational& operator+=(const rational& rhs);
    constexpr rational& operator-=(const rational& rhs);
    constexpr rational& operator*=(const rational& rhs);
    constexpr rational& operator/=(const rational& rhs);

    // Unchecked arithmetic — caller takes responsibility for non-overflow
    // (and non-zero operand for div_unchecked / inv_unchecked).
    static constexpr rational add_unchecked(rational, rational);
    static constexpr rational mul_unchecked(rational, rational);
    static constexpr rational div_unchecked(rational, rational);
    static constexpr rational inv_unchecked(rational);

    static constexpr std::expected<rational, errc> add(rational a, rational b) { return a + b; }
    static constexpr std::expected<rational, errc> inv(rational);

    // Shared algorithm bodies. Checked=true returns expected<rational, errc>,
    // reporting overflow / division_by_zero (a compile error at compile time,
    // std::unexpected at runtime); Checked=false
    // silently overflows — the caller must guarantee its absence.
    template <bool Checked, bool Loud = true>
    static constexpr auto add_impl(const rational&, const rational&);
    template <bool Checked, bool Loud = true>
    static constexpr auto mul_impl(const rational&, const rational&);
    template <bool Checked, bool Loud = true>
    static constexpr auto div_impl(const rational&, const rational&);
    template <bool Checked, bool Loud = true>
    static constexpr auto inv_impl(const rational&);

  private:
    // Domain check + canonical-zero + gcd reduction; used by the integral ctors.
    // Two domain errors: Denominator == 0 (undefined)
    // and Denominator == imax_min (cannot be negated without UB, which every
    // sign-flip in the file assumes is well-defined).
    static constexpr void canonicalize(umax& num, imax& den) {
        if (den == 0)
            raise(errc::domain_error, "Denominator of Zero is invalid");
        if (den == std::numeric_limits<imax>::min())
            raise(errc::domain_error, "Denominator imax_min is invalid (cannot be negated)");
        if (num == 0)
            den = 1;
        trim(num, den);
    }

    // Signed-encoded denominator for the signed ctor, validated BEFORE the
    // negation so `-den` is never UB (the ctor-body canonicalize() would catch
    // these too, but only after the mem-init already evaluated `-den`).
    static constexpr imax signed_den_from(std::signed_integral auto num, imax den) {
        if (den == 0)
            raise(errc::domain_error, "Denominator of Zero is invalid");
        if (den == std::numeric_limits<imax>::min())
            raise(errc::domain_error, "Denominator imax_min is invalid (cannot be negated)");
        return (num < 0) ? -den : den;
    }
};

[[nodiscard]] constexpr std::expected<rational, errc> gcd(const rational&, const rational&);
[[nodiscard]] constexpr rational                      abs(rational);

[[nodiscard]] constexpr bool divides_evenly(const rational&, const rational&);

//---------------------------------------------------------------------------
// sign / named integer reductions — free functions over the public
// numerator/denominator (structural type), siblings of abs/gcd. Reductions
// are explicit, lossy alternatives to `static_cast`:
// trunc → 0; floor → -inf; ceil → +inf; round → half-away-from-zero.
//---------------------------------------------------------------------------
// -1 / 0 / +1 — single source of truth for the sign convention (sign lives in
// Denominator; canonical zero is {0, 1}).
[[nodiscard]] constexpr int sign(rational v) noexcept {
    if (v.Numerator == 0)
        return 0;
    return (v.Denominator < 0) ? -1 : 1;
}

// A rational from already-canonical parts (magnitude, signed denominator):
// no gcd, no domain checks.
[[nodiscard]] constexpr rational make_raw(umax num, imax den) noexcept {
    rational r;
    r.Numerator   = num;
    r.Denominator = den;
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

// The numerator with the value's sign, as imax (callers ensure it fits).
// Negated in umax, so −2^63 (numerator 2^63) comes out as INT64_MIN.
[[nodiscard]] constexpr imax signed_numerator(rational v) noexcept {
    return static_cast<imax>((v.Denominator < 0) ? umax{0} - v.Numerator : v.Numerator);
}

// v rounded to an integer by m (nearest: half away from zero), as a sign
// and a umax magnitude: q + 1 cannot overflow, since a nonzero remainder
// needs a denominator ≥ 2, so q ≤ umax/2.
struct rounded_integer {
    umax Magnitude;
    bool Negative;
};
[[nodiscard]] constexpr rounded_integer round_magnitude(rational v, round_mode m) {
    const umax ad  = abs_den(v.Denominator);
    const umax q   = v.Numerator / ad;
    const bool neg = v.Denominator < 0;
    return {q + rounds_away(m, neg, classify_remainder(m, v.Numerator % ad, ad), (q & 1) != 0), neg};
}

// Narrowed to imax (callers keep |v| within it).
[[nodiscard]] constexpr imax round_to_int(rational v, round_mode m) {
    const auto [mag, neg] = round_magnitude(v, m);
    return neg ? -mag : mag;
}

// Kept exact as a rational, for indices that may pass imax.
[[nodiscard]] constexpr rational round_to_integral(rational v, round_mode m) {
    const auto [mag, neg] = round_magnitude(v, m);
    return make_raw(mag, (neg && mag != 0) ? imax{-1} : imax{1});
}

[[nodiscard]] constexpr imax trunc(rational v) { return round_to_int(v, round_mode::trunc); }
[[nodiscard]] constexpr imax floor(rational v) { return round_to_int(v, round_mode::floor); }
[[nodiscard]] constexpr imax ceil(rational v) { return round_to_int(v, round_mode::ceil); }
[[nodiscard]] constexpr imax round(rational v) { return round_to_int(v, round_mode::nearest); }

//---------------------------------------------------------------------------
// abs
//---------------------------------------------------------------------------
[[nodiscard]] constexpr rational abs(rational v) {
    if (v.Denominator < 0)
        v.Denominator = -v.Denominator;
    return v;
}

//---------------------------------------------------------------------------
// gcd
//---------------------------------------------------------------------------
// Returns errc::overflow if the combined denominator lcm = (a/gcd)·b would exceed
// imax_max (sign-bit reservation) — traps the mul_overflow then range-checks.
//---------------------------------------------------------------------------
[[nodiscard]] constexpr std::expected<rational, errc> gcd(const rational& lhs, const rational& rhs) {
    umax a = abs_den(lhs.Denominator);
    umax b = abs_den(rhs.Denominator);
    umax g = std::gcd(a, b);

    umax denominator;
    if (mul_overflow(a / g, b, &denominator))
        return std::unexpected{errc::overflow};
    if (denominator > static_cast<umax>(std::numeric_limits<imax>::max()))
        return std::unexpected{errc::overflow};

    auto numerator = std::gcd(lhs.Numerator, rhs.Numerator);
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
        return make_raw(0, 1);
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
    return make_raw(static_cast<umax>(num), v < 0 ? -den : den);
}

//---------------------------------------------------------------------------
// rational::rational
//---------------------------------------------------------------------------
constexpr rational::rational(std::signed_integral auto num, imax den)
    : Numerator{safe_abs(num)}, Denominator{signed_den_from(num, den)} {
    canonicalize(Numerator, Denominator);
}

constexpr rational::rational(std::floating_point auto value) {
    if (!is_finite(value))
        raise(errc::not_finite, "non-finite double");
    if constexpr (wide_float<decltype(value)>)
        if (!is_double_value(value)) {
            bool exact;
            *this = rational_of_wide_float(value, exact);
            return;
        }

    if (value == 0.0) {
        Numerator   = 0;
        Denominator = 1;
        return;
    }

    bool neg = (value < 0.0);
    if (neg)
        value = -value;

    auto [num, den] = abs_fraction(value);
    Numerator       = num;
    Denominator     = neg ? -den : den;
    // trim not needed, because abs_fraction already trims in its special case
}

// rational{value}, and whether it is exact: a finite |value| < 2^64 finer
// than 2^-62 loses its low bits (truncated toward zero).
constexpr rational rational_of_double(double value, bool& exact) {
    exact = true;
    if (value == 0.0)
        return rational{0};
    const auto [num, den] = abs_fraction(value < 0 ? -value : value, exact);
    return make_raw(num, value < 0 ? -static_cast<imax>(den) : static_cast<imax>(den));
}

//---------------------------------------------------------------------------
// to
//---------------------------------------------------------------------------
template <std::unsigned_integral T>
constexpr std::expected<T, errc> rational::to() const {
    if (Denominator < 0)
        return std::unexpected{errc::domain_error};
    const umax q = Numerator / abs_den(Denominator);
    if (q > static_cast<umax>(std::numeric_limits<T>::max()))
        return std::unexpected{errc::overflow};
    return static_cast<T>(q);
}

// per<D> and frac<N, D> are defined publicly in `namespace beman::inside` (see the
// re-export block at the end of this header) so consumers spell grid values
// without naming the internal representation type.

// a + b exactly, for the checked add once a 64-bit step overflowed: the
// numerator and lcm in 3 limbs, reduced, then checked to fit (a sum can
// reduce back into range: (2^63+1)/2 + 1/6 = (3·2^62+2)/3).
template <bool Loud>
BEMAN_INSIDE_COLD BEMAN_INSIDE_NOINLINE constexpr std::expected<rational, errc> add_wide(rational a, rational b) {
    using W       = wide_sint<3>;
    const umax ad = abs_den(a.Denominator), bd = abs_den(b.Denominator), g = std::gcd(ad, bd);
    const W    an  = a.Denominator < 0 ? -W{a.Numerator} : W{a.Numerator};
    const W    bn  = b.Denominator < 0 ? -W{b.Numerator} : W{b.Numerator};
    W          num = an * W{bd / g} + bn * W{ad / g};
    W          den = W{ad} * W{bd / g};
    if (num.is_zero())
        return rational{0};
    W x = abs(num), y = den; // gcd(|num|, den)
    while (!y.is_zero()) {
        const W t = x % y;
        x         = y;
        y         = t;
    }
    num            = num / x;
    den            = den / x;
    const bool neg = num.negative();
    const W    mag = neg ? -num : num;
    if (W{std::numeric_limits<umax>::max()} < mag || W{std::numeric_limits<imax>::max()} < den)
        return fail<"rational +: result out of the 64-bit range", Loud>(errc::overflow);
    const imax d = static_cast<imax>(static_cast<umax>(den));
    return make_raw(static_cast<umax>(mag), neg ? -d : d);
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
// add_impl / mul_impl / div_impl — shared bodies (Checked toggles overflow)
//---------------------------------------------------------------------------
template <bool Checked, bool Loud>
inline constexpr auto rational::add_impl(const rational& a, const rational& b) {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;

    // (a == −b needs no test: equal denominators, opposite signs → num == 0 below.)
    if (a.Numerator == 0)
        return ret_t{b};
    if (b.Numerator == 0)
        return ret_t{a};

    bool a_neg = a.Denominator < 0;
    bool b_neg = b.Denominator < 0;
    umax a_ad  = abs_den(a.Denominator);
    umax b_ad  = abs_den(b.Denominator);

    if (a_ad == b_ad) {
        if (a_neg == b_neg) {
            umax numerator;
            if constexpr (Checked) {
                if (add_overflow(a.Numerator, b.Numerator, &numerator)) {
                    return ret_t{fail<"rational +: numerator overflow (same denominator)", Loud>(errc::overflow)};
                }
            } else
                numerator = a.Numerator + b.Numerator;

            rational r;
            r.Numerator   = numerator;
            r.Denominator = a.Denominator;
            trim(r.Numerator, r.Denominator);
            return ret_t{r};
        }

        umax num   = (a.Numerator > b.Numerator) ? (a.Numerator - b.Numerator) : (b.Numerator - a.Numerator);
        bool r_neg = a_neg ? (a.Numerator > b.Numerator) : (b.Numerator > a.Numerator);
        if (num == 0)
            return ret_t{rational{0}};
        rational r;
        r.Numerator   = num;
        r.Denominator = r_neg ? -a_ad : a_ad;
        trim(r.Numerator, r.Denominator);
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
            denominator > static_cast<umax>(std::numeric_limits<imax>::max())) {
            return ret_t{fail<"rational +: denominator overflow", Loud>(errc::overflow)};
        }
        if (mul_overflow(a.Numerator, b_ad_r, &A) || mul_overflow(b.Numerator, a_ad_r, &B)) {
            // Mixed signs: |A − B| can fit umax even when a cross-product alone
            // does not (e.g. 1024 − m/2^54 forms 1024·2^54 == 2^64 before the
            // subtraction brings it back in range — the former double engine's store hit
            // exactly this). Retry the difference in 128-bit before giving up.
            if (a_neg != b_neg) {
                const limb::pair<umax> A2       = limb::mul(a.Numerator, b_ad_r);
                const limb::pair<umax> B2       = limb::mul(b.Numerator, a_ad_r);
                const bool             a_bigger = A2.Hi != B2.Hi ? A2.Hi > B2.Hi : A2.Lo > B2.Lo;
                const limb::pair<umax> big = a_bigger ? A2 : B2, small = a_bigger ? B2 : A2;
                if (big.Hi - small.Hi - (big.Lo < small.Lo ? 1u : 0u) == 0) {
                    rational r;
                    r.Numerator   = big.Lo - small.Lo;
                    r.Denominator = (a_neg ? a_bigger : !a_bigger) ? -denominator : denominator;
                    trim(r.Numerator, r.Denominator);
                    return ret_t{r};
                }
            }
            return ret_t{fail<"rational +: cross-multiplication overflow", Loud>(errc::overflow)};
        }
    } else {
        denominator = a_ad * b_ad_r;
        A           = a.Numerator * b_ad_r;
        B           = b.Numerator * a_ad_r;
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
        return ret_t{make_raw(numerator, a_neg ? -static_cast<imax>(denominator) : static_cast<imax>(denominator))};
    }

    // numerator == 0 (exact cancellation) is unreachable here: it would
    // require a == -b, which for canonical inputs has equal denominators and
    // took the equal-denominator branch above.
    umax       numerator = (A > B) ? (A - B) : (B - A);
    const bool r_neg     = a_neg ? (A > B) : (B > A);
    reduce_sum(numerator, denominator, g);
    return ret_t{make_raw(numerator, r_neg ? -static_cast<imax>(denominator) : static_cast<imax>(denominator))};
}

template <bool Checked, bool Loud>
inline constexpr auto rational::mul_impl(const rational& a_in, const rational& b_in) {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;
    rational a = a_in, b = b_in;

    if (a.Numerator == 0 || b.Numerator == 0)
        return ret_t{rational{0}};

    bool r_neg = (a.Denominator < 0) != (b.Denominator < 0);
    umax a_ad  = abs_den(a.Denominator);
    umax b_ad  = abs_den(b.Denominator);

    if (a_ad == 1 && b_ad == 1) {
        umax numerator;
        if constexpr (Checked) {
            if (mul_overflow(a.Numerator, b.Numerator, &numerator)) {
                return ret_t{fail<"rational *: numerator overflow", Loud>(errc::overflow)};
            }
        } else
            numerator = a.Numerator * b.Numerator;

        return ret_t{make_raw(numerator, r_neg ? imax{-1} : imax{1})};
    }

    trim(a.Numerator, b_ad);
    trim(b.Numerator, a_ad);

    umax numerator;
    umax denominator;
    if constexpr (Checked) {
        if (mul_overflow(a.Numerator, b.Numerator, &numerator) || mul_overflow(a_ad, b_ad, &denominator) ||
            denominator > static_cast<umax>(std::numeric_limits<imax>::max())) {
            return ret_t{fail<"rational *: numerator or denominator overflow", Loud>(errc::overflow)};
        }
    } else {
        numerator   = a.Numerator * b.Numerator;
        denominator = a_ad * b_ad;
    }

    // The cross-trims above guarantee gcd(numerator, denominator) == 1, so
    // bypass rational(num, den) and skip its redundant trim.
    return ret_t{make_raw(numerator, r_neg ? -denominator : denominator)};
}

//---------------------------------------------------------------------------
// inv_impl — multiplicative inverse (1 / a)
//---------------------------------------------------------------------------
// a is already trimmed (Numerator and |Denominator| coprime), so the swapped
// pair is also trimmed. Sign lives in the denominator and 1/(-x) has the same
// sign as -x, so the sign bit moves with the (now) denominator unchanged.
template <bool Checked, bool Loud>
inline constexpr auto rational::inv_impl(const rational& a) {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;

    if constexpr (Checked) {
        // a.Numerator goes into the result's Denominator slot, so it must fit in
        // imax (else the umax→imax conversion wraps and a later -Denominator is UB).
        if (a.Numerator == 0) {
            return ret_t{fail<"rational inv: division by zero", Loud>(errc::division_by_zero)};
        }
        if (a.Numerator > static_cast<umax>(std::numeric_limits<imax>::max())) {
            return ret_t{fail<"rational inv: numerator out of denominator range", Loud>(errc::overflow)};
        }
    }

    return ret_t{make_raw(abs_den(a.Denominator), signed_numerator(a))};
}

// div(a, b) = a * inv(b). The checked path goes through inv_impl<true> so
// the b.Numerator-fits-in-imax check (added there) propagates here too;
// the unchecked path skips it (caller's contract).
template <bool Checked, bool Loud>
inline constexpr auto rational::div_impl(const rational& a, const rational& b) {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;

    if constexpr (Checked) {
        // Cross-reduce a.N/b.N and b.D/a.D first: b.N need not fit the
        // denominator alone (x/x with x > 2^63 is 1).
        if (b.Numerator == 0)
            return ret_t{fail<"rational /: division by zero", Loud>(errc::division_by_zero)};
        if (a.Numerator == 0)
            return ret_t{rational{0}};
        const umax g1 = std::gcd(a.Numerator, b.Numerator);
        const umax g2 = std::gcd(abs_den(a.Denominator), abs_den(b.Denominator));
        umax       num, den;
        if (mul_overflow(a.Numerator / g1, abs_den(b.Denominator) / g2, &num) ||
            mul_overflow(abs_den(a.Denominator) / g2, b.Numerator / g1, &den) ||
            den > static_cast<umax>(std::numeric_limits<imax>::max()))
            return ret_t{fail<"rational /: numerator or denominator overflow", Loud>(errc::overflow)};
        const bool neg = (a.Denominator < 0) != (b.Denominator < 0);
        return ret_t{make_raw(num, neg ? -static_cast<imax>(den) : static_cast<imax>(den))};
    } else
        return mul_impl<false>(a, inv_impl<false>(b));
}

//---------------------------------------------------------------------------
// unchecked rational arithmetic — caller guarantees: no umax overflow on the
// products, no zero divisor/numerator, and the result Denominator fits in imax.
// The checked variants enforce all three; unchecked skips them.
//---------------------------------------------------------------------------
inline constexpr rational rational::add_unchecked(rational a, rational b) { return add_impl<false>(a, b); }

inline constexpr rational rational::mul_unchecked(rational a, rational b) { return mul_impl<false>(a, b); }

inline constexpr rational rational::div_unchecked(rational a, rational b) { return div_impl<false>(a, b); }

inline constexpr rational rational::inv_unchecked(rational a) { return inv_impl<false>(a); }

inline constexpr std::expected<rational, errc> rational::inv(rational a) { return inv_impl<true>(a); }

//---------------------------------------------------------------------------
// operator-
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr rational rational::operator-() const {
    if (Numerator == 0)
        return *this;

    // Already trimmed; flip the sign-encoding directly without re-running trim.
    return make_raw(Numerator, -Denominator);
}

//---------------------------------------------------------------------------
// operator<=>
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr auto operator<=>(rational lhs, rational rhs) -> std::strong_ordering {
    int lhs_sign = sign(lhs);
    int rhs_sign = sign(rhs);

    if (lhs_sign != rhs_sign)
        return lhs_sign <=> rhs_sign;

    if (lhs_sign == 0)
        return std::strong_ordering::equal;

    // signs are equal here (the `lhs_sign != rhs_sign` branch returned above)
    bool lhs_neg = lhs_sign < 0;

    umax lhs_ad = abs_den(lhs.Denominator);
    umax rhs_ad = abs_den(rhs.Denominator);

    // integer comparison: skip cross-multiply entirely
    if (lhs_ad == 1 && rhs_ad == 1) {
        if (lhs_neg)
            return rhs.Numerator <=> lhs.Numerator;
        else
            return lhs.Numerator <=> rhs.Numerator;
    }

    // One side is an integer: compare via divmod instead of cross-multiply.
    // Cross-multiplying would otherwise overflow when the non-integer side
    // has a huge denominator (e.g. doubles like 19.99 stored as N/2^48).
    if (rhs_ad == 1) {
        umax q   = lhs.Numerator / lhs_ad;
        umax r   = lhs.Numerator % lhs_ad;
        auto cmp = (q == rhs.Numerator) ? (r == 0 ? std::strong_ordering::equal : std::strong_ordering::greater)
                                        : (q <=> rhs.Numerator);
        return lhs_neg ? (0 <=> cmp) : cmp;
    }
    if (lhs_ad == 1) {
        umax q   = rhs.Numerator / rhs_ad;
        umax r   = rhs.Numerator % rhs_ad;
        auto cmp = (q == lhs.Numerator) ? (r == 0 ? std::strong_ordering::equal : std::strong_ordering::less)
                                        : (lhs.Numerator <=> q);
        return lhs_neg ? (0 <=> cmp) : cmp;
    }

    // Cross-multiply in 128-bit: |numerator| and |denominator| are each ≤ 2^64−1,
    // so the products fit exactly in 128 bits — the comparison can never overflow,
    // so no trap is needed.
    return lhs_neg ? limb::mul_compare(rhs.Numerator, lhs_ad, lhs.Numerator, rhs_ad)
                   : limb::mul_compare(lhs.Numerator, rhs_ad, rhs.Numerator, lhs_ad);
}

template <typename T>
[[nodiscard]] inline constexpr auto operator<=>(const std::expected<T, errc>& lhs, const rational& rhs) {
    return rational{lhs.value()} <=> rhs;
}

template <typename T>
[[nodiscard]] inline constexpr auto operator<=>(const rational& lhs, const std::expected<T, errc>& rhs) {
    return lhs <=> rational{rhs.value()};
}

// lhs against a finite double finer than 2^-62 (which rational{rhs}
// truncates): |lhs| = N/D against |rhs| = s·2^-k (k > 62) as N·2^k against
// s·D, which stays within two 64-bit factors. Cold and out of line: the
// stores compare doubles with their limits.
[[nodiscard]] BEMAN_INSIDE_COLD BEMAN_INSIDE_NOINLINE constexpr std::strong_ordering
compare_fine_double(const rational& lhs, double rhs) {
    const int ls = sign(lhs), rs = rhs < 0 ? -1 : 1;
    if (ls != rs)
        return ls <=> rs;
    const auto bits = std::bit_cast<std::uint64_t>(rhs) & 0x7FFF'FFFF'FFFF'FFFF;
    const int  e    = static_cast<int>(bits >> 52);
    const umax s    = (bits & 0x000F'FFFF'FFFF'FFFF) | (e != 0 ? umax{1} << 52 : 0);
    const int  k    = e != 0 ? 1023 + 52 - e : 1023 + 52 - 1; // rhs = s·2^-k, k > 62
    const umax n = lhs.Numerator, d = abs_den(lhs.Denominator);
    // s·D < 2^117: past that width N·2^k is larger.
    const auto mag = std::bit_width(n) + k > 117
                         ? std::strong_ordering::greater
                         : limb::mul_compare(n << (k - 63), umax{1} << 63, s, d); // N·2^(k−63) < 2^54
    return ls < 0 ? (0 <=> mag) : mag;
}

// lhs against a double, exactly (non-finite raises, as rational{rhs}). Every
// rational lies strictly inside ±2^64, so a larger double is decided by its sign.
[[nodiscard]] inline constexpr std::strong_ordering compare_double(const rational& lhs, double rhs) {
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
[[nodiscard]] constexpr std::strong_ordering compare_float(const rational& lhs, T rhs) {
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
                mag                  = (W{lhs.Numerator} << wide_float_shift) <=> W{m} * W{abs_den(lhs.Denominator)};
                if (mag == 0)
                    mag = 0 <=> tail;
            }
            return ls < 0 ? (0 <=> mag) : mag;
        }
    return compare_double(lhs, static_cast<double>(rhs));
}

template <arithmetic T>
[[nodiscard]] constexpr bool rational::operator==(T value) const {
    if constexpr (std::floating_point<T>)
        return compare_float(*this, value) == 0;
    else
        return operator==(rational{value});
}

template <arithmetic T>
[[nodiscard]] inline constexpr auto operator<=>(T lhs, const rational& rhs) {
    if constexpr (std::floating_point<T>)
        return 0 <=> compare_float(rhs, lhs);
    else
        return rational{lhs} <=> rhs;
}

template <arithmetic T>
[[nodiscard]] inline constexpr auto operator<=>(const rational& lhs, T rhs) {
    if constexpr (std::floating_point<T>)
        return compare_float(lhs, rhs);
    else
        return lhs <=> rational{rhs};
}

//---------------------------------------------------------------------------
// Expected-lifting operators — one generic overload per arithmetic operator
// that engages when an operand is a std::expected, both unwrap to arithmetic,
// and at least one to rational. Gating on `arithmetic` (not `insidable`, which
// isn't visible this low) excludes inside operands, so inside-involving expected
// expressions partition cleanly to arithmetic.hpp's generic instead.
//---------------------------------------------------------------------------
template <class L, class R>
concept rational_lift_operands =
    (is_expected_v<L> || is_expected_v<R>) && arithmetic<unwrap_t<L>> && arithmetic<unwrap_t<R>> &&
    (std::same_as<unwrap_t<L>, rational> || std::same_as<unwrap_t<R>, rational>);

//---------------------------------------------------------------------------
// Binary operators: the checked rational ⋈ rational cores, then per operator
// the arithmetic-operand forms (direct construction, no lift overhead), the
// expected-operand form (propagates via lift), and the compound assignments.
// Compound assignments unwrap with .value() — std::bad_expected_access on
// overflow; callers needing a non-throwing path use the binary operators.
//---------------------------------------------------------------------------
// add_impl fails on a 64-bit step's overflow; the sum may still fit once
// reduced, which add_wide (cold) settles.
[[nodiscard]] inline constexpr std::expected<rational, errc> operator+(const rational& lhs, const rational& rhs) {
    const auto r = rational::add_impl<true, false>(lhs, rhs);
    if (!r) [[unlikely]]
        return add_wide<true>(lhs, rhs);
    return r;
}

[[nodiscard]] inline constexpr std::expected<rational, errc> operator-(const rational& lhs, const rational& rhs) {
    return operator+(lhs, -rhs);
}

[[nodiscard]] inline constexpr std::expected<rational, errc> operator*(const rational& lhs, const rational& rhs) {
    return rational::mul_impl<true>(lhs, rhs);
}

[[nodiscard]] inline constexpr std::expected<rational, errc> operator/(const rational& lhs, const rational& rhs) {
    return rational::div_impl<true>(lhs, rhs);
}

// Quiet checked ops: like the operators, but an overflow is an error value
// even in constant evaluation (the operators make it a compile error there).
// For compile-time code that asks whether a result fits.
[[nodiscard]] inline constexpr std::expected<rational, errc> try_add(const rational& a, const rational& b) {
    const auto r = rational::add_impl<true, false>(a, b);
    if (!r) [[unlikely]]
        return add_wide<false>(a, b);
    return r;
}
[[nodiscard]] inline constexpr std::expected<rational, errc> try_sub(const rational& a, const rational& b) {
    return try_add(a, -b);
}
[[nodiscard]] inline constexpr std::expected<rational, errc> try_mul(const rational& a, const rational& b) {
    return rational::mul_impl<true, false>(a, b);
}
[[nodiscard]] inline constexpr std::expected<rational, errc> try_div(const rational& a, const rational& b) {
    return rational::div_impl<true, false>(a, b);
}

[[nodiscard]] inline constexpr std::expected<rational, errc> operator-(const std::expected<rational, errc>& v) {
    return lift([](rational r) { return -r; }, v);
}

#define BEMAN_INSIDE_RATIONAL_OP(op)                                                                        \
    template <arithmetic T>                                                                                 \
    [[nodiscard]] inline constexpr auto operator op(T lhs, rational const& rhs) {                           \
        return rational{lhs} op rhs;                                                                        \
    }                                                                                                       \
    template <arithmetic T>                                                                                 \
    [[nodiscard]] inline constexpr auto operator op(rational const& lhs, T rhs) {                           \
        return lhs op rational{rhs};                                                                        \
    }                                                                                                       \
    template <class L, class R>                                                                             \
        requires rational_lift_operands<L, R>                                                               \
    [[nodiscard]] inline constexpr auto operator op(L const& lhs, R const& rhs) {                           \
        return lift([](auto const& a, auto const& b) { return a op b; }, lhs, rhs);                         \
    }                                                                                                       \
    inline constexpr rational& rational::operator op## = (rational const& rhs) {                            \
        *this = (*this op rhs).value();                                                                     \
        return *this;                                                                                       \
    }                                                                                                       \
    template <arithmetic T>                                                                                 \
    inline constexpr rational& operator op## = (rational & lhs, T rhs) {                                    \
        return lhs op## = rational{rhs};                                                                    \
    }                                                                                                       \
    inline constexpr rational& operator op## = (rational & lhs, std::expected<rational, errc> const& rhs) { \
        return lhs op## = rhs.value();                                                                      \
    }

BEMAN_INSIDE_RATIONAL_OP(+)
BEMAN_INSIDE_RATIONAL_OP(-)
BEMAN_INSIDE_RATIONAL_OP(*)
BEMAN_INSIDE_RATIONAL_OP(/)
#undef BEMAN_INSIDE_RATIONAL_OP

//---------------------------------------------------------------------------
// divides_evenly
//---------------------------------------------------------------------------
[[nodiscard]] inline constexpr bool divides_evenly(const rational& dividend, const rational& divisor) {
    if (divisor == 0)
        return true; // convention: everything divides 0 evenly
    if (dividend.Numerator == 0)
        return true; // 0 / anything is the integer 0

    // dividend/divisor ∈ ℤ without forming the (possibly umax-overflowing)
    // quotient numerator. In lowest terms dividend = p/q, divisor = r/s; the
    // quotient p·s/(q·r) is integral iff q | s and r | p (rationals are
    // canonicalized, so gcd(p,q)=gcd(r,s)=1). All checks are on single fields.
    const umax p = dividend.Numerator, q = abs_den(dividend.Denominator);
    const umax r = divisor.Numerator, s = abs_den(divisor.Denominator);
    return (s % q == 0) && (p % r == 0);
}

} // namespace beman::inside::detail

namespace beman::inside::detail {
// Checked builders for the public helpers below: a static_assert here fires at
// the user's spelling. Denominators are unsigned, so a sign can only sit in
// frac's numerator.
template <umax D>
consteval rational make_per() {
    static_assert(D >= 1, "per<D> is the positive step 1/D; a continuous grid is spelled 0");
    static_assert(D <= static_cast<umax>(std::numeric_limits<imax>::max()), "per<D>: denominator too large");
    return rational{umax{1}, D};
}

template <imax N, umax D>
consteval rational make_frac() {
    static_assert(D >= 1, "frac<N, D>: the denominator must be at least 1");
    static_assert(D <= static_cast<umax>(std::numeric_limits<imax>::max()), "frac<N, D>: denominator too large");
    return rational{N, static_cast<imax>(D)};
}
} // namespace beman::inside::detail

namespace beman::inside {
// `rational` — an exact 64-bit fraction: the value of a literal (`0.1_r`), a
// continuous inside's raw, and a runtime exact value to compute with.
using detail::rational;

// The grid-building helpers:
//   per<D>       the step 1/D — the common notch (per<256> is Q·8);
//   frac<N, D>   any exact ratio (signed numerator): a limit like frac<-6, 5>
//                for -1.2, or another step like frac<360, 4096>;
// (the _r literal, an exact decimal / hex value — 0.1_r is exactly 1/10 —
// is in detail/number_text.hpp.)
// Integer steps are plain integers ({{0, 100}, 5}); grid::validate rejects a
// negative notch however it is spelled.
template <umax D>
inline constexpr detail::rational per = detail::make_per<D>();

template <imax N, umax D = 1>
inline constexpr detail::rational frac = detail::make_frac<N, D>();

} // namespace beman::inside

#endif // BEMAN_INSIDE_DETAIL_RATIONAL_HPP
