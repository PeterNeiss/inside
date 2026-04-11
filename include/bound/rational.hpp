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
  constexpr umax abs_den(imax d) { return (d >= 0) ? static_cast<umax>(d) : -static_cast<umax>(d); }

  //---------------------------------------------------------------------------
  // trim
  //---------------------------------------------------------------------------
  inline constexpr void trim(umax& numerator, imax& denominator)
  {
    umax ad = abs_den(denominator);
    auto g = std::gcd(numerator, ad);
    if (g == 0)
      return;
    numerator /= g;
    ad /= g;
    denominator = (denominator < 0) ? -static_cast<imax>(ad) : static_cast<imax>(ad);
  }

  inline constexpr void trim(umax& a, umax& b)
  {
    auto g = std::gcd(a, b);
    if (g == 0)
      return;
    a /= g;
    b /= g;
  }

  //---------------------------------------------------------------------------
  // rational
  //---------------------------------------------------------------------------
  // Must be a structural type for template NTTP (only public members)
  //---------------------------------------------------------------------------
  // Sign is encoded in the denominator (negative denominator = negative rational)
  // Numerator is unsigned to represent e.g. umax itself
  //---------------------------------------------------------------------------
  struct rational
  {
    umax Numerator;
    imax Denominator;

    constexpr rational() = default;
    constexpr rational(std::floating_point  auto);
    constexpr rational(std::signed_integral auto, imax = 1);
    constexpr rational(std::unsigned_integral auto num, imax den = 1)
     :Numerator{num}, Denominator{den}
    {
      if (Denominator == 0)
        throw "Denominator of Zero is invalid";
      if (Numerator == 0)
        Denominator = 1;
      trim(Numerator, Denominator);
    }

    // operator== be default for structural type
    constexpr bool operator==(const rational&) const = default;
    constexpr bool operator==(auto value) const { return operator==(rational{value}); }

    constexpr rational operator-() const;

    template <std::unsigned_integral T>
    constexpr slim::optional<T> to();

    template <std::unsigned_integral T>
    explicit constexpr operator T () const
    {
      if (Denominator < 0)
        throw "cannot convert negative rational to unsigned";
      return static_cast<T>(Numerator/static_cast<umax>(Denominator));
    }

    // allow unary+ for generic programming
    constexpr rational operator+() const { return *this; }

    static constexpr rational make_sentinel() noexcept
    { rational r; r.Numerator = 1; r.Denominator = 0; return r; }
  };

  constexpr slim::optional<rational> operator+(const rational&, const rational&);
  constexpr slim::optional<rational> operator/(rational, rational);
  constexpr slim::optional<rational> operator-(const rational&, const rational&);

  constexpr slim::optional<rational> operator*(rational, rational);
  constexpr auto     operator<=>(rational, rational) -> std::strong_ordering;

  constexpr rational gcd(const rational&, const rational&);
  constexpr rational abs(rational);

  constexpr bool divides_evenly(const rational&, const rational&);

  //---------------------------------------------------------------------------
  // abs
  //---------------------------------------------------------------------------
  constexpr rational abs(rational value)
  { return (value.Denominator < 0) ? -value : value; }

  //---------------------------------------------------------------------------
  // gcd
  //---------------------------------------------------------------------------
  constexpr rational gcd(const rational& lhs, const rational& rhs)
  {
    auto numerator   = std::gcd(lhs.Numerator, rhs.Numerator);
    auto denominator = std::lcm(abs_den(lhs.Denominator), abs_den(rhs.Denominator));
    return rational{numerator, static_cast<imax>(denominator)};
  }

  //---------------------------------------------------------------------------
  // rational::rational
  //---------------------------------------------------------------------------
  constexpr rational::rational(std::signed_integral auto num, imax den)
   :Numerator{safe_abs(num)},
    Denominator{ (num < 0) ? -den : den }
  {
    if (den == 0)
      throw "Denominator of Zero is invalid";
    if (Numerator == 0)
      Denominator = 1;
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
      return;
    }

    bool neg = (value < 0.0);
    if (neg) value = -value;

    auto [num, den] = abs_fraction(value);
    Numerator = num;
    Denominator = neg ? -static_cast<imax>(den) : static_cast<imax>(den);
    // trim not needed, because abs_fraction already trims in its special case
  }

  //---------------------------------------------------------------------------
  // to
  //---------------------------------------------------------------------------
  template <std::unsigned_integral T>
  constexpr slim::optional<T> rational::to()
  { return (Denominator <= 0) ? slim::nullopt : slim::make_optional<T>(static_cast<T>(Numerator/static_cast<umax>(Denominator))); }

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
    if (Numerator == 0)
      return *this;

    return rational{Numerator, -Denominator};
  }

  //---------------------------------------------------------------------------
  // operator<=>
  //---------------------------------------------------------------------------
  // this is only overlow safe in constexpr context
  //---------------------------------------------------------------------------
  inline constexpr auto operator<=>(rational lhs, rational rhs) -> std::strong_ordering
  {
    bool lhs_neg = lhs.Denominator < 0;
    bool rhs_neg = rhs.Denominator < 0;
    int lhs_sign = (lhs.Numerator == 0) ? 0 : (lhs_neg ? -1 : 1);
    int rhs_sign = (rhs.Numerator == 0) ? 0 : (rhs_neg ? -1 : 1);

    if (lhs_sign != rhs_sign)
      return lhs_sign <=> rhs_sign;

    if (lhs_sign == 0)
      return std::strong_ordering::equal;

    umax lhs_ad = abs_den(lhs.Denominator);
    umax rhs_ad = abs_den(rhs.Denominator);

    // cross trim to avoid overflow if possible
    trim(lhs.Numerator, rhs.Numerator);
    trim(lhs_ad, rhs_ad);

    umax A;
    umax B;

    if
    (
      mul_overflow(lhs.Numerator, rhs_ad, &A) ||
      mul_overflow(rhs.Numerator, lhs_ad, &B)
    )
      OVERFLOW_trap("multiplicative overflow");

    if (lhs_neg)
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
  inline constexpr slim::optional<rational> operator*(rational lhs, rational rhs)
  {
    if (lhs.Numerator == 0 || rhs.Numerator == 0)
      return 0_r;

    bool result_neg = (lhs.Denominator < 0) != (rhs.Denominator < 0);

    umax lhs_ad = abs_den(lhs.Denominator);
    umax rhs_ad = abs_den(rhs.Denominator);

    // cross trim to avoid overflow if possible
    trim(lhs.Numerator, rhs_ad);
    trim(rhs.Numerator, lhs_ad);

    umax numerator;
    umax denominator;

    if
    (
      mul_overflow(lhs.Numerator, rhs.Numerator, &numerator) ||
      mul_overflow(lhs_ad, rhs_ad, &denominator)
    )
      return slim::nullopt;

    imax signed_den = result_neg ? -static_cast<imax>(denominator) : static_cast<imax>(denominator);
    return rational{numerator, signed_den};
  }

  inline constexpr slim::optional<rational> operator*(auto lhs, const rational& rhs)
  { return rational{lhs} * rhs; }

  template <typename T>
  inline constexpr slim::optional<rational> operator*(slim::optional<T> lhs, const rational& rhs)
  {
    if (lhs.has_value())
      return rational{lhs.value()} * rhs;
    else
      return slim::nullopt;
  }

  inline constexpr slim::optional<rational> operator*(rational const& lhs, auto rhs)
  { return lhs * rational{rhs}; }

  template <typename T>
  inline constexpr slim::optional<rational> operator*(rational const& lhs, slim::optional<T> rhs)
  {
    if (rhs.has_value())
      return lhs * rational{rhs.value()};
    else
      return slim::nullopt;
  }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<rational> operator/(rational lhs, rational rhs)
  {
    if (rhs.Numerator == 0)
      return slim::nullopt;

    if (lhs.Numerator == 0)
      return 0_r;

    bool result_neg = (lhs.Denominator < 0) != (rhs.Denominator < 0);

    umax lhs_ad = abs_den(lhs.Denominator);
    umax rhs_ad = abs_den(rhs.Denominator);

    trim(lhs.Numerator, rhs.Numerator);
    trim(lhs_ad, rhs_ad);

    umax numerator;
    umax denominator;

    if
    (
      mul_overflow(lhs.Numerator, rhs_ad, &numerator) ||
      mul_overflow(rhs.Numerator, lhs_ad, &denominator)
    )
      return slim::nullopt;

    imax signed_den = result_neg ? -static_cast<imax>(denominator) : static_cast<imax>(denominator);
    return rational{numerator, signed_den};
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

    if (lhs.Numerator == 0)
      return rhs;

    if (rhs.Numerator == 0)
      return lhs;

    bool lhs_neg = lhs.Denominator < 0;
    bool rhs_neg = rhs.Denominator < 0;

    umax lhs_ad = abs_den(lhs.Denominator);
    umax rhs_ad = abs_den(rhs.Denominator);

    umax numerator;
    umax denominator;
    umax A;
    umax B;

    if
    (
      mul_overflow(lhs_ad, rhs_ad, &denominator) ||
      mul_overflow(lhs.Numerator, rhs_ad, &A)    ||
      mul_overflow(rhs.Numerator, lhs_ad, &B)
    )
      return slim::nullopt;

    if (lhs_neg == rhs_neg)
    {
      if (add_overflow(A, B, &numerator))
        return slim::nullopt;
      imax signed_den = lhs_neg ? -static_cast<imax>(denominator) : static_cast<imax>(denominator);
      return rational{numerator, signed_den};
    }

    numerator = (A > B) ? (A - B) : (B - A);
    bool result_neg = lhs_neg ? (A > B) : (B > A);

    imax signed_den = result_neg ? -static_cast<imax>(denominator) : static_cast<imax>(denominator);
    return rational{numerator, signed_den};
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
    return (divisor == 0_r) ? true : abs_den((dividend / divisor).value().Denominator) == 1;
  }

} // namespace bnd

namespace slim
{
  constexpr bnd::rational sentinel_traits<bnd::rational>::sentinel() noexcept { return bnd::rational::make_sentinel(); }
  constexpr bool sentinel_traits<bnd::rational>::is_sentinel(const bnd::rational& v) noexcept
  { return v.Denominator == 0; }
} // namespace slim

#endif // BNDrationalHPP

