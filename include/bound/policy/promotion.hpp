//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDpromotionHPP
#define BNDpromotionHPP

#include "bound/common.hpp"

namespace bnd
{
  template <boundable L, boundable R>
  struct promotion
  {
    // addition 
    static constexpr grid     add_Grid     = L::Grid + R::Grid;
    static constexpr interval add_Interval = add_Grid.Interval;
    static constexpr rational add_Notch    = add_Grid.Notch;
    static constexpr rational add_Lower    = add_Interval.Lower;
    static constexpr rational add_Upper    = add_Interval.Upper;

    using add_return = bound<add_Lower, add_Upper, add_Notch>;
  };

} // namespace bnd

#endif // BNDpromotionHPP
