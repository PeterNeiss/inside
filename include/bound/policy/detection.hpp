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
  template <boundable L, boundable R = L>
  struct detection 
  { };

  template <boundable B>
  struct detection<B,B> 
  {
    using raw_type = B::raw_type;
    static constexpr waiver_flag default_construct_flag = none;

    template<arithmetic V, waiver_flag F = default_construct_flag>
    static constexpr raw_type construct(V value, waiver_type<F> waiver = {} ) 
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
  };
} // namespace bnd

#endif // BNDdetectionHPP
