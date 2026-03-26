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
  std::string to_string(rational r)
  {
    if (r.Denominator == 1)
      return std::to_string(r.Numerator);

    std::string str;
    if (r.Sign == sign::negative)
      str =  "-";

    str += std::to_string(r.Numerator);
    str += "/"; 
    str += std::to_string(r.Denominator); 
    return str;
  }

  std::string to_string(interval ival)
  {
    std::string str{"["};

    str += bnd::to_string(ival.Lower);
    str += " .. "; 
    str += bnd::to_string(ival.Upper); 
    str += "]";
    return str;
  }

  inline std::ostream& operator<<(std::ostream& stream, rational r)
  {
    if (r.Sign == sign::negative)
      stream << "-";

    stream << r.Numerator << "/" << r.Denominator; 
    return stream;
  }

  inline std::ostream& operator<<(std::ostream& stream, boundable auto b)
  {
    //TODO auto [num,den] = b.to_rational();
    stream << b.to_rational()
           <<" {"   
           << +b.Raw << "[" << uint_type_name<typename decltype(b)::raw_type>() << "] "
           << b.Interval.Lower << "[Low] "
           << b.Interval.Upper << "[Up] "
           << b.Grid.Notch << "[Notch] "
           << +b.Grid.max_notch() << "[MaxRaw]}";
    return stream;
  }

} // namespace bnd

#endif // BNDprintHPP
