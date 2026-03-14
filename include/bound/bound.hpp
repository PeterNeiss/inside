//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDboundHPP
#define BNDboundHPP

#include "bound/utility/rational.hpp"

namespace bnd
{
  template
  <
    rational Lower = {},
    rational Upper = {},
    rational Notch  = {1} 
  >
  class bound
  {
    static_assert(Notch >= 0);
  };

  //TODO wrap_bound, sat_bound
  //safe_loop, force_add,
} // namespace bnd

#endif // BNDboundHPP
