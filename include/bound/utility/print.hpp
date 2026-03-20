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
    stream << r.Numerator << "/" << r.Denominator; 
    return stream;
  }

  inline std::ostream& operator<<(std::ostream& stream, boundable auto b)
  {
    //TODO auto [num,den] = b.to_rational();
    stream << "Value: "  << b.to_rational()
           << " Lower: " << static_cast<double>(b.Interval.Lower)
           << " Upper: " << static_cast<double>(b.Interval.Upper)
           << " Notch: " << static_cast<double>(b.Grid.Notch)
           << " max_notch: " << static_cast<double>(b.Grid.max_notch())
           << " Type: " << uint_type_name<typename decltype(b)::raw_type>() 
           << " Raw: "   << b.Raw;
    return stream;
  }
} // namespace bnd

#endif // BNDprintHPP
