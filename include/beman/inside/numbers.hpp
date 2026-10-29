// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_NUMBERS_HPP
#define BEMAN_INSIDE_NUMBERS_HPP

#include <beman/inside/detail/config.hpp>

#include <concepts>
#include <cstdint>

//---------------------------------------------------------------------------
// numbers — primitive numeric utilities: umax/imax, the plain_number /
// fractional concepts and safe_abs. The floating-point bit helpers are in
// detail/float_bits.hpp.
//---------------------------------------------------------------------------
namespace beman::inside {
using umax = std::uint64_t;
using imax = std::int64_t;

namespace detail {
struct rational;

// A plain number a grid-less value can be: integral, floating point, or the
// library's exact rational. Internal; the public operand concept is `numeric`.
template <typename T>
concept plain_number = std::integral<T> || std::floating_point<T> || std::same_as<rational, T>;

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

#endif // BEMAN_INSIDE_NUMBERS_HPP
