//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDprintHPP
#define BNDprintHPP

#include "bound/bound.hpp"

#include <ostream>
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

  template <boundable B>
  inline std::string to_string(B b)
  { return bnd::to_string(static_cast<rational>(b)); }

  template <boundable B>
  inline std::string to_string_debug(B b)
  {
    std::string str;
    str += bnd::to_string(static_cast<rational>(b));
    str += " {";
    str += bnd::to_string(+b.Raw);
    str += "[" + std::string(uint_type_name<raw_t<B>>());
    str += " Max:" + bnd::to_string(+MaxNotch<B>) + "] ";
    str += bnd::to_string(Grid<B>);
    str += "}";
    return str;
  }

  inline std::ostream& operator<<(std::ostream& stream, rational r)
  {
    stream << bnd::to_string(r);
    return stream;
  }

  template <boundable B>
  inline std::ostream& operator<<(std::ostream& stream, B b)
  {
    stream << bnd::to_string(b);
    return stream;
  }

} // namespace bnd

#endif // BNDprintHPP
