//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDdivisionHPP
#define BNDdivisionHPP

#include "bound/common.hpp"
#include "bound/utility/grid.hpp"
#include "bound/waiver.hpp"

namespace bnd
{
  template <boundable L, boundable R = L>
  struct division 
  { 
    using result = bound<{L::Grid.Interval / R::Grid.Interval, 0}>;
    using raw_type = result::raw_type;

    template <waiver_flag F = none>
    static constexpr result div(L, R, waiver_type<F> = {});
  };

  //---------------------------------------------------------------------------
  // div 
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<waiver_flag F>
  constexpr auto division<L,R>::div(L lhs, R rhs, waiver_type<F>) -> result
  { 
    return lhs.to_rational() / rhs.to_rational();
  }
} // namespace bnd

#endif // BNDdivisionHPP
