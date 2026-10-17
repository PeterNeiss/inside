// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_NUMBERS_HPP
#define BEMAN_INSIDE_NUMBERS_HPP

#include <beman/inside/detail/config.hpp>
BEMAN_INSIDE_WARNINGS_PUSH

#include <cstdint>
#include <utility>
#include <concepts>
#include <type_traits>
#include <bit>
#include <limits>

//---------------------------------------------------------------------------
// numbers — primitive numeric utilities: umax/imax, smallest_uint_for_t /
// smallest_int_for_t (grid storage selection), the plain_number/fractional
// concepts and safe_abs. The floating-point bit helpers are in
// detail/float_bits.hpp.
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
concept plain_number = std::integral<T> || std::floating_point<T> || std::same_as<rational, T>;
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

// (type_name_of<T> — used only by the debug stringifier — lives in
// "beman/inside/io.hpp" so the core stays free of <string_view>.)

// Subset of arithmetic excluding integrals — the rhs types that need the
// rational-arithmetic assignment path.
template <typename T>
concept fractional = std::floating_point<T> || std::same_as<rational, T>;

template <std::signed_integral V>
[[nodiscard]] constexpr umax safe_abs(V value) noexcept {
    return (value >= 0) ? static_cast<umax>(value) : umax{0} - static_cast<umax>(value);
}
} // namespace detail
} // namespace beman::inside

BEMAN_INSIDE_WARNINGS_POP

#endif // BEMAN_INSIDE_NUMBERS_HPP
