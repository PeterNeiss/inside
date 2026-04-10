//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDrationalHPP
#define BNDrationalHPP

#include "bound/common.hpp"
#include "bound/math.hpp"
#include "bound/overflow.hpp"

#include <numeric>
#include <compare>
#include <cmath>
#include <tuple>

namespace bnd { struct rational; }

namespace slim
{
  template<>
  struct sentinel_traits<bnd::rational>
  {
    protected:
      static constexpr bnd::rational sentinel() noexcept;
      static constexpr bool is_sentinel(const bnd::rational& v) noexcept;
  };
} // namespace slim

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
  enum class sign {negative = -1, zero = 0, positive = 1};
  struct rational
  {
    umax Numerator;
    umax Denominator;
    sign Sign;

    constexpr rational() = default;
    constexpr rational(std::floating_point  auto);
    constexpr rational(std::signed_integral auto, umax = 1ull);
    constexpr rational(std::unsigned_integral auto, umax, sign);
    constexpr rational(std::unsigned_integral auto num, umax den = 1ull)
     :rational{num, den, (num == 0) ? sign::zero : sign::positive} { }

    // operator== be default for structural type
    constexpr bool operator==(const rational&) const = default;
    constexpr bool operator==(auto value) const { return operator==(rational{value}); }

    constexpr rational operator-() const;

    template <std::unsigned_integral T>
    constexpr slim::optional<T> to();

    template <std::unsigned_integral T>
    explicit constexpr operator T () const
    { return static_cast<T>(Numerator/Denominator); }

    // allow unary+ for generic programming
    constexpr rational operator+() const { return *this; }

  };

  constexpr slim::optional<rational> operator+(const rational&, const rational&);
  constexpr slim::optional<rational> operator/(const rational&, const rational&);
  constexpr slim::optional<rational> operator-(const rational&, const rational&);

  constexpr rational operator*  (rational, rational);
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
   :Numerator{num}, Denominator{den}, Sign{s}
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
  // to
  //---------------------------------------------------------------------------
  template <std::unsigned_integral T>
  constexpr slim::optional<T> rational::to()
  { return (Denominator == 0) ? slim::nullopt : slim::make_optional<T>(static_cast<T>(Numerator/Denominator)); }

  //---------------------------------------------------------------------------
  // user defined literals for rational
  //---------------------------------------------------------------------------
  constexpr rational operator ""_r(unsigned long long int numerator)
  { return rational{numerator}; }

  constexpr rational operator ""_r(long double value)
  { return rational{static_cast<double>(value)}; }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  inline constexpr rational rational::operator-() const
  {
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

  template <typename T>
  inline constexpr auto operator<=>(slim::optional<T> lhs, const rational& rhs)
  { return rational{lhs.value()} <=> rhs; }

  template <typename T>
  inline constexpr auto operator<=>(rational const& lhs, slim::optional<T> rhs)
  { return lhs <=> rational{rhs.value()}; }

  inline constexpr auto operator<=>(auto lhs, const rational& rhs)
  { return rational{lhs} <=> rhs; }

  inline constexpr auto operator<=>(rational const& lhs, auto rhs)
  { return lhs <=> rational{rhs}; }

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

  inline constexpr rational operator*(auto lhs, const rational& rhs)
  { return rational{lhs} * rhs; }

  template <typename T>
  inline constexpr rational operator*(slim::optional<T> lhs, const rational& rhs)
  { return rational{lhs.value()} * rhs; }

  inline constexpr rational operator*(rational const& lhs, auto rhs)
  { return lhs * rational{rhs}; }

  template <typename T>
  inline constexpr rational operator*(rational const& lhs, slim::optional<T> rhs)
  { return lhs * rational{rhs.value()}; }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<rational> operator/(const rational& lhs, const rational& rhs)
  {
    if (rhs.Sign == sign::zero)
      return slim::nullopt;

    return rational
    {
      lhs.Numerator * rhs.Denominator,
      rhs.Numerator * lhs.Denominator,
      (lhs.Sign == rhs.Sign) ? sign::positive : sign::negative
    };
  }

  inline constexpr slim::optional<rational> operator/(auto lhs, const rational& rhs)
  { return rational{lhs} / rhs; }

  template <typename T>
  inline constexpr slim::optional<rational> operator/(slim::optional<T> lhs, const rational& rhs)
  {
    if (lhs.has_value())
      return rational{*lhs} / rhs;
    else
      return slim::nullopt;
  }

  inline constexpr slim::optional<rational> operator/(rational const& lhs, auto rhs)
  { return lhs / rational{rhs}; }

  template <typename T>
  inline constexpr slim::optional<rational> operator/(rational const& lhs, slim::optional<T> rhs)
  {
    if (rhs.has_value())
      return lhs / rational{*rhs};
    else
      return slim::nullopt;
  }

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<rational> operator+(const rational& lhs, const rational& rhs)
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

    umax numerator;

    if (lhs.Sign == rhs.Sign)
    {
      if (add_overflow(A, B, &numerator))
        return slim::nullopt;
      else
        return rational{numerator, denominator, lhs.Sign};
    }

    numerator = (A > B) ? (A - B) : (B - A);

    if (lhs.Sign == sign::negative)
      return rational{numerator, denominator, (A > B) ? sign::negative : sign::positive};
    else
      return rational{numerator, denominator, (A > B) ? sign::positive : sign::negative};
  }

  inline constexpr slim::optional<rational> operator+(auto lhs, const rational& rhs)
  { return rational{lhs} + rhs; }

  inline constexpr slim::optional<rational> operator+(rational const& lhs, auto rhs)
  { return lhs + rational{rhs}; }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<rational> operator-(const rational& lhs, const rational& rhs)
  { return operator+(lhs, -rhs); }

  inline constexpr slim::optional<rational> operator-(auto lhs, const rational& rhs)
  { return rational{lhs} - rhs; }

  template <typename T>
  inline constexpr slim::optional<rational> operator-(slim::optional<T> lhs, const rational& rhs)
  {
    if (lhs.has_value())
      return rational{lhs.value()} - rhs;
    else
      return slim::nullopt;
  }

  inline constexpr slim::optional<rational> operator-(rational const& lhs, auto rhs)
  { return lhs - rational{rhs}; }

  template <typename T>
  inline constexpr slim::optional<rational> operator-(rational const& lhs, slim::optional<T> rhs)
  {
    if (rhs.has_value())
      return lhs - rational{rhs.value()};
    else
      return slim::nullopt;
  }

  //---------------------------------------------------------------------------
  // divides_evenly
  //---------------------------------------------------------------------------
  inline constexpr bool divides_evenly(const rational& dividend, const rational& divisor)
  {
    return (divisor == 0_r) ? true : (dividend / divisor).value().Denominator == 1;
  }

} // namespace bnd

namespace slim
{
  constexpr bnd::rational sentinel_traits<bnd::rational>::sentinel() noexcept { return {1,0}; }
  constexpr bool sentinel_traits<bnd::rational>::is_sentinel(const bnd::rational& v) noexcept
  { return v.Denominator == 0ull; }
} // namespace slim

#endif // BNDrationalHPP

