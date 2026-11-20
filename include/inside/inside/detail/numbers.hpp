// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INSIDE_INSIDE_DETAIL_NUMBERS_HPP
#define INSIDE_INSIDE_DETAIL_NUMBERS_HPP

#include <inside/inside/detail/config.hpp>

#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>

// Primitive numeric utilities: the overflow-detecting add, sub and mul, the plain_number and fractional concepts,
// and safe_abs.
// The 64-bit fast paths name std::int64_t and std::uint64_t themselves;
// the floating-point bit helpers are in inside/detail/float_bits.hpp.
namespace ins
{
  namespace detail
  {
    // Overflow-detecting add, sub and mul for integers (after Peter Sommerlad's odins.h, MIT license),
    // on GCC's __builtin_*_overflow: true on overflow, *result holds the wrapped value either way.
    // Used by rational's kernels and every checked path. (C++26's ckd_add is not constexpr in libstdc++ 16.)
    template <std::integral T>
    [[nodiscard]] constexpr bool add_overflow(T l, T r, T* result) noexcept
    {
      return __builtin_add_overflow(l, r, result);
    }

    template <std::integral T>
    [[nodiscard]] constexpr bool sub_overflow(T l, T r, T* result) noexcept
    {
      return __builtin_sub_overflow(l, r, result);
    }

    template <std::integral T>
    [[nodiscard]] constexpr bool mul_overflow(T l, T r, T* result) noexcept
    {
      return __builtin_mul_overflow(l, r, result);
    }

    struct rational;

    // A plain number, which a value without a grid can be: integral, floating point, or the library's exact
    // rational. Internal; the public operand concept is numeric.
    template <typename T>
    concept plain_number = std::integral<T> || std::floating_point<T> || std::same_as<rational, T>;

    // The arithmetic types other than integers: the sources that need the rational assignment path.
    template <typename T>
    concept fractional = std::floating_point<T> || std::same_as<rational, T>;

    // A builtin integer wider than 64 bits (__int128 in the GNU dialects):
    // it takes the exact paths, never int64 or the 64-bit rational.
    template <typename T>
    concept wide_builtin = std::integral<T> && (std::numeric_limits<T>::digits > 64);

    // Whether a builtin integer's value fits the 64-bit rational's numerator.
    template <std::integral T>
    [[nodiscard]] constexpr bool fits_uint64_magnitude(T v) noexcept
    {
      if constexpr (wide_builtin<T>)
        return std::is_signed_v<T> && v < 0 ? !(v < -static_cast<T>(std::numeric_limits<std::uint64_t>::max()))
                                            : !(static_cast<T>(std::numeric_limits<std::uint64_t>::max()) < v);
      else
        return true;
    }

    // |v| as a uint64, exact for the most negative value too (it does not fit V), for an integer of at most 64 bits.
    template <std::integral V>
      requires(std::numeric_limits<V>::digits <= 64)
    [[nodiscard]] constexpr std::uint64_t safe_abs(V value) noexcept
    {
      return std::is_signed_v<V> && value < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(value)
                                              : static_cast<std::uint64_t>(value);
    }
    // |v| in V's unsigned twin for a wider builtin (__int128).
    template <std::integral V>
      requires(std::numeric_limits<V>::digits > 64)
    [[nodiscard]] constexpr std::make_unsigned_t<V> safe_abs(V value) noexcept
    {
      using U = std::make_unsigned_t<V>;
      return std::is_signed_v<V> && value < 0 ? U{0} - static_cast<U>(value) : static_cast<U>(value);
    }
    // The int64 of a sign and a magnitude (at most 2^63, and 2^63 only when negative): taken modulo 2^64,
    // so -2^63 needs no special case.
    [[nodiscard]] constexpr std::int64_t with_sign(bool negative, std::uint64_t magnitude) noexcept
    {
      return static_cast<std::int64_t>(negative ? std::uint64_t{0} - magnitude : magnitude);
    }
  } // namespace detail
} // namespace ins

#endif // INSIDE_INSIDE_DETAIL_NUMBERS_HPP
