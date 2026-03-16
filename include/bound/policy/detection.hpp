//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDdetectionHPP
#define BNDdetectionHPP

#include "bound/common.hpp"
#include "bound/policy/waiver.hpp"

namespace bnd
{
  template <boundable L, boundable R = L>
  struct detection 
  { };

  template <boundable B>
  struct detection<B,B> 
  {
    template<waiver_flag F>
    static B::raw_type construct(auto value, waiver_type<F> waiver = {} ) 
    { return B::Grid.template to_raw<typename B::raw_type>(value, waiver); }
  };
} // namespace bnd

#endif // BNDdetectionHPP
