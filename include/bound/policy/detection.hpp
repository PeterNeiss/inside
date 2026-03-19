//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDdetectionHPP
#define BNDdetectionHPP

#include "bound/common.hpp"
#include "bound/policy/waiver.hpp"

#include <limits>

namespace bnd
{
  //---------------------------------------------------------------------------
  // detection<L,R>
  //---------------------------------------------------------------------------
  template <boundable L, boundable R = L>
  struct detection 
  { };

  //---------------------------------------------------------------------------
  // detection<B,B> 
  //---------------------------------------------------------------------------
  template <boundable B>
  struct detection<B,B> 
  {
    using raw_type = B::raw_type;

    // construct
    static constexpr waiver_flag default_construct_flag = none;

    template<waiver_flag F = default_construct_flag>
    static constexpr raw_type construct(rational value, waiver_type<F> waiver = {});

    template<arithmetic V, waiver_flag F = default_construct_flag>
    static constexpr raw_type construct(V value, waiver_type<F> waiver = {}); 

    // add 
    static constexpr waiver_flag default_add_flag = none;

    template<boundable Ret, waiver_flag F = default_construct_flag>
    static constexpr Ret add(B, B, waiver_type<F> = {});
  };

  //---------------------------------------------------------------------------
  // construct 
  //---------------------------------------------------------------------------
  template<boundable B>
  template<arithmetic V, waiver_flag F>
  constexpr auto detection<B,B>::construct(V value, waiver_type<F> waiver) -> raw_type 
  { 
    static constexpr interval value_interval = 
      {std::numeric_limits<V>::lowest(), std::numeric_limits<V>::max()};

    // can never construct
    if (B::Interval.excludes(value_interval))
      throw "Can never construct";

    // can always construct
    if (B::Interval.includes(value_interval))
      return B::Grid.template to_raw<raw_type>(value); 
   
    // can maybe construct
    if consteval
    {
      // always check at compile time
      if (not B::Interval.includes(value))
        throw "value not in interval";
    }  
    else 
    {
      if (not waiver.test(no_runtime_check) && not B::Interval.includes(value))
        throw "value not in interval(runtime)";
    }

    return B::Grid.template to_raw<raw_type>(value); 
  }

  //---------------------------------------------------------------------------
  // construct 
  //---------------------------------------------------------------------------
  template<boundable B>
  template<waiver_flag F>
  constexpr auto detection<B,B>::construct(rational value, waiver_type<F> waiver) -> raw_type 
  { 
    // can never construct: false
    // can always construct: false
    // can maybe construct
    if consteval
    {
      // always check at compile time
      if (not B::Interval.includes(value))
        throw "value not in interval";
    }  
    else 
    {
      if (not waiver.test(no_runtime_check) && not B::Interval.includes(value))
        throw "value not in interval(runtime)";
    }

    return B::Grid.template to_raw<raw_type>(value); 
  }

  //---------------------------------------------------------------------------
  // add 
  //---------------------------------------------------------------------------
  template<boundable B>
  template<boundable Ret, waiver_flag F>
  constexpr auto detection<B,B>::add(B lhs, B rhs, waiver_type<F>) -> Ret
  { 
    // TODO Detect overflows,etc. 
    return Ret::from_raw
    (
      static_cast<Ret::raw_type>
      (
        static_cast<Ret::raw_type>(lhs.Raw) * (Ret::Grid.Notch / lhs.Grid.Notch).Numerator + 
        static_cast<Ret::raw_type>(rhs.Raw) * (Ret::Grid.Notch / rhs.Grid.Notch).Numerator
      )
    );
  }
} // namespace bnd

#endif // BNDdetectionHPP
