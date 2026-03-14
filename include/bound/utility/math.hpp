//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDmathHPP
#define BNDmathHPP

#include "bound/common.hpp"

#include <cstdint>
#include <bit>
#include <cmath>
#include <utility>

namespace bnd
{
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

  // workaround because std::ilogb is not constexpr everywhere yet
  constexpr int ilogb(double value) 
  {
    if (value < 0) value = -value;
    auto bits = std::bit_cast<uint64_t>(value);
    int biased = static_cast<int>((bits >> 52) & 0x7FF);
    uint64_t mantissa = bits & 0x000F'FFFF'FFFF'FFFF;

    if (biased == 0 && mantissa == 0) return 0; 
    if (biased == 0x7FF && mantissa == 0) 
      throw "Keep your cr*ppy INF double to yourself!";
    if (biased == 0x7FF)
      throw "Keep your cr*ppy NAN double to yourself!";
    if (biased == 0) {
        // subnormal: count leading zeros in mantissa
        int exp = -1023;
        while ((mantissa & (1ULL << 52)) == 0) {
            mantissa <<= 1;
            --exp;
        }
        return exp;
    }
    return biased - 1023;
  }

} // namespace bnd

#endif // BNDcommonHPP
