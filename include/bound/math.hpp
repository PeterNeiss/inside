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
#include <bit>
#include <limits>

namespace bnd
{
  using umax = std::uint64_t;
  using imax = std::int64_t;

  struct rational;

  // Strict `<` reserves the type's max as the sentinel slot used by
  // slim::optional<bound>. Mirrors smallest_int_for's `+1` margin on the
  // signed side. A grid whose max_notch lands exactly on a type's max
  // therefore promotes to the next-wider type.
  template <std::uintmax_t N>
  using smallest_uint_for =
    std::conditional_t<(N == 0), rational,
    std::conditional_t<(N < UINT8_MAX),  std::uint8_t,
    std::conditional_t<(N < UINT16_MAX), std::uint16_t,
    std::conditional_t<(N < UINT32_MAX), std::uint32_t,
                                          std::uint64_t>>>>;

  // +1 on min accounts for sentinel value reserved by slim::optional
  template <std::intmax_t Low, std::intmax_t High>
  using smallest_int_for =
    std::conditional_t<(Low >= INT8_MIN+1 && High <= INT8_MAX),   std::int8_t,
    std::conditional_t<(Low >= INT16_MIN+1 && High <= INT16_MAX), std::int16_t,
    std::conditional_t<(Low >= INT32_MIN+1 && High <= INT32_MAX), std::int32_t,
                                                                   std::int64_t>>>;

  template <typename T>
  constexpr std::string_view type_name()
  {
    if constexpr (std::is_same_v<T, std::uint8_t>)  return "uint8_t";
    if constexpr (std::is_same_v<T, std::uint16_t>) return "uint16_t";
    if constexpr (std::is_same_v<T, std::uint32_t>) return "uint32_t";
    if constexpr (std::is_same_v<T, std::uint64_t>) return "uint64_t";
    if constexpr (std::is_same_v<T, std::int8_t>)   return "int8_t";
    if constexpr (std::is_same_v<T, std::int16_t>)  return "int16_t";
    if constexpr (std::is_same_v<T, std::int32_t>)  return "int32_t";
    if constexpr (std::is_same_v<T, std::int64_t>)  return "int64_t";
    if constexpr (std::is_same_v<T, rational>) return "rational";
    return "unknown";
  }

  template<typename T>
  concept arithmetic = std::integral<T> || std::floating_point<T> || std::same_as<rational,T>;

  template <std::signed_integral V>
  constexpr umax safe_abs(V value)
  { return (value >= 0) ? static_cast<umax>(value) : -static_cast<umax>(value); }

  inline constexpr double frexp(double value, int* exp) noexcept
  {
    if (value == 0.0)
    {
        *exp = 0;
        return value;
    }

    auto bits = std::bit_cast<std::uint64_t>(value);
    constexpr std::uint64_t mantissa_mask = 0x000F'FFFF'FFFF'FFFF;
    constexpr std::uint64_t sign_mask     = 0x8000'0000'0000'0000;
    auto e = static_cast<int>((bits >> 52) & 0x7FF);

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

      auto bits = std::bit_cast<std::uint64_t>(value);
      constexpr std::uint64_t sign_mask     = 0x8000'0000'0000'0000;
      constexpr std::uint64_t mantissa_mask = 0x000F'FFFF'FFFF'FFFF;
      auto e = static_cast<int>((bits >> 52) & 0x7FF);

      if (e == 0x7FF)
          return value; // inf or NaN

      // Normalize subnormals
      int extra = 0;
      if (e == 0) {
          bits = std::bit_cast<std::uint64_t>(value * 0x1p53);
          e = static_cast<int>((bits >> 52) & 0x7FF);
          extra = -53;
      }

      int new_exp = e + exp + extra;

      if (new_exp >= 0x7FF) {
          // overflow → ±inf
          return (bits & sign_mask) ? -std::numeric_limits<double>::infinity()
                                   : std::numeric_limits<double>::infinity();
      }

      if (new_exp > 0) {
          // normal result
          bits = (bits & (sign_mask | mantissa_mask))
               | (static_cast<std::uint64_t>(new_exp) << 52);
          return std::bit_cast<double>(bits);
      }

      // Subnormal or underflow
      auto mantissa = (bits & mantissa_mask) | (std::uint64_t{1} << 52);
      int shift = 1 - new_exp;

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

  // I barely understand this, but it seems to work fine
  constexpr auto abs_fraction(double value)
  {
    if (not std::isfinite(value))
      throw "non-finite double";

    // abs
    if (value < 0) value = -value;

    int exponent;
    // norm_frac in [0.5, 1.0)
    double norm_frac = bnd::frexp(value, &exponent);
    // double has 53 bits of precision
    constexpr int bits = 53;
    umax num = static_cast<umax>(bnd::ldexp(norm_frac, bits));
    umax den;
    if (exponent >= bits)
    {
      num <<= (exponent - bits);
      den = 1;
    }
    else
    {
      // den = 2^(bits - exponent); the rational denominator is later stored as
      // a signed imax, so the shift must not exceed 62 (1ULL << 63 sets the
      // sign bit and casting to imax is implementation-defined). For values
      // too small to represent precisely, scale num down so den fits. Right-
      // shifting a umax by >= 64 is UB, so fold that case to a hard zero.
      int shift = bits - exponent;
      constexpr int max_shift = 62;
      if (shift > max_shift)
      {
        int rshift = shift - max_shift;
        num = (rshift >= 64) ? 0 : (num >> rshift);
        shift = max_shift;
      }
      den = 1ULL << shift;
      // simplify by removing trailing zeros from num
      while (num && (num & 1) == 0 && den != 1) {
          num >>= 1;
          den >>= 1;
      }
    }
    return std::pair{num, den};
  }

} // namespace bnd

#endif // BNDmathHPP
