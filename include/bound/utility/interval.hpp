//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDintervalHPP
#define BNDintervalHPP

#include "bound/utility/rational.hpp"
#include "bound/concept.hpp"

#include <algorithm>
#include <initializer_list>

namespace bnd
{
  //---------------------------------------------------------------------------
  // interval 
  //---------------------------------------------------------------------------
  // Must be a structural type for template NTTP (only public members)
  //---------------------------------------------------------------------------
  // The interval has inclusive upper and lower bounds.
  // At least one rational is in the interval
  // The rationals are embedded in the interval with rational == Lower == Upper
  //---------------------------------------------------------------------------
  struct interval 
  {
    rational Lower;
    rational Upper;

    constexpr interval(arithmetic auto lower = 0):Lower{lower}, Upper{lower} { }
    constexpr interval(arithmetic auto lower, arithmetic auto upper):Lower{lower}, Upper{upper} { }
    constexpr interval(std::initializer_list<rational> list)
    {
      if (std::empty(list))
      {
        Lower = 0;
        Upper = 0;
      }
      
      if (list.size() == 1)
      {
        Lower = *list.begin();
        Upper = Lower;
      }

      if (list.size() == 2)
      {
        Lower = *list.begin();
        Upper = *(std::next(list.begin()));
      }
    }

    // operator== be default for structural type
    constexpr bool operator==(const interval& rhs) const = default;
    constexpr interval operator-() const { return {-Upper, -Lower}; } 

    constexpr bool includes(interval const& rhs) const
    { return Lower <= rhs.Lower && rhs.Upper <= Upper; } 

    // NOT equivalent to !includes()
    constexpr bool excludes(interval const& rhs) const
    { return rhs.Upper < Lower || Upper < rhs.Lower; } 
  };
  
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

  //---------------------------------------------------------------------------
  // operator<=> 
  //---------------------------------------------------------------------------
  inline constexpr auto operator<=>(const interval& lhs, const interval& rhs) -> std::partial_ordering 
  {
    if (lhs.Upper < rhs.Lower) 
      return std::partial_ordering::less;
 
    if (lhs.Lower > rhs.Upper)
      return std::partial_ordering::greater;

    if (rhs.Lower == rhs.Lower && lhs.Upper == rhs.Upper) 
      return std::partial_ordering::equivalent;

    return std::partial_ordering::unordered;
  }

} // namespace bnd

#endif // BNDintervalHPP
