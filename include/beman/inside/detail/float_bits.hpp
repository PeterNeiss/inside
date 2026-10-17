// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_FLOAT_BITS_HPP
#define BEMAN_INSIDE_DETAIL_FLOAT_BITS_HPP

#include <beman/inside/detail/config.hpp>
BEMAN_INSIDE_WARNINGS_PUSH

#include <beman/inside/numbers.hpp>      // umax, imax
#include <beman/inside/detail/debug.hpp> // errc, detail::raise

#include <bit>
#include <concepts>
#include <cstdint>
#include <utility>

//---------------------------------------------------------------------------
// float_bits — floating-point values read and built from their IEEE-754
// bits, with no <cmath>: constexpr frexp/ldexp, finiteness, and abs_fraction
// (the double → rational engine behind rational(double)).
//---------------------------------------------------------------------------
namespace beman::inside::detail {
inline constexpr double frexp(double value, int* exp) noexcept {
    if (value == 0.0) {
        *exp = 0;
        return value;
    }

    auto                    bits          = std::bit_cast<std::uint64_t>(value);
    constexpr std::uint64_t mantissa_mask = 0x000F'FFFF'FFFF'FFFF;
    constexpr std::uint64_t sign_mask     = 0x8000'0000'0000'0000;
    auto                    e             = static_cast<int>((bits >> 52) & 0x7FF);

    if (e == 0x7FF) {
        *exp = 0;
        return value;
    }

    if (e == 0) {
        // subnormal: scale up, recurse
        double scaled = value * 0x1p53;
        double result = frexp(scaled, exp);
        *exp -= 53;
        return result;
    }

    *exp = e - 0x3FE;
    bits = (bits & (sign_mask | mantissa_mask)) | (std::uint64_t{0x3FE} << 52);
    return std::bit_cast<double>(bits);
}

// value·2^exp for |exp| ≤ 2^20 (the kernels' scalings): e + exp below
// cannot overflow int. ldexp below takes any exp.
constexpr double ldexp_small(double value, int exp) noexcept {
    if (value == 0.0 || exp == 0)
        return value;

    auto                    bits          = std::bit_cast<std::uint64_t>(value);
    constexpr std::uint64_t sign_mask     = 0x8000'0000'0000'0000;
    constexpr std::uint64_t mantissa_mask = 0x000F'FFFF'FFFF'FFFF;
    auto                    e             = static_cast<int>((bits >> 52) & 0x7FF);

    if (e == 0x7FF)
        return value; // inf or NaN

    // Normalize subnormals
    int extra = 0;
    if (e == 0) {
        bits  = std::bit_cast<std::uint64_t>(value * 0x1p53);
        e     = static_cast<int>((bits >> 52) & 0x7FF);
        extra = -53;
    }

    int new_exp = e + exp + extra;

    if (new_exp >= 0x7FF) {
        // overflow → ±inf
        return (bits & sign_mask) ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
    }

    if (new_exp > 0) {
        // normal result
        bits = (bits & (sign_mask | mantissa_mask)) | (static_cast<std::uint64_t>(new_exp) << 52);
        return std::bit_cast<double>(bits);
    }

    // Subnormal or underflow
    auto mantissa = (bits & mantissa_mask) | (std::uint64_t{1} << 52);
    int  shift    = 1 - new_exp;

    if (shift > 53)
        return std::bit_cast<double>(bits & sign_mask); // ±0

    // Round-to-nearest-even
    std::uint64_t dropped = mantissa & ((std::uint64_t{1} << shift) - 1);
    mantissa >>= shift;
    std::uint64_t halfway = std::uint64_t{1} << (shift - 1);
    if (dropped > halfway || (dropped == halfway && (mantissa & 1)))
        ++mantissa;

    bits = (bits & sign_mask) | mantissa;
    return std::bit_cast<double>(bits);
}

// value·2^exp for any exp: past ±2200 every finite double overflows or
// underflows alike, so exp is clamped there (an exp near INT_MAX would
// overflow ldexp_small's exponent sum).
constexpr double ldexp(double value, int exp) noexcept {
    return ldexp_small(value, exp > 2200 ? 2200 : exp < -2200 ? -2200 : exp);
}

// Freestanding finite check: `v - v` is 0 for every finite v, NaN otherwise
// (and inf - inf is NaN). Avoids <cmath>/std::isfinite so the core stays
// bare-metal clean; the same idiom is used in the store paths.
template <std::floating_point T>
constexpr bool is_finite(T v) noexcept {
    return v - v == 0;
}

// |v| ≥ 2^64 or NaN: past every 64-bit integer and rational (a grid within
// them is decided by the sign alone).
template <std::floating_point T>
constexpr bool past_64_bits(T v) noexcept {
    return !(v < static_cast<T>(0x1p64) && v > static_cast<T>(-0x1p64));
}

// Whether abs_fraction keeps a finite double exactly below 2^64: its reduced
// denominator is at most 2^62 (the bits past that are dropped). Every |value|
// ≥ 2^-10 qualifies; smaller ones need enough trailing zero bits.
constexpr bool abs_fraction_exact(double value) noexcept {
    const auto bits = std::bit_cast<std::uint64_t>(value) & 0x7FFF'FFFF'FFFF'FFFF;
    const int  e    = static_cast<int>(bits >> 52);
    if (e >= 1023 - 10 || bits == 0)
        return true;
    const umax significand = (bits & 0x000F'FFFF'FFFF'FFFF) | (e != 0 ? umax{1} << 52 : 0);
    const int  exp2        = e != 0 ? e - 1023 - 52 : 1 - 1023 - 52;
    return exp2 + std::countr_zero(significand) >= -62;
}

// Exact conversion of a finite double to num/den (den a power of two), the
// engine behind rational(double). A finite double is exactly
// `significand · 2^exp2` (a 53-bit significand), both read straight from the
// IEEE-754 bits — no <cmath>, no FPU rounding, bit-identical across platforms.
constexpr std::pair<umax, umax> abs_fraction(double value, bool& exact) {
    if (not is_finite(value))
        raise(errc::not_finite, "beman::inside::detail::abs_fraction: non-finite double");

    if (value == 0.0)
        return {0, 1};
    if (value < 0)
        value = -value; // |value|; sign is the caller's job

    const auto bits        = std::bit_cast<std::uint64_t>(value);
    const int  e           = static_cast<int>((bits >> 52) & 0x7FF);
    umax       significand = bits & 0x000F'FFFF'FFFF'FFFF;
    int        exp2;
    if (e == 0) // subnormal: no implicit leading 1
        exp2 = 1 - 1023 - 52;
    else // normal: restore the implicit bit
    {
        significand |= (umax{1} << 52);
        exp2 = e - 1023 - 52;
    }

    // value == significand * 2^exp2. Re-express as num/den with den = 2^k.
    if (exp2 >= 0) // integer-valued: scale up, den = 1
    {
        // |value| ≥ 2^64 has no 64-bit numerator: fail rather than wrap the shift.
        if (exp2 > 64 - std::bit_width(significand)) {
            if consteval {
                // Clang prints only the first ~34 characters: lead with the cause.
                constexpr_error<"no 64-bit rational for a double of magnitude 2^64 or more (a grid limit needs C++26 "
                                "big grids)">();
            }
            raise(errc::overflow, "beman::inside::detail::abs_fraction: |double| >= 2^64");
        }
        return {significand << exp2, 1};
    }

    // exp2 < 0 → den = 2^(-exp2), capped at 2^62 (den is stored signed). Beyond
    // the cap the value is too small to keep: drop the significand's low bits
    // (shift ≥ 64 folds to zero). Reachable for every subnormal and normals
    // below ~2^-62, where the significand collapses to 0 (0/d canonicalizes to 0/1).
    int           den_pow = -exp2;
    constexpr int max_pow = 62;
    if (den_pow > max_pow) {
        const int drop = den_pow - max_pow;
        exact          = drop <= std::countr_zero(significand); // only zero bits dropped
        significand    = (drop >= 64) ? 0 : (significand >> drop);
        // Return canonical {0,1}: callers take this verbatim, so a non-canonical
        // {0, 2^62} would break the structural rational::operator==.
        if (significand == 0)
            return {0, 1};
        den_pow = max_pow;
    }
    umax den = umax{1} << den_pow;

    // den is a power of two, so the whole reduction is one shift by the
    // shared factor count (bounded by den's exponent). Plain ternary — no
    // <algorithm> in this core header (libc++ does not provide std::min
    // transitively).
    if (significand != 0) {
        const int trailing = std::countr_zero(significand);
        const int shift    = trailing < den_pow ? trailing : den_pow;
        significand >>= shift;
        den >>= shift;
    }
    return {significand, den};
}
constexpr std::pair<umax, umax> abs_fraction(double value) {
    bool exact = true;
    return abs_fraction(value, exact);
}

// A floating point wider than double (long double) whose value is a double.
template <std::floating_point T>
[[nodiscard]] constexpr bool is_double_value(T v) noexcept {
    return static_cast<T>(static_cast<double>(v)) == v; // NaN: false
}

// A floating point wider than double: long double on x87 and IEEE quad, whose
// bits past a double's every exact path keeps.
template <typename T>
concept wide_float = std::floating_point<T> && (sizeof(T) > sizeof(double));

// Whether a finite double d lies halfway between two adjacent values of a
// narrower T (float): there static_cast<T>(d) breaks a tie that d's source
// value may not have, so a value rounded to d first must round again from its
// exact value. f and the other neighbour f + 2(d − f) are exact in double.
template <std::floating_point T>
[[nodiscard]] constexpr bool narrow_midpoint(double d) noexcept {
    const T f = static_cast<T>(d);
    if (!is_finite(f))
        return true; // past T's largest finite: decide exactly
    const double g = static_cast<double>(f) + 2 * (d - static_cast<double>(f));
    return g != static_cast<double>(f) && static_cast<double>(static_cast<T>(g)) == g;
}

// A finite nonzero double as |d| = Mant·2^Exp, Mant of 53 bits.
struct double_parts_t {
    umax Mant;
    int  Exp;
};
[[nodiscard]] constexpr double_parts_t double_parts(double d) noexcept {
    int        e    = 0;
    const umax mant = static_cast<umax>(ldexp(frexp(d < 0 ? -d : d, &e), 53));
    return {mant, e - 53};
}

// The doubles a floating point splits into, each the rounding error of the
// last: f(part) for each, until a part is 0, f returns false, or 8 parts.
// Returns what is left (0 when every part was taken).
template <std::floating_point T, typename F>
constexpr T for_each_double_part(T v, F f) {
    T rem = v;
    for (int i = 0; i < 8 && rem != 0; ++i) {
        const double d = static_cast<double>(rem);
        if (d == 0 || !f(d))
            break;
        rem -= static_cast<T>(d);
    }
    return rem;
}

} // namespace beman::inside::detail

BEMAN_INSIDE_WARNINGS_POP

#endif // BEMAN_INSIDE_DETAIL_FLOAT_BITS_HPP
