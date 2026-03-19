//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDprintHPP
#define BNDprintHPP

#include <ostream>
#include "bound/bound.hpp"

namespace bnd
{
  inline std::ostream& operator<<(std::ostream& stream, boundable auto b)
  {
    //TODO auto [num,den] = b.to_rational();
    stream << "Value: "  <<static_cast<double>(b)
           << " Lower: " << static_cast<double>(b.Interval.Lower)
           << " Upper: " << static_cast<double>(b.Interval.Upper)
           << " Notch: " << static_cast<double>(b.Grid.Notch)
           << " Raw: "   << static_cast<double>(b.Raw);
    return stream;
  }
} // namespace bnd

#endif // BNDprintHPP
