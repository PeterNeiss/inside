//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDrationalHPP
#define BNDrationalHPP

#include "bound/common.hpp"

#include <concepts>
#include <numeric>
#include <compare>

namespace bnd
{
  //---------------------------------------------------------------------------
  // rational 
  //---------------------------------------------------------------------------
  // We need the sign with unsigned types to represent e.g. umax itself
  //---------------------------------------------------------------------------
  enum class sign {negative, zero, positive, detect};
  struct rational 
  {
    umax Numerator;
    umax Denominator;
    sign Sign;

    constexpr rational():Numerator{0}, Denominator{1}, Sign{sign::zero} { }

    template <std::unsigned_integral T>
    constexpr rational(T num, umax den = 1ull, sign s = sign::detect);
    template <std::signed_integral T>
    constexpr rational(T num, umax den = 1ull);

    private:
      constexpr void trim()
      {
        auto gcd = std::gcd(Numerator, Denominator);
        Numerator   /= gcd;
        Denominator /= gcd;
      }
  };

  //---------------------------------------------------------------------------
  // rational::rational 
  //---------------------------------------------------------------------------
  template <std::unsigned_integral T>
  inline constexpr rational::rational(T num, umax den, sign s)
   :Numerator{num}, Denominator{den}, 
    Sign{Numerator == 0 ? sign::zero : (s == sign::detect) ? sign::positive: s}
  {
    if (Denominator == 0)
      throw "Denominator of Zero is invalid";
    if (Numerator != 0 && Sign == sign::zero)
      throw "Numerator is not Zero but sign is";

    trim();
  }

  template <std::signed_integral T>
  inline constexpr rational::rational(T num, umax den)
   :Numerator{static_cast<umax>(num<0 ? -num : num)}, Denominator{den}, 
    Sign{num == 0 ? sign::zero : (num > 0) ? sign::positive: sign::negative}
  {
    if (Denominator == 0)
      throw "Denominator of Zero is invalid";

    trim();
  }

  //---------------------------------------------------------------------------
  // operator<=> 
  //---------------------------------------------------------------------------
  // this is only overlow safe in constexpr context
  //---------------------------------------------------------------------------
  constexpr auto operator<=>(const rational& lhs, const rational& rhs) 
  { 
    if (lhs.Sign == sign::detect || rhs.Sign == sign::detect)
      throw "internal logic error";

    if (lhs.Sign == sign::negative && rhs.Sign == sign::negative) 
      return rhs.Numerator * lhs.Denominator <=> lhs.Numerator * rhs.Denominator; 
    else
    {
      if (lhs.Sign == sign::negative)
        return std::strong_ordering::less;

      if (rhs.Sign == sign::negative)
        return std::strong_ordering::greater;
    }

    return lhs.Numerator * rhs.Denominator <=> rhs.Numerator * lhs.Denominator; 
  }

} // namespace bnd

#endif // BNDrationalHPP
