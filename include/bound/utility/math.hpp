//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDmathHPP
#define BNDmathHPP

#include <cstdint>
#include <cmath>
#include <utility>
#include <concepts>
#include <type_traits>

namespace bnd
{
  using umax = std::uint64_t;
  using imax = std::int64_t;

  struct rational;

  template <std::uintmax_t N>
  using smallest_uint_for = 
    std::conditional_t<(N == 0), rational,
    std::conditional_t<(N <= UINT8_MAX),  std::uint8_t,
    std::conditional_t<(N <= UINT16_MAX), std::uint16_t,
    std::conditional_t<(N <= UINT32_MAX), std::uint32_t,
                                          std::uint64_t>>>>;

  template <typename T>
  constexpr std::string_view uint_type_name() 
  {
    if constexpr (std::is_same_v<T, std::uint8_t>)  return "uint8_t";
    if constexpr (std::is_same_v<T, std::uint16_t>) return "uint16_t";
    if constexpr (std::is_same_v<T, std::uint32_t>) return "uint32_t";
    if constexpr (std::is_same_v<T, std::uint64_t>) return "uint64_t";
    if constexpr (std::is_same_v<T, rational>) return "rational";
    return "unknown";
  }

  template<typename T>
  concept arithmetic = std::integral<T> || std::floating_point<T> || std::same_as<rational,T>;

  template <std::signed_integral V>
  constexpr umax safe_abs(V value)
  { return (value >= 0) ? static_cast<umax>(value) : -static_cast<umax>(value); }

  // I barely understand this, but it seems to work fine
  constexpr auto abs_fraction(double value) 
  {
    if (not std::isfinite(value))
      throw "Keep your cr*ppy double to yourself!";

    // abs
    if (value < 0) value = -value;
    
    int exponent;
    // norm_frac in [0.5, 1.0)
    double norm_frac = std::frexp(value, &exponent);
    // double has 53 bits of precision
    constexpr int bits = 53;
    umax num = static_cast<umax>(std::ldexp(norm_frac, bits));
    umax den = 1ULL << (bits - exponent);
    // simplify by removing trailing zeros from num
    while (num && (num & 1) == 0 && den != 1) {
        num >>= 1;
        den >>= 1;
    }
    return std::pair{num, den};
  }

  template <std::unsigned_integral T>
  constexpr bool add_overflow(T lhs, T rhs)
  { return (lhs + rhs) < rhs; }

  template <std::unsigned_integral T>
  constexpr bool mul_overflow(T lhs, T rhs)
  { return lhs != 0 && rhs != 0 && (rhs > std::numeric_limits<T>::max() / lhs); }
} // namespace bnd

#endif // BNDcommonHPP
