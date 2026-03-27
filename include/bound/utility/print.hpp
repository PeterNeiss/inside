//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDprintHPP
#define BNDprintHPP

#include "bound/bound.hpp"

#include <ostream>
#include <string>
#include <source_location>

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

  std::string to_string(diag_location loc) 
  {
#ifndef NDEBUG
    using namespace std::string_literals;
    return loc.file_name() + ":"s
         + std::to_string(loc.line()) + ":"s
         + std::to_string(loc.column()) + " ("s
         + loc.function_name() + "): "s;
#else
    (void) loc;
    return "";
#endif
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
