// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_RATIONAL_HPP
#define BEMAN_INSIDE_DETAIL_RATIONAL_HPP

#include <beman/inside/math.hpp>            // umax/imax, arithmetic, rational fwd
#include <beman/inside/lift.hpp>            // lift, is_expected_v, unwrap_t
#include <beman/inside/detail/overflow.hpp> // add/sub/mul_overflow
#include <beman/inside/detail/debug.hpp>    // errc, detail::raise, detail::constexpr_error
#include <beman/inside/detail/wide_int.hpp> // limb kernels for exact 128-bit cross products
#include <beman/inside/detail/rounding.hpp> // round_mode, rounds_away

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
    constexpr rational(std::floating_point auto);
    constexpr rational(std::signed_integral auto, imax = 1);
    constexpr rational(std::unsigned_integral auto num, imax den = 1) : Numerator{num}, Denominator{den} {
        canonicalize(Numerator, Denominator);
    }

    // Two-unsigned overload: lets `rational{i, N}` accept two `size_t`
    // operands without forcing the caller to `static_cast<imax>` the
    // numerator. Numerator is unsigned anyway; the cast on `den` is
    // safe because the unsigned-integral concept exclude negative inputs.
    template <std::unsigned_integral N, std::unsigned_integral D>
    constexpr rational(N num, D den) : Numerator{num}, Denominator{static_cast<imax>(den)} {
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
    template <arithmetic T>
    [[nodiscard]] constexpr bool operator==(T value) const {
        return operator==(rational{value});
    }

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
[[nodiscard]] constexpr imax signed_numerator(rational v) noexcept {
    const imax n = static_cast<imax>(v.Numerator);
    return (v.Denominator < 0) ? -n : n;
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
// rational::rational
//---------------------------------------------------------------------------
constexpr rational::rational(std::signed_integral auto num, imax den)
    : Numerator{safe_abs(num)}, Denominator{signed_den_from(num, den)} {
    canonicalize(Numerator, Denominator);
}

constexpr rational::rational(std::floating_point auto value) {
    if (not is_finite(value))
        raise(errc::not_finite, "non-finite double");

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

//---------------------------------------------------------------------------
// _ins / _r literal parser — shared between core.hpp's `_ins` and `_r` below.
// Accepts:
//   integer:           5, 1'000
//   decimal:           1.25, .5
//   decimal scientific 1.5e2, 2.5e-1
//   hex integer:       0xff
//   binary integer:    0b1010
//   hex float (Q-fmt): 0x1p15, 0x1p-15, 0x1.8p3
// Exact (no double round-trip). A malformed or overflowing literal fails
// constant evaluation via `constexpr_error<Msg>()`; parse_text (below) is the
// runtime form behind from_chars.
//---------------------------------------------------------------------------
constexpr int parse_digit(char c, int base) {
    if (c >= '0' && c <= '9') {
        int d = c - '0';
        return d < base ? d : -1;
    }
    if (base == 16) {
        if (c >= 'a' && c <= 'f')
            return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F')
            return 10 + (c - 'A');
    }
    return -1;
}

// Failure kinds of the number parser. The literals turn each into a named
// compile-time diagnostic; runtime parsing maps them onto errc.
enum class parse_fail : unsigned char {
    none,
    empty,
    invalid_exponent,
    multiple_dots,
    dot_in_binary,
    invalid_digit,
    numerator_overflow,
    denominator_overflow,
    hex_fraction_too_long,
    p_exponent_too_large,
    p_exponent_denominator_overflow,
    e_exponent_numerator_overflow,
    e_exponent_denominator_overflow,
};

struct parsed_number {
    rational   Value;
    parse_fail Fail;
};

// Unsigned number in [first, last): the grammar of the _ins / _r literals.
constexpr parsed_number parse_number(const char* first, const char* last) {
    const std::size_t N    = static_cast<std::size_t>(last - first);
    auto              fail = [](parse_fail f) { return parsed_number{rational{}, f}; };

    // Detect radix prefix.
    int         base = 10;
    std::size_t i    = 0;
    if (N >= 2 && first[0] == '0') {
        if (first[1] == 'x' || first[1] == 'X') {
            base = 16;
            i    = 2;
        } else if (first[1] == 'b' || first[1] == 'B') {
            base = 2;
            i    = 2;
        }
    }

    umax num            = 0;
    int  frac_len       = 0;
    bool in_frac        = false;
    bool seen_digit     = false;
    int  exp            = 0;
    bool exp_neg        = false;
    bool has_p_exp      = false; // 2^exp (hex floats)
    bool has_e_exp      = false; // 10^exp (decimal scientific)
    bool in_exp         = false;
    bool exp_seen_digit = false;
    int  frac_zeros     = 0; // deferred fractional zeros (see the digit loop)

    for (; i < N; ++i) {
        const char c = first[i];
        if (c == '\'')
            continue;

        if (in_exp) {
            if (!exp_seen_digit && (c == '+' || c == '-')) {
                exp_neg = (c == '-');
                continue;
            }
            if (c >= '0' && c <= '9') {
                if (exp < 100000)
                    exp = exp * 10 + (c - '0'); // past 10^5 it overflows anyway
                exp_seen_digit = true;
                continue;
            }
            return fail(parse_fail::invalid_exponent);
        }

        if (c == '.') {
            if (in_frac)
                return fail(parse_fail::multiple_dots);
            if (base == 2)
                return fail(parse_fail::dot_in_binary);
            in_frac = true;
            continue;
        }

        if ((c == 'p' || c == 'P') && base == 16) {
            has_p_exp = true;
            in_exp    = true;
            continue;
        }

        if ((c == 'e' || c == 'E') && base == 10) {
            has_e_exp = true;
            in_exp    = true;
            continue;
        }

        const int d = parse_digit(c, base);
        if (d < 0)
            return fail(parse_fail::invalid_digit);
        seen_digit = true;

        const umax base_u = static_cast<umax>(base);
        // A fractional zero only matters if a non-zero digit follows: defer it,
        // so trailing zeros ("1.000…0") cannot overflow num / den.
        if (in_frac && d == 0) {
            ++frac_zeros;
            continue;
        }
        for (; frac_zeros > 0; --frac_zeros, ++frac_len) {
            if (num > (~umax{0}) / base_u)
                return fail(parse_fail::numerator_overflow);
            num *= base_u;
        }
        if (num > (~umax{0} - static_cast<umax>(d)) / base_u)
            return fail(parse_fail::numerator_overflow);
        num = num * base_u + static_cast<umax>(d);
        if (in_frac)
            ++frac_len;
    }
    if (!seen_digit)
        return fail(parse_fail::empty);
    if (in_exp && !exp_seen_digit)
        return fail(parse_fail::invalid_exponent);

    // Build denominator from fractional part.
    // For decimal: den = 10^frac_len. For hex: den = 2^(4*frac_len).
    umax den = 1;
    if (base == 10) {
        for (int k = 0; k < frac_len; ++k) {
            if (den > (~umax{0}) / 10u)
                return fail(parse_fail::denominator_overflow);
            den *= 10u;
        }
    } else if (base == 16) {
        const int shift = 4 * frac_len;
        if (shift >= 64)
            return fail(parse_fail::hex_fraction_too_long);
        den <<= shift;
    }

    // Zero stays zero whatever the exponent: skip the scaling (which could
    // only overflow).
    if (num == 0)
        return parsed_number{rational{umax{0}}, parse_fail::none};

    // Apply binary exponent (hex floats, `p`).
    if (has_p_exp) {
        if (exp >= 63)
            return fail(parse_fail::p_exponent_too_large);
        if (!exp_neg) {
            if (num > (~umax{0}) >> exp)
                return fail(parse_fail::numerator_overflow);
            num <<= exp;
        } else {
            if (den > (~umax{0}) >> exp)
                return fail(parse_fail::p_exponent_denominator_overflow);
            den <<= exp;
        }
    }

    // Apply decimal exponent (decimal scientific, `e`).
    if (has_e_exp) {
        for (int k = 0; k < exp; ++k) {
            if (!exp_neg) {
                if (num > (~umax{0}) / 10u)
                    return fail(parse_fail::e_exponent_numerator_overflow);
                num *= 10u;
            } else {
                if (den > (~umax{0}) / 10u)
                    return fail(parse_fail::e_exponent_denominator_overflow);
                den *= 10u;
            }
        }
    }

    if (den > static_cast<umax>(std::numeric_limits<imax>::max()))
        return fail(parse_fail::denominator_overflow);
    return parsed_number{rational{num, den}, parse_fail::none};
}

template <char... Chars>
consteval rational parse_ins_literal() {
    constexpr char          src[] = {Chars..., '\0'};
    constexpr parsed_number r     = parse_number(src, src + sizeof...(Chars));
    if constexpr (r.Fail == parse_fail::invalid_exponent)
        constexpr_error<"_ins/_r literal: invalid char in exponent">();
    else if constexpr (r.Fail == parse_fail::multiple_dots)
        constexpr_error<"_ins/_r literal: multiple '.'">();
    else if constexpr (r.Fail == parse_fail::dot_in_binary)
        constexpr_error<"_ins/_r literal: '.' not allowed in binary">();
    else if constexpr (r.Fail == parse_fail::invalid_digit || r.Fail == parse_fail::empty)
        constexpr_error<"_ins/_r literal: invalid digit for radix">();
    else if constexpr (r.Fail == parse_fail::numerator_overflow)
        constexpr_error<"_ins/_r literal: numerator overflow">();
    else if constexpr (r.Fail == parse_fail::denominator_overflow)
        constexpr_error<"_ins/_r literal: denominator overflow">();
    else if constexpr (r.Fail == parse_fail::hex_fraction_too_long)
        constexpr_error<"_ins/_r literal: hex fraction too long">();
    else if constexpr (r.Fail == parse_fail::p_exponent_too_large)
        constexpr_error<"_ins/_r literal: p exponent too large">();
    else if constexpr (r.Fail == parse_fail::p_exponent_denominator_overflow)
        constexpr_error<"_ins/_r literal: p exponent denominator overflow">();
    else if constexpr (r.Fail == parse_fail::e_exponent_numerator_overflow)
        constexpr_error<"_ins/_r literal: e exponent numerator overflow">();
    else if constexpr (r.Fail == parse_fail::e_exponent_denominator_overflow)
        constexpr_error<"_ins/_r literal: e exponent denominator overflow">();
    return r.Value;
}

// Runtime text → exact value: an optional sign, then a number in the literal
// grammar, or a fraction `N/D` of two such numbers (the form to_string prints
// for a non-terminating value). Malformed text is errc::invalid_format; a value
// past the 64-bit fields is errc::overflow; `N/0` is errc::division_by_zero.
constexpr std::expected<rational, errc> parse_text(const char* first, const char* last) {
    bool neg = false;
    if (first != last && (*first == '+' || *first == '-')) {
        neg = (*first == '-');
        ++first;
    }
    const char* slash = first;
    while (slash != last && *slash != '/')
        ++slash;

    auto one = [](const char* f, const char* l) -> std::expected<rational, errc> {
        const parsed_number r = parse_number(f, l);
        switch (r.Fail) {
        case parse_fail::none:
            return r.Value;
        case parse_fail::numerator_overflow:
        case parse_fail::denominator_overflow:
        case parse_fail::hex_fraction_too_long:
        case parse_fail::p_exponent_too_large:
        case parse_fail::p_exponent_denominator_overflow:
        case parse_fail::e_exponent_numerator_overflow:
        case parse_fail::e_exponent_denominator_overflow:
            return std::unexpected{errc::overflow};
        default:
            return std::unexpected{errc::invalid_format};
        }
    };

    std::expected<rational, errc> v = one(first, slash);
    if (v && slash != last) {
        const auto d = one(slash + 1, last);
        if (!d)
            return d;
        if (d->Numerator == 0)
            return std::unexpected{errc::division_by_zero};
        v = *v / *d;
    }
    if (v && neg)
        v = -*v;
    return v;
}

template <char... Chars>
constexpr rational operator""_r() {
    return parse_ins_literal<Chars...>();
}

// per<D> and frac<N, D> are defined publicly in `namespace beman::inside` (see the
// re-export block at the end of this header) so consumers spell grid values
// without naming the internal representation type.

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
            return ret_t{0_r};
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
        // assemble directly and trim.
        rational r;
        r.Numerator   = numerator;
        r.Denominator = a_neg ? -denominator : denominator;
        trim(r.Numerator, r.Denominator);
        return ret_t{r};
    }

    // numerator == 0 (exact cancellation) is unreachable here: it would
    // require a == -b, which for canonical inputs has equal denominators and
    // took the equal-denominator branch above.
    umax     numerator = (A > B) ? (A - B) : (B - A);
    bool     r_neg     = a_neg ? (A > B) : (B > A);
    rational r;
    r.Numerator   = numerator;
    r.Denominator = r_neg ? -denominator : denominator;
    trim(r.Numerator, r.Denominator);
    return ret_t{r};
}

template <bool Checked, bool Loud>
inline constexpr auto rational::mul_impl(const rational& a_in, const rational& b_in) {
    using ret_t = std::conditional_t<Checked, std::expected<rational, errc>, rational>;
    rational a = a_in, b = b_in;

    if (a.Numerator == 0 || b.Numerator == 0)
        return ret_t{0_r};

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
        auto inv_b = inv_impl<true, Loud>(b);
        if (!inv_b.has_value())
            return ret_t{std::unexpected{inv_b.error()}};
        return mul_impl<true, Loud>(a, *inv_b);
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

template <arithmetic T>
[[nodiscard]] inline constexpr auto operator<=>(T lhs, const rational& rhs) {
    return rational{lhs} <=> rhs;
}

template <arithmetic T>
[[nodiscard]] inline constexpr auto operator<=>(const rational& lhs, T rhs) {
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
[[nodiscard]] inline constexpr std::expected<rational, errc> operator+(const rational& lhs, const rational& rhs) {
    return rational::add_impl<true>(lhs, rhs);
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
    return rational::add_impl<true, false>(a, b);
}
[[nodiscard]] inline constexpr std::expected<rational, errc> try_sub(const rational& a, const rational& b) {
    return rational::add_impl<true, false>(a, -b);
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
//   _r literal   an exact decimal / hex value, e.g. 0.1_r is exactly 1/10.
// Integer steps are plain integers ({{0, 100}, 5}); grid::validate rejects a
// negative notch however it is spelled.
template <umax D>
inline constexpr detail::rational per = detail::make_per<D>();

template <imax N, umax D = 1>
inline constexpr detail::rational frac = detail::make_frac<N, D>();

using detail::operator""_r;
} // namespace beman::inside

#endif // BEMAN_INSIDE_DETAIL_RATIONAL_HPP
