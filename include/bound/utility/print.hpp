//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDprintHPP
#define BNDprintHPP

#include <ostream>
#include "bound/bound.hpp"

namespace bnd
{
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
