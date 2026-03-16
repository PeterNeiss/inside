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
  //---------------------------------------------------------------------------
  struct grid 
  {
    interval Interval;
    rational Notch;

    constexpr grid(interval val, rational notch = 1):Interval{val}, Notch{notch}
    {
      if (not Interval.divides_evenly(Notch))
        throw "Notch does not evenly divide the interval";
    }

    constexpr umax max_notch() const { return (Interval/Notch).Numerator; }

    // operator== be default for structural type
    constexpr bool operator==(const grid& rhs) const = default;
    constexpr grid operator-() const { return {-Interval, Notch}; } 
 
    template <std::unsigned_integral Raw, waiver_flag F>
    constexpr Raw to_raw(std::signed_integral auto value, waiver_type<F> = {}) const
    {
      return static_cast<Raw>(value);
    }
  };
/*  
  constexpr interval operator+  (const interval&, const interval&); 
  constexpr interval operator-  (const interval&, const interval&); 
  constexpr interval operator*  (const interval&, const interval&); 
  constexpr interval operator/  (const interval&, const interval&); 
  constexpr auto     operator<=>(const interval&, const interval&) -> std::partial_ordering; 
  // TODO includes, excludes

  //---------------------------------------------------------------------------
  // operator+ 
  //---------------------------------------------------------------------------
  inline constexpr interval operator+(const interval& lhs, const interval& rhs) 
  { 
    return {lhs.Lower + rhs.Lower, lhs.Upper + rhs.Upper}; 
  }

  //---------------------------------------------------------------------------
  // operator- 
  //---------------------------------------------------------------------------
  inline constexpr interval operator-(const interval& lhs, const interval& rhs) 
  { 
    return operator+(lhs, -rhs);
  }

  //---------------------------------------------------------------------------
  // operator* 
  //---------------------------------------------------------------------------
  // this is only overlow safe in constexpr context
  //---------------------------------------------------------------------------
  inline constexpr interval operator*(const interval& lhs, const interval& rhs) 
  { 
    auto [lower, upper] = std::minmax
    (
      {
        lhs.Lower * rhs.Lower,
        lhs.Lower * rhs.Upper,
        lhs.Upper * rhs.Lower,
        lhs.Upper * rhs.Upper 
      }
    );
  
    return {lower, upper};
  }

  //---------------------------------------------------------------------------
  // operator/ 
  //---------------------------------------------------------------------------
  // this is only overlow safe in constexpr context
  //---------------------------------------------------------------------------
  inline constexpr interval operator/(const interval& lhs, const interval& rhs) 
  { 
    if (rhs.includes(0))
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
