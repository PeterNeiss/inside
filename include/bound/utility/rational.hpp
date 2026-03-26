//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDrationalHPP
#define BNDrationalHPP

#include "bound/common.hpp"
#include "bound/utility/math.hpp"
#include "bound/detail/fixed_string.hpp"

#include <numeric>
#include <compare>
#include <cmath>
#include <tuple>

namespace bnd
{
  //---------------------------------------------------------------------------
  // rational
  //---------------------------------------------------------------------------
  // Must be a structural type for template NTTP (only public members)
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

    rational() = default;
    constexpr rational(std::unsigned_integral auto, umax = 1ull, sign = sign::detect);
    constexpr rational(std::signed_integral   auto, umax = 1ull);
    constexpr rational(std::floating_point    auto);

    // operator== be default for structural type
    constexpr bool operator==(const rational&) const = default;
    constexpr rational operator-() const;

    template <std::unsigned_integral T>
    constexpr operator T () const
    { return static_cast<T>(Numerator/Denominator); }

    explicit constexpr operator double() const
    { 
      double abs = static_cast<double>(Numerator)/static_cast<double>(Denominator);
      return (Sign == sign::negative) ? -abs : abs; 
    }

    // allow unary+ for generic programming
    constexpr rational operator+() const { return *this; }

    constexpr std::string_view to_string() const
    {
      if consteval
      {
        return "TODO"; 
      }
    
      return "TODO";
    }
  };
  
  //---------------------------------------------------------------------------
  // user defined literals for rational 
  //---------------------------------------------------------------------------
  constexpr rational operator ""_r(unsigned long long int numerator)
  { return rational{numerator}; }

  constexpr rational operator ""_r(long double value)
  { return rational{static_cast<double>(value)}; }

  constexpr rational operator+  (const rational&, const rational&); 
  constexpr rational operator-  (const rational&, const rational&); 
  constexpr rational operator*  (rational, rational); 
  constexpr rational operator/  (const rational&, const rational&); 
  constexpr auto     operator<=>(rational, rational) -> std::strong_ordering; 

  constexpr rational gcd(const rational&, const rational&);
  constexpr void trim(umax&, umax&);
  constexpr rational abs(rational);

  constexpr bool divides_evenly (const rational&, const rational&); 

  //---------------------------------------------------------------------------
  // abs 
  //---------------------------------------------------------------------------
  constexpr rational abs(rational value)
  { return (value.Sign == sign::negative) ? -value : value; }

  //---------------------------------------------------------------------------
  // gcd 
  //---------------------------------------------------------------------------
  constexpr rational gcd(const rational& lhs, const rational& rhs)
  {
    auto numerator   = std::gcd(lhs.Numerator  , rhs.Numerator);
    auto denominator = std::lcm(lhs.Denominator, rhs.Denominator);
    return rational{numerator, denominator};
  }

  //---------------------------------------------------------------------------
  // trim
  //---------------------------------------------------------------------------
  inline constexpr void trim(umax& numerator,umax& denominator)
  {
    //TODO divide by zero
    auto gcd = std::gcd(numerator, denominator);
    if (gcd == 0)
      return;
    numerator   /= gcd;
    denominator /= gcd;
  }

  //---------------------------------------------------------------------------
  // rational::rational 
  //---------------------------------------------------------------------------
  constexpr rational::rational(std::unsigned_integral auto num, umax den, sign s)
   :Numerator{num}, Denominator{den}, 
    Sign{Numerator == 0 ? sign::zero : (s == sign::detect) ? sign::positive: s}
  {
    if (Denominator == 0)
      throw "Denominator of Zero is invalid";
    if (Numerator != 0 && Sign == sign::zero)
      throw "Numerator is not Zero but sign is";

    trim(Numerator, Denominator);
  }

  constexpr rational::rational(std::signed_integral auto num, umax den)
   :Numerator{safe_abs(num)}, Denominator{den}, 
    Sign{num == 0 ? sign::zero : (num > 0) ? sign::positive: sign::negative}
  {
    if (Denominator == 0)
      throw "Denominator of Zero is invalid";

    trim(Numerator, Denominator);
  }

  constexpr rational::rational(std::floating_point auto value)
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
    // trim not needed, because abs_fractio already trims in its special case
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

    return rational
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
  inline constexpr auto operator<=>(rational lhs, rational rhs) -> std::strong_ordering
  { 
    if (lhs.Sign == sign::detect || rhs.Sign == sign::detect)
      throw "internal logic error";

    if (lhs.Sign != rhs.Sign)
    {
      if (lhs.Sign == sign::negative)
        return std::strong_ordering::less;

      if (rhs.Sign == sign::negative)
        return std::strong_ordering::greater;
    }

    if consteval // check mul overflow
    {
      // cross trim to avoid overflow if possible
      trim(lhs.Numerator, rhs.Numerator);
      trim(lhs.Denominator, rhs.Denominator);

      if 
      (
        mul_overflow(lhs.Numerator, rhs.Denominator) ||
        mul_overflow(rhs.Numerator, lhs.Denominator)
      )
        OVERFLOW_trap("multiplicative overflow");
    }

    auto A = lhs.Numerator * rhs.Denominator;
    auto B = rhs.Numerator * lhs.Denominator;

    if (lhs.Sign == sign::negative && rhs.Sign == sign::negative) 
      return B <=> A; 
    else
      return A <=> B; 
  }

  //---------------------------------------------------------------------------
  // operator* 
  //---------------------------------------------------------------------------
  inline constexpr rational operator*(rational lhs, rational rhs) 
  { 
    if (lhs.Sign == sign::zero || rhs.Sign == sign::zero)
      return 0_r;

    if consteval // check mul overflow
    {
      // cross trim to avoid overflow if possible
      trim(lhs.Numerator, rhs.Denominator);
      trim(rhs.Numerator, lhs.Denominator);

      if 
      (
        mul_overflow(lhs.Numerator  , rhs.Numerator)   ||
        mul_overflow(lhs.Denominator, rhs.Denominator)
      )
        OVERFLOW_trap("multiplicative overflow");
    }

    return rational
    {
      lhs.Numerator * rhs.Numerator,
      lhs.Denominator * rhs.Denominator, 
      (lhs.Sign == rhs.Sign) ? sign::positive : sign::negative
    };
  }

  //---------------------------------------------------------------------------
  // operator/ 
  //---------------------------------------------------------------------------
  inline constexpr rational operator/(const rational& lhs, const rational& rhs) 
  { 
    if (rhs.Sign == sign::zero)
      throw "division by zero imminent";

    return rational
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
      return 0_r;

    if (lhs.Sign == sign::zero)
      return rhs;

    if (rhs.Sign == sign::zero)
      return lhs;

    if consteval // check mul overflow
    {
      if 
      (
        mul_overflow(lhs.Denominator, rhs.Denominator) ||
        mul_overflow(lhs.Numerator  , rhs.Denominator) ||
        mul_overflow(rhs.Numerator  , lhs.Denominator)
      )
        OVERFLOW_trap("multiplicative overflow");
    }

    auto denominator = lhs.Denominator * rhs.Denominator;
    auto A = lhs.Numerator * rhs.Denominator;
    auto B = rhs.Numerator * lhs.Denominator;

    if (lhs.Sign == rhs.Sign)
    {
      if consteval 
      { 
        if (add_overflow(A, B))
          OVERFLOW_trap("additive overflow");
      }
      return rational{A + B, denominator, lhs.Sign}; 
    }

    auto numerator = (A > B) ? (A - B) : (B - A);

    if (lhs.Sign == sign::negative)
      return rational{numerator, denominator, (A > B) ? sign::negative : sign::positive}; 
    else
      return rational{numerator, denominator, (A > B) ? sign::positive : sign::negative}; 
  }

  //---------------------------------------------------------------------------
  // operator- 
  //---------------------------------------------------------------------------
  // this is only overlow safe in constexpr context
  //---------------------------------------------------------------------------
  inline constexpr rational operator-(const rational& lhs, const rational& rhs) 
  { 
    return operator+(lhs, -rhs);
  }

  //---------------------------------------------------------------------------
  // divides_evenly 
  //---------------------------------------------------------------------------
  inline constexpr bool divides_evenly(const rational& dividend, const rational& divisor)
  {
    return (divisor == 0_r) ? true : (dividend / divisor).Denominator == 1;
  }

} // namespace bnd

#endif // BNDrationalHPP
