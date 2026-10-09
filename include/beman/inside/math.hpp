// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_MATH_HPP
#define BEMAN_INSIDE_MATH_HPP

#include <beman/inside/detail/debug.hpp> // errc, detail::raise (replaces <stdexcept> throw)

#include <cstdint>
#include <utility>
#include <concepts>
#include <type_traits>
#include <bit>
#include <limits>

//---------------------------------------------------------------------------
// math — primitive numeric utilities: umax/imax, smallest_uint_for /
// smallest_int_for (grid storage selection), the arithmetic/fractional concepts,
// safe_abs, constexpr frexp/ldexp, and abs_fraction (the double → rational
// engine behind rational(double)).
//---------------------------------------------------------------------------
namespace beman::inside {
using umax = std::uint64_t;
using imax = std::int64_t;

namespace detail {
struct rational;
}

namespace detail {
// A plain number a grid-less value can be: integral, floating point, or the
// library's exact rational. Internal; the public operand concept is `numeric`.
template <typename T>
concept arithmetic = std::integral<T> || std::floating_point<T> || std::same_as<rational, T>;
} // namespace detail

namespace detail {

// Smallest unsigned type whose range holds every index 0..N.
template <std::uintmax_t N>
using smallest_uint_for_t = std::conditional_t<
    (N == 0),
    rational,
    std::conditional_t<(N <= UINT8_MAX),
                       std::uint8_t,
                       std::conditional_t<(N <= UINT16_MAX),
                                          std::uint16_t,
                                          std::conditional_t<(N <= UINT32_MAX), std::uint32_t, std::uint64_t>>>>;

// Smallest signed type whose range holds Low..High.
template <std::intmax_t Low, std::intmax_t High>
using smallest_int_for_t = std::conditional_t<
    (Low >= INT8_MIN && High <= INT8_MAX),
    std::int8_t,
    std::conditional_t<(Low >= INT16_MIN && High <= INT16_MAX),
                       std::int16_t,
                       std::conditional_t<(Low >= INT32_MIN && High <= INT32_MAX), std::int32_t, std::int64_t>>>;

// (type_name_v<T> — used only by the debug stringifier — lives in
// "beman/inside/io.hpp" so the core stays free of <string_view>.)

// Subset of arithmetic excluding integrals — the rhs types that need the
// rational-arithmetic assignment path.
template <typename T>
concept fractional = std::floating_point<T> || std::same_as<rational, T>;

template <std::signed_integral V>
[[nodiscard]] constexpr umax safe_abs(V value) noexcept {
    return (value >= 0) ? static_cast<umax>(value) : umax{0} - static_cast<umax>(value);
}

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

constexpr double ldexp(double value, int exp) noexcept {
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

// Freestanding finite check: `v - v` is 0 for every finite v, NaN otherwise
// (and inf - inf is NaN). Avoids <cmath>/std::isfinite so the core stays
// bare-metal clean; the same idiom is used in the store paths.
constexpr bool is_finite(double v) noexcept { return v - v == 0; }

// Exact conversion of a finite double to num/den (den a power of two), the
// engine behind rational(double). A finite double is exactly
// `significand · 2^exp2` (a 53-bit significand), both read straight from the
// IEEE-754 bits — no <cmath>, no FPU rounding, bit-identical across platforms.
constexpr std::pair<umax, umax> abs_fraction(double value) {
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
    // below ~2^-62, where the significand collapses to 0 (0/d canonicalises to 0/1).
    int           den_pow = -exp2;
    constexpr int max_pow = 62;
    if (den_pow > max_pow) {
        const int drop = den_pow - max_pow;
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

} // namespace detail
} // namespace beman::inside

#endif // BEMAN_INSIDE_MATH_HPP
