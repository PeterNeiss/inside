//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDmitigationHPP
#define BNDmitigationHPP

#include "bound/common.hpp"

namespace bnd
{
  template <boundable L, boundable R>
  struct mitigation
  {
    static constexpr auto default_waiver_add = waiver<none>;
  };

} // namespace bnd

#endif // BNDmitigationHPP
