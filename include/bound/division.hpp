//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDdivisionHPP
#define BNDdivisionHPP

#include "bound/common.hpp"
#include "bound/utility/grid.hpp"
#include "bound/waiver.hpp"
/*
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
    using raw_type = result::raw_type;

    template <waiver_flag F = none>
    static constexpr result mul(L, R, waiver_type<F> = {});
  };

  //---------------------------------------------------------------------------
  // mul 
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<waiver_flag F>
  constexpr auto multiplication<L,R>::mul(L lhs, R rhs, waiver_type<F> waiver) -> result
  { 
    if constexpr (result::Grid.Notch.Sign == sign::zero)
      return result::from_raw(lhs.to_rational() * rhs.to_rational());
    else
    {
      if constexpr (mul_Grid.Interval.Lower == L::Grid.Interval.Lower * R::Grid.Interval.Lower)
      {
        // low_per_notch is always positive in this case
        return result::from_raw
        (
          static_cast<raw_type>
            (
              lhs.Raw * rhs.Raw + 
              lhs.Raw * R::Grid.low_per_notch().Numerator +
              rhs.Raw * L::Grid.low_per_notch().Numerator 
            )
        );
      }

      if constexpr (mul_Grid.Interval.Lower == L::Grid.Interval.Upper * R::Grid.Interval.Upper)
      { return multiplication<typename L::negative, typename R::negative>::mul(-lhs, -rhs, waiver); }

      if constexpr (mul_Grid.Interval.Lower == L::Grid.Interval.Upper * R::Grid.Interval.Lower)
      { 
        typename L::raw_type negRaw = static_cast<typename L::raw_type>(L::Grid.max_notch()) - lhs.Raw;
        return result::from_raw
        (
          static_cast<raw_type>
            (
              // Check math, its correct and necessary to drop sign here
              (negRaw * R::Grid.low_per_notch().Numerator + rhs.Raw * L::Grid.up_per_notch().Numerator) - (negRaw * rhs.Raw) 
            )
        );
      }

      if constexpr (mul_Grid.Interval.Lower == L::Grid.Interval.Lower * R::Grid.Interval.Upper)
      { return -multiplication<L, typename R::negative>::mul(lhs, -rhs, waiver); }

      throw "multiplication: internal logic error";
    }
  }
} // namespace bnd
*/
#endif // BNDdivisionHPP
