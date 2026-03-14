//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDrationalHPP
#define BNDrationalHPP

#include "bound/common.hpp"
#include "bound/utility/math.hpp"

#include <concepts>
#include <numeric>
#include <compare>
#include <cmath>

namespace bnd
{
  //---------------------------------------------------------------------------
  // rational 
  //---------------------------------------------------------------------------
  // We need the sign with unsigned types to represent e.g. umax itself
  // rational is intended for compile time and is not arithmetically safe at
  // runtime
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

    template <std::floating_point T>
    constexpr rational(T);

    constexpr rational operator-() const;
    constexpr bool operator==(const rational& rhs) const = default;

    private:
      constexpr void trim();
  };
  
  inline constexpr rational operator+(const rational&, const rational&); 

  //---------------------------------------------------------------------------
  // rational::trim
  //---------------------------------------------------------------------------
  inline constexpr void rational::trim()
  {
    auto gcd = std::gcd(Numerator, Denominator);
    Numerator   /= gcd;
    Denominator /= gcd;
  }
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

  // the difficult thing is to heuristically find an appropriate denominator
  // because its difficult to pin a floating point at a fixed location
  template <std::floating_point T>
  inline constexpr rational::rational(T value)
  {
    if (not std::isfinite(value))
      throw "Keep your cr*ppy double to yourself!";

    if (value == 0.0)
    {
      Numerator = 0;
      Denominator = 1;
      Sign = sign::zero;
      return;
    }

    if (value < 0.0)
    {
      value = -value;
      Sign = sign::negative;
    }
    else
      Sign = sign::positive;

    std::tie(Numerator, Denominator) = abs_fraction(value); 
    trim();
  }

  //---------------------------------------------------------------------------
  // operator- 
  //---------------------------------------------------------------------------
  inline constexpr rational rational::operator-() const
  { 
    if (Sign == sign::detect)
      throw "internal logic error";

    if (Sign == sign::zero)
      return *this;

    return 
    {
      Numerator, Denominator, 
      (Sign == sign::negative) ? sign::positive : sign::negative
    };
  }

  //---------------------------------------------------------------------------
  // operator<=> 
  //---------------------------------------------------------------------------
  // this is only overlow safe in constexpr context
  //---------------------------------------------------------------------------
  inline constexpr auto operator<=>(const rational& lhs, const rational& rhs) 
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

  //---------------------------------------------------------------------------
  // operator/ 
  //---------------------------------------------------------------------------
  // this is only overlow safe in constexpr context
  //---------------------------------------------------------------------------
  inline constexpr rational operator/(const rational& lhs, const rational& rhs) 
  { 
    if (rhs.Sign == sign::zero)
      throw "division by zero imminent";

    return 
    {
      lhs.Numerator * rhs.Denominator,
      rhs.Numerator * lhs.Denominator, 
      (lhs.Sign == rhs.Sign) ? sign::positive : sign::negative
    };
  }

  //---------------------------------------------------------------------------
  // operator+ 
  //---------------------------------------------------------------------------
  // this is only overlow safe in constexpr context
  //---------------------------------------------------------------------------
  inline constexpr rational operator+(const rational& lhs, const rational& rhs) 
  { 
    if (lhs == -rhs)
      return 0;

    if (lhs.Sign == sign::zero)
      return rhs;

    if (rhs.Sign == sign::zero)
      return lhs;

    auto A = lhs.Numerator * rhs.Denominator;
    auto B = rhs.Numerator * lhs.Denominator;
    auto denominator = lhs.Denominator * rhs.Denominator;

    if (lhs.Sign == rhs.Sign)
      return {A + B, denominator, lhs.Sign}; 

    // avoid underflow with opposing signs
    auto numerator = (A > B) ? (A - B) : (B - A);

    if (lhs.Sign == sign::negative)
      return {numerator, denominator, (A > B) ? sign::negative : sign::positive}; 
    else
      return {numerator, denominator, (A > B) ? sign::positive : sign::negative}; 
  }

  namespace literals
  {
    constexpr rational operator ""_r(unsigned long long int numerator)
    { return rational{numerator}; }

    constexpr rational operator ""_r(long double value)
    { return rational{static_cast<double>(value)}; }
  } // namespace literals

} // namespace bnd

#endif // BNDrationalHPP
