//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDcommonHPP
#define BNDcommonHPP

#include "bound/debug.hpp"
#include "bound/utility/grid.hpp"

namespace bnd
{
  template <typename B>
  concept boundable = requires(B b)
  {
    { B::Grid } -> std::same_as<grid const&>;
    typename B::raw_type;
  };

  template <typename N>
  concept numeric = boundable<N> or arithmetic<N>; 

  template<grid G = {{0, 0}, 0}> struct bound;

} // namespace bnd

#endif // BNDcommonHPP
