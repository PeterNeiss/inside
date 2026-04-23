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

    str += std::to_string(r.Numerator);
    str += "/";
    str += std::to_string(ad);
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
