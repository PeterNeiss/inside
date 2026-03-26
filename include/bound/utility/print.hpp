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
    std::string str;
    if (r.Sign == sign::negative)
      str =  "-";

    if (r.Denominator == 1)
      return str += std::to_string(r.Numerator);

    str += std::to_string(r.Numerator);
    str += "/"; 
    str += std::to_string(r.Denominator); 
    return str;
  }

  std::string to_string(interval ival)
  {
    std::string str{"["};

    str += bnd::to_string(ival.Lower);
    str += ".."; 
    str += bnd::to_string(ival.Upper); 
    str += "]";
    return str;
  }

  std::string to_string(grid g)
  {
    std::string str{"{"};

    str += bnd::to_string(g.Interval);
    str += ", "; 
    str += bnd::to_string(g.Notch); 
    str += "}";
    return str;
  }

  inline std::ostream& operator<<(std::ostream& stream, rational r)
  {
    stream << bnd::to_string(r); 
    return stream;
  }

  inline std::ostream& operator<<(std::ostream& stream, boundable auto b)
  {
    //TODO auto [num,den] = b.to_rational();
    stream << b.to_rational()
           <<" {"   
           << +b.Raw << "[" << uint_type_name<typename decltype(b)::raw_type>()
           << " Max:" << +b.Grid.max_notch() << "] "
           << bnd::to_string(b.Grid)
           << "}";
    return stream;
  }

} // namespace bnd

#endif // BNDprintHPP
