//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDadditionHPP
#define BNDadditionHPP

#include "bound/common.hpp"
#include "bound/utility/grid.hpp"
#include "bound/policy.hpp"

namespace bnd
{
  template <boundable L, boundable R = L>
  struct addition 
  { 
    using result = bound<L::Grid + R::Grid>;

    template <policy_flag F = none>
    static constexpr result add(L, R, policy<F> = {});
  };

  //---------------------------------------------------------------------------
  // add 
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<policy_flag F>
  constexpr auto addition<L,R>::add(L lhs, R rhs, policy<F>) -> result
  { 
    using raw = result::raw_type;
    constexpr raw lhs_widen = (result::Grid.Notch / lhs.Grid.Notch).Numerator; 
    constexpr raw rhs_widen = (result::Grid.Notch / rhs.Grid.Notch).Numerator; 

    // TODO Check argument
    // Because result type calculation did not overflow at compile time,
    // both widen multiplications dont overflow and
    // their sum does not overflow
    return result::from_raw(static_cast<raw>(lhs.Raw * lhs_widen + rhs.Raw * rhs_widen));
  }
} // namespace bnd

#endif // BNDadditionHPP
