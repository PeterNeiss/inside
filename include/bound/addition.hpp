//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDadditionHPP
#define BNDadditionHPP

#include "bound/common.hpp"
#include "bound/utility/grid.hpp"
#include "bound/waiver.hpp"

namespace bnd
{
  template <boundable L, boundable R = L>
  struct addition 
  { 
    static constexpr grid     add_Grid     = L::Grid + R::Grid;
    static constexpr interval add_Interval = add_Grid.Interval;
    static constexpr rational add_Notch    = add_Grid.Notch;
    static constexpr rational add_Lower    = add_Interval.Lower;
    static constexpr rational add_Upper    = add_Interval.Upper;

    using result = bound<add_Lower, add_Upper, add_Notch>;
    static constexpr waiver_flag default_flag = none;

    template <waiver_flag F = default_flag>
    static constexpr result add(L, R, waiver_type<F> = {});
  };

  //---------------------------------------------------------------------------
  // add 
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<waiver_flag F>
  constexpr auto addition<L,R>::add(L lhs, R rhs, waiver_type<F>) -> result
  { 
    using raw = result::raw_type;
    constexpr raw lhs_widen = (result::Grid.Notch / lhs.Grid.Notch).Numerator; 
    constexpr raw rhs_widen = (result::Grid.Notch / rhs.Grid.Notch).Numerator; 

    // Because result type calculation did not overflow at runtime,
    // both widen multiplications dont overflow and
    // their sum does not overflow
    return result::from_raw(static_cast<raw>(lhs.Raw * lhs_widen + rhs.Raw * rhs_widen));
  }
} // namespace bnd

#endif // BNDadditionHPP
