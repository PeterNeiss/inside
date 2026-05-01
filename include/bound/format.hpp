//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDformatHPP
#define BNDformatHPP

#include "bound/grid.hpp"

#include <string>

namespace bnd
{
  inline std::string to_string(rational r)
  {
    std::string str;
    if (r.Denominator < 0)
      str = "-";

    umax ad = abs_den(r.Denominator);
    if (ad == 1)
      return str += std::to_string(r.Numerator);

    // power-of-2 or power-of-10: decimal output
    // find smallest 10^k divisible by ad
    umax pow10 = 1;
    unsigned digits = 0;
    bool is_decimal = false;
    for (unsigned k = 0; k < 20; ++k)
    {
      if (pow10 % ad == 0)
      { is_decimal = true; digits = k; break; }
      pow10 *= 10;
    }

    if (is_decimal)
    {
      umax scale = pow10 / ad;
      umax total;
      if (!mul_overflow(r.Numerator, scale, &total))
      {
        umax int_part = total / pow10;
        umax frac_part = total % pow10;
        str += std::to_string(int_part);
        if (digits > 0)
        {
          str += ".";
          auto frac_str = std::to_string(frac_part);
          // zero-pad
          for (unsigned i = 0; i < digits - frac_str.size(); ++i)
            str += "0";
          str += frac_str;
        }
        return str;
      }
      // overflow: fall through to mixed-number/fraction form below
    }

    // mixed number for improper fractions
    umax int_part = r.Numerator / ad;
    umax remainder = r.Numerator % ad;
    if (int_part > 0)
    {
      str += std::to_string(int_part);
      if (remainder > 0)
      {
        str += " ";
        str += std::to_string(remainder);
        str += "/";
        str += std::to_string(ad);
      }
    }
    else
    {
      str += std::to_string(r.Numerator);
      str += "/";
      str += std::to_string(ad);
    }
    return str;
  }

  inline std::string to_string(interval ival)
  {
    std::string str{"["};

    str += bnd::to_string(ival.Lower);
    str += "..";
    str += bnd::to_string(ival.Upper);
    str += "]";
    return str;
  }

  inline std::string to_string(grid g)
  {
    std::string str{"{"};

    str += bnd::to_string(g.Interval);
    str += ", ";
    str += bnd::to_string(g.Notch);
    str += "}";
    return str;
  }

  // delegate to std::to_string
  template <typename V>
  auto to_string(V value)
  { return std::to_string(value); }

} // namespace bnd

#endif // BNDformatHPP
