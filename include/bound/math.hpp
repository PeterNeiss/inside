//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDmathHPP
#define BNDmathHPP

#include <cstdint>
#include <cmath>
#include <stdexcept>
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
namespace bnd
{
  using umax = std::uint64_t;
  using imax = std::int64_t;

  namespace detail { struct rational; }

  template<typename T>
  concept arithmetic = std::integral<T> || std::floating_point<T> || std::same_as<bnd::detail::rational,T>;

  namespace detail
  {

  // Strict `<` reserves the type's max as the sentinel slot for
  // slim::optional<bound>, so a grid whose max_notch lands on a type's max
  // promotes to the next-wider type (e.g. bound<{0,255}> uses uint16_t, not
  // uint8_t). A valid grid value can thus never collide with the sentinel.
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

  // Subset of arithmetic excluding integrals — the rhs types that need the
  // rational-arithmetic assignment path. (Named to avoid clashing with `real`.)
  template<typename T>
  concept fractional = std::floating_point<T> || std::same_as<rational, T>;

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

  // Exact conversion of a finite double to num/den (den a power of two), the
  // engine behind rational(double). A finite double is exactly
  // `significand · 2^exp2` (a 53-bit significand), both read straight from the
  // IEEE-754 bits — no <cmath>, no FPU rounding, bit-identical across platforms.
  constexpr std::pair<umax, umax> abs_fraction(double value)
  {
    if (not std::isfinite(value))
      throw std::domain_error("bnd::detail::abs_fraction: non-finite double");

    if (value == 0.0) return {0, 1};
    if (value < 0)    value = -value;        // |value|; sign is the caller's job

    const auto bits = std::bit_cast<std::uint64_t>(value);
    const int  e    = static_cast<int>((bits >> 52) & 0x7FF);
    umax significand = bits & 0x000F'FFFF'FFFF'FFFF;
    int  exp2;
    if (e == 0)                              // subnormal: no implicit leading 1
      exp2 = 1 - 1023 - 52;
    else                                     // normal: restore the implicit bit
    {
      significand |= (umax{1} << 52);
      exp2 = e - 1023 - 52;
    }

    // value == significand * 2^exp2. Re-express as num/den with den = 2^k.
    if (exp2 >= 0)                           // integer-valued: scale up, den = 1
      return {significand << exp2, 1};

    // exp2 < 0 → den = 2^(-exp2), capped at 2^62 (den is stored signed). Beyond
    // the cap the value is too small to keep: drop the significand's low bits
    // (shift ≥ 64 folds to zero). Reachable for every subnormal and normals
    // below ~2^-62, where the significand collapses to 0 (0/d canonicalises to 0/1).
    int den_pow = -exp2;
    constexpr int max_pow = 62;
    if (den_pow > max_pow)
    {
      const int drop = den_pow - max_pow;
      significand = (drop >= 64) ? 0 : (significand >> drop);
      // Return canonical {0,1}: callers take this verbatim, so a non-canonical
      // {0, 2^62} would break the structural rational::operator==.
      if (significand == 0) return {0, 1};
      den_pow = max_pow;
    }
    umax den = umax{1} << den_pow;

    // den is a power of two, so reduce by cancelling shared factors of two.
    while (significand && (significand & 1) == 0 && den != 1)
    {
      significand >>= 1;
      den >>= 1;
    }
    return {significand, den};
  }

  } // namespace detail
} // namespace bnd

#endif // BNDmathHPP
