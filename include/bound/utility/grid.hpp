//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDgridHPP
#define BNDgridHPP

#include "bound/utility/rational.hpp"
#include "bound/utility/interval.hpp"

#include <algorithm>
#include <initializer_list>

namespace bnd
{
  //---------------------------------------------------------------------------
  // grid 
  //---------------------------------------------------------------------------
  // Must be a structural type for template NTTP (only public members)
  //---------------------------------------------------------------------------
  // The grid has an interval space by notches 
  // The interval must divide evenly by the notches. 
  // If Notch is zero, all rationals in the interval are allowed, raw is not offset
  //---------------------------------------------------------------------------
  struct grid 
  {
    interval Interval;
    rational Notch;

    grid() = default; 
    constexpr grid(arithmetic auto lower, arithmetic auto upper, arithmetic auto notch)
      :grid{interval{lower, upper}, rational{notch}} { }
    constexpr grid(arithmetic auto lower, arithmetic auto upper)
      :grid{interval{lower, upper}, 1_r} { }
    constexpr grid(arithmetic auto lower)
      :grid{interval{lower, lower}, 0_r} { }
    constexpr grid(interval val, rational notch):Interval{val}, Notch{notch} { }
   
    constexpr bool validate() const
    {
      if (not Interval.validate())
        return false;

      if (not Interval.divides_evenly(Notch))
        return false;

      if (Notch == 0)
        return true;
      else
        return (Interval.Lower/Notch).Denominator == 1;  // Grid extension must include 0
    }

    constexpr umax max_notch() const
    { return (Notch == 0_r) ? 0 : (Interval/Notch).Numerator; }

    constexpr rational low_per_notch() const
    { return (Notch == 0_r) ? 0_r : Interval.Lower/Notch; }

    constexpr rational up_per_notch() const
    { return (Notch == 0_r) ? 0_r : Interval.Upper/Notch; }


    // operator== be default for structural type
    constexpr bool operator==(const grid& rhs) const = default;
    constexpr grid operator-() const { return {-Interval, Notch}; } 
/* 
    template <std::unsigned_integral Raw>
    constexpr Raw to_raw(std::signed_integral auto value) const
    { 
      rational raw;
      raw.Denominator = 1;
      if (value < 0)
      {
        raw.Sign = sign::negative;
 
      }
      rational raw = (value - Interval.Lower)/Notch;
      return static_cast<Raw>(raw.Numerator/ raw.Denominator);  
    }
*/
    template <std::unsigned_integral Raw>
    constexpr Raw to_raw(rational value) const
    { 
      //TODO: check safty of calculation
      rational raw = (value - Interval.Lower)/Notch;
      return static_cast<Raw>(raw.Numerator/ raw.Denominator);  
    }

    template <std::same_as<rational> Raw>
    constexpr rational to_raw(rational value) const
    { 
      return (Notch == 0_r) ? value : (value - Interval.Lower)/Notch;  
    }

    constexpr double raw_to_double(std::unsigned_integral auto raw) const
    { return static_cast<double>(raw*Notch + Interval.Lower); }

    constexpr double raw_to_double(std::same_as<rational> auto raw) const
    { 
      return (Notch == 0_r) ? static_cast<double>(raw) : 
        static_cast<double>(raw*Notch + Interval.Lower);  
    }
  };

  constexpr grid operator+(const grid&, const grid&); 
  constexpr grid operator-(const grid&, const grid&); 
  constexpr interval operator*  (const interval&, const interval&); 

  //---------------------------------------------------------------------------
  // operator+ 
  //---------------------------------------------------------------------------
  inline constexpr grid operator+(const grid& lhs, const grid& rhs) 
  { 
    return {lhs.Interval + rhs.Interval, gcd(lhs.Notch, rhs.Notch)}; 
  }

  //---------------------------------------------------------------------------
  // operator- 
  //---------------------------------------------------------------------------
  inline constexpr grid operator-(const grid& lhs, const grid& rhs) 
  { 
    return operator+(lhs, -rhs);
  }

  //---------------------------------------------------------------------------
  // operator* 
  //---------------------------------------------------------------------------
  inline constexpr grid operator*(const grid& lhs, const grid& rhs) 
  { 
    return {lhs.Interval * rhs.Interval, lhs.Notch * rhs.Notch}; 
  }

/*  
  constexpr interval operator/  (const interval&, const interval&); 
  constexpr auto     operator<=>(const interval&, const interval&) -> std::partial_ordering; 
  // TODO includes, excludes

  //---------------------------------------------------------------------------
  // operator/ 
  //---------------------------------------------------------------------------
  // this is only overlow safe in constexpr context
  //---------------------------------------------------------------------------
  inline constexpr interval operator/(const interval& lhs, const interval& rhs) 
  { 
    if (rhs.includes(0_r))
      throw "division by zero imminent";

    auto [lower, upper] = std::minmax
    (
      {
        lhs.Lower / rhs.Lower,
        lhs.Lower / rhs.Upper,
        lhs.Upper / rhs.Lower,
        lhs.Upper / rhs.Upper 
      }
    );
  
    return {lower, upper};
  }
*/
} // namespace bnd

#endif // BNDgridHPP
