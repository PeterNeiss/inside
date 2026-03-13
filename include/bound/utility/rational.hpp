//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDrationalHPP
#define BNDrationalHPP

#include "bound/common.hpp"

namespace bnd
{
  enum class sign {negative, zero, positive};
  struct rational 
  {
    umax Numerator;
    umax Denominator;
    sign Sign;

    constexpr rational(umax num = 0, umax den = 1, sign s = sign::positive)
     :Numerator{num}, Denominator{den}, 
      Sign{Numerator == 0 ? sign::zero : s}
    {
      if (Denominator == 0)
        throw "Denominator of Zero is invalid";
      if (Numerator != 0 && Sign == sign::zero)
        throw "Numerator is not Zero but sign is";
    }
  };
} // namespace bnd

#endif // BNDrationalHPP
