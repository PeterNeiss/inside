//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDconstructionHPP
#define BNDconstructionHPP

#include "bound/utility/grid.hpp"
#include "bound/policy.hpp"

namespace bnd
{
  //---------------------------------------------------------------------------
  // construction 
  //---------------------------------------------------------------------------
  template <boundable B>
  struct construction 
  {
    using raw_type = B::raw_type;
    static constexpr policy_flag default_flag = none;

    template<policy_flag F = default_flag>
    static constexpr raw_type from_value(rational value, policy<F> waiver = {});

    template<std::integral V, policy_flag F = default_flag>
    static constexpr raw_type from_value(V value, policy<F> waiver = {}); 

    template<std::floating_point V, policy_flag F = default_flag>
    static constexpr raw_type from_value(V value, policy<F> waiver = {}); 
  };

  //---------------------------------------------------------------------------
  // from_value(integral)
  //---------------------------------------------------------------------------
  template<boundable B>
  template<std::integral V, policy_flag F>
  constexpr auto construction<B>::from_value(V value, policy<F> waiver) -> raw_type 
  { 
    constexpr interval value_interval = 
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
      if (not waiver.test(ignore_all) && not B::Interval.includes(value))
        throw "value not in interval(runtime)";
    }

    return B::Grid.template to_raw<raw_type>(value); 
  }

  //---------------------------------------------------------------------------
  // from_value(floating_point)
  //---------------------------------------------------------------------------
  template<boundable B>
  template<std::floating_point V, policy_flag F>
  constexpr auto construction<B>::from_value(V value, policy<F> waiver) -> raw_type 
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
      if (not waiver.test(ignore_all) && not B::Interval.includes(value))
        throw "value not in interval(runtime)";
    }

    return B::Grid.template to_raw<raw_type>(value); 
  }

  //---------------------------------------------------------------------------
  // from_value(rational)
  //---------------------------------------------------------------------------
  template<boundable B>
  template<policy_flag F>
  constexpr auto construction<B>::from_value(rational value, policy<F> waiver) -> raw_type 
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
      if (not waiver.test(ignore_all) && not B::Interval.includes(value))
        throw "value not in interval(runtime)";
    }

    return B::Grid.template to_raw<raw_type>(value); 
  }
} // namespace bnd

#endif // BNDconstructionHPP
