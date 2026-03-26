//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDdivisionHPP
#define BNDdivisionHPP

#include "bound/common.hpp"
#include "bound/utility/grid.hpp"
#include "bound/policy.hpp"

namespace bnd
{
  template <boundable L, boundable R = L>
  struct division 
  { 
    using result = bound<{L::Grid.Interval / R::Grid.Interval, 0}>;
    using raw_type = result::raw_type;

    template <policy_flag F = none>
    static constexpr result div(L, R, policy<F> = {});
  };

  //---------------------------------------------------------------------------
  // div 
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<policy_flag F>
  constexpr auto division<L,R>::div(L lhs, R rhs, policy<F>) -> result
  { 
    return lhs.to_rational() / rhs.to_rational();
  }
} // namespace bnd

#endif // BNDdivisionHPP
