// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_DETAIL_NUMBERS_HPP
#define BEMAN_INSIDE_INSIDE_DETAIL_NUMBERS_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <concepts>
#include <cstdint>

//---------------------------------------------------------------------------
// numbers — primitive numeric utilities: umax/imax (public), and in detail
// the overflow-detecting add/sub/mul, the plain_number / fractional concepts
// and safe_abs. The floating-point bit helpers are in inside/detail/float_bits.hpp.
//---------------------------------------------------------------------------
namespace beman::inside {
using umax = std::uint64_t;
using imax = std::int64_t;

namespace detail {
// Overflow-detecting add/sub/mul for integers (derived from Peter Sommerlad's
// odins.h, MIT license): GCC's __builtin_*_overflow intrinsics. Return true on
// overflow; *result holds the wrapped value either way. Used by rational's
// kernels and every checked path. (C++26's ckd_add is not constexpr in
// libstdc++ 16.)
template <std::integral T>
[[nodiscard]] constexpr bool add_overflow(T l, T r, T* result) noexcept {
    return __builtin_add_overflow(l, r, result);
}

template <std::integral T>
[[nodiscard]] constexpr bool sub_overflow(T l, T r, T* result) noexcept {
    return __builtin_sub_overflow(l, r, result);
}

template <std::integral T>
[[nodiscard]] constexpr bool mul_overflow(T l, T r, T* result) noexcept {
    return __builtin_mul_overflow(l, r, result);
}

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

#endif // BEMAN_INSIDE_INSIDE_DETAIL_NUMBERS_HPP
