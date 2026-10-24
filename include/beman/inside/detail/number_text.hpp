// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_NUMBER_TEXT_HPP
#define BEMAN_INSIDE_DETAIL_NUMBER_TEXT_HPP

#include <beman/inside/detail/config.hpp>
BEMAN_INSIDE_WARNINGS_PUSH

#include <beman/inside/detail/rational.hpp> // rational, its checked arithmetic
#include <beman/inside/detail/debug.hpp>    // errc, constexpr_error

#include <cstddef>
#include <expected>
#include <limits>

namespace beman::inside::detail {
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
// runtime form behind parse.
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

// An unsigned number in the grammar of the _ins / _r literals, scanned in
// one pass: the radix and the exponent come back, and each mantissa digit
// goes to on_digit(d, in_frac, base) (false stops the scan: the builder's
// overflow). parse_number (64-bit), parse_exact (wide, wide_value.hpp) and
// stand_in build from it, so all read one grammar.
struct scanned_number {
    int        Base    = 10;
    int        Exp     = 0; // capped at 100000: past it any value overflows
    bool       ExpNeg  = false;
    bool       HasP    = false; // 2^Exp (hex floats)
    bool       HasE    = false; // 10^Exp (decimal scientific)
    bool       Stopped = false; // on_digit returned false
    parse_fail Fail    = parse_fail::none;
};

template <typename OnDigit>
BEMAN_INSIDE_ALWAYS_INLINE constexpr scanned_number
scan_number(const char* first, const char* last, OnDigit&& on_digit) {
    scanned_number r;
    auto           fail = [&](parse_fail f) {
        r.Fail = f;
        return r;
    };
    if (last - first >= 2 && first[0] == '0') { // radix prefix
        if (first[1] == 'x' || first[1] == 'X') {
            r.Base = 16;
            first += 2;
        } else if (first[1] == 'b' || first[1] == 'B') {
            r.Base = 2;
            first += 2;
        }
    }
    bool in_frac = false, seen_digit = false;
    for (; first != last; ++first) {
        const char c = *first;
        if (c == '\'')
            continue;
        if (c == '.') {
            if (in_frac)
                return fail(parse_fail::multiple_dots);
            if (r.Base == 2)
                return fail(parse_fail::dot_in_binary);
            in_frac = true;
            continue;
        }
        if (((c == 'p' || c == 'P') && r.Base == 16) || ((c == 'e' || c == 'E') && r.Base == 10))
            break;
        const int d = parse_digit(c, r.Base);
        if (d < 0)
            return fail(parse_fail::invalid_digit);
        seen_digit = true;
        if (!r.Stopped && !on_digit(d, in_frac, r.Base))
            r.Stopped = true; // keep scanning: malformed text still reports invalid_format
    }
    if (!seen_digit)
        return fail(parse_fail::empty);
    if (first == last)
        return r;
    (r.Base == 16 ? r.HasP : r.HasE) = true;
    bool exp_seen_digit = false, exp_seen_sign = false; // one sign, right after e/p
    for (++first; first != last; ++first) {
        const char c = *first;
        if (c == '\'')
            continue;
        if (!exp_seen_digit && !exp_seen_sign && (c == '+' || c == '-')) {
            r.ExpNeg      = (c == '-');
            exp_seen_sign = true;
            continue;
        }
        if (c < '0' || c > '9')
            return fail(parse_fail::invalid_exponent);
        if (r.Exp < 100000)
            r.Exp = r.Exp * 10 + (c - '0');
        exp_seen_digit = true;
    }
    if (!exp_seen_digit)
        return fail(parse_fail::invalid_exponent);
    return r;
}

// Unsigned number in [first, last): the grammar of the _ins / _r literals.
constexpr parsed_number parse_number(const char* first, const char* last) {
    auto fail       = [](parse_fail f) { return parsed_number{rational{}, f}; };
    umax num        = 0;
    int  frac_len   = 0;
    int  frac_zeros = 0; // deferred fractional zeros
    // A fractional zero only matters if a non-zero digit follows: defer it,
    // so trailing zeros ("1.000…0") cannot overflow num / den.
    const scanned_number sc = scan_number(first, last, [&](int d, bool in_frac, int base) {
        const umax base_u = static_cast<umax>(base);
        if (in_frac && d == 0) {
            ++frac_zeros;
            return true;
        }
        for (; frac_zeros > 0; --frac_zeros, ++frac_len) {
            if (num > (~umax{0}) / base_u)
                return false;
            num *= base_u;
        }
        if (num > (~umax{0} - static_cast<umax>(d)) / base_u)
            return false;
        num = num * base_u + static_cast<umax>(d);
        if (in_frac)
            ++frac_len;
        return true;
    });
    if (sc.Fail != parse_fail::none)
        return fail(sc.Fail);
    if (sc.Stopped)
        return fail(parse_fail::numerator_overflow);
    const int  base      = sc.Base;
    const int  exp       = sc.Exp;
    const bool exp_neg   = sc.ExpNeg;
    const bool has_p_exp = sc.HasP;
    const bool has_e_exp = sc.HasE;

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
    // The factors of two cancel first (0x0.1p4 is 1, 0x10p-66 is 2^-62), so
    // only a value past the 64-bit rationals fails; 2^63 itself fits.
    if (has_p_exp) {
        umax&     other  = exp_neg ? num : den; // the side the shift cancels against
        const int tz     = std::countr_zero(other);
        const int cancel = tz < exp ? tz : exp;
        other >>= cancel;
        const int e = exp - cancel;
        if (e >= 64)
            return fail(parse_fail::p_exponent_too_large);
        if (!exp_neg) {
            if (num > (~umax{0}) >> e)
                return fail(parse_fail::numerator_overflow);
            num <<= e;
        } else {
            if (den > (~umax{0}) >> e)
                return fail(parse_fail::p_exponent_denominator_overflow);
            den <<= e;
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

    // In lowest terms before the signed-denominator check (0.5e-18 is
    // 1/(2·10^18), though 10^19 itself is past imax).
    const umax g = std::gcd(num, den);
    num /= g;
    den /= g;
    if (den > static_cast<umax>(std::numeric_limits<imax>::max()))
        return fail(parse_fail::denominator_overflow);
    return parsed_number{rational{num, den}, parse_fail::none};
}

// Whether a numeric literal's characters are an octal integer to C++: a
// leading 0, then only digits (and ' separators). The literal grammar reads
// decimal, so 010 would silently be ten, not eight: such literals are refused.
template <char... Chars>
consteval bool octal_literal() {
    constexpr char text[] = {Chars..., '\0'};
    if (sizeof...(Chars) < 2 || text[0] != '0')
        return false;
    for (std::size_t i = 1; i < sizeof...(Chars); ++i)
        if (text[i] != '\'' && (text[i] < '0' || text[i] > '9'))
            return false;
    return true;
}

template <char... Chars>
consteval rational parse_ins_literal() {
    constexpr char          src[] = {Chars..., '\0'};
    constexpr parsed_number r     = parse_number(src, src + sizeof...(Chars));
    if constexpr (octal_literal<Chars...>())
        constexpr_error<"_ins/_r literal: a leading 0 makes an octal literal; write the number without it">();
    else if constexpr (r.Fail == parse_fail::invalid_exponent)
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
// Text as an optional sign and the number's [First, Slash), with the
// denominator after Slash when Slash != last.
struct signed_fraction_text {
    bool        Neg;
    const char* First;
    const char* Slash;
};
constexpr signed_fraction_text split_fraction(const char* first, const char* last) noexcept {
    const bool neg = first != last && *first == '-';
    if (first != last && (*first == '+' || *first == '-'))
        ++first;
    const char* slash = first;
    while (slash != last && *slash != '/')
        ++slash;
    return {neg, first, slash};
}

// The sign and N/D around `one` (text → an expected value): N/0 is
// division_by_zero.
template <class One, class IsZero>
constexpr auto parse_fraction(const char* first, const char* last, One one, IsZero is_zero) {
    const auto [neg, f, slash] = split_fraction(first, last);
    auto v                     = one(f, slash);
    if (v && slash != last) {
        const auto d = one(slash + 1, last);
        if (!d)
            return d;
        if (is_zero(*d))
            return decltype(v){std::unexpect, errc::division_by_zero};
        v = *v / *d;
    }
    if (v && neg)
        v = -*v;
    return v;
}

constexpr std::expected<rational, errc> parse_text(const char* first, const char* last) {
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
    return parse_fraction(first, last, one, [](const rational& d) { return d.Numerator == 0; });
}

template <char... Chars>
constexpr rational operator""_r() {
    return parse_ins_literal<Chars...>();
}
} // namespace beman::inside::detail

namespace beman::inside {
//   _r literal   an exact decimal / hex value, e.g. 0.1_r is exactly 1/10.
using detail::operator""_r;
} // namespace beman::inside

BEMAN_INSIDE_WARNINGS_POP

#endif // BEMAN_INSIDE_DETAIL_NUMBER_TEXT_HPP
