//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDmultiplicationHPP
#define BNDmultiplicationHPP

#include "bound/common.hpp"
#include "bound/utility/grid.hpp"
#include "bound/waiver.hpp"

namespace bnd
{
  template <boundable L, boundable R = L>
  struct multiplication 
  { 
    static constexpr grid     mul_Grid     = L::Grid * R::Grid;
    static constexpr interval mul_Interval = mul_Grid.Interval;
    static constexpr rational mul_Notch    = mul_Grid.Notch;
    static constexpr rational mul_Lower    = mul_Interval.Lower;
    static constexpr rational mul_Upper    = mul_Interval.Upper;

    using result = bound<mul_Lower, mul_Upper, mul_Notch>;

    template <waiver_flag F = none>
    static constexpr result mul(L, R, waiver_type<F> = {});
  };

  //---------------------------------------------------------------------------
  // mul 
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<waiver_flag F>
  constexpr auto multiplication<L,R>::mul(L lhs, R rhs, waiver_type<F>) -> result
  { 
    return 0;
  }
} // namespace bnd

#endif // BNDadditionHPP
