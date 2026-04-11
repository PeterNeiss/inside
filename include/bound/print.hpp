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

  inline std::ostream& operator<<(std::ostream& stream, rational r)
  {
    stream << bnd::to_string(r);
    return stream;
  }

  template <boundable B>
  inline std::ostream& operator<<(std::ostream& stream, B b)
  {
    //TODO auto [num,den] = b.to_rational();
    stream << static_cast<rational>(b)
           <<" {"
           << +b.Raw.value() << "[" << uint_type_name<typename raw_t<B>::value_type>()
           << " Max:" << +MaxNotch<B>.value() << "] "
           << bnd::to_string(Grid<B>)
           << "}";
    return stream;
  }

} // namespace bnd

#endif // BNDprintHPP
