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
    rational Step  = {}
  >
  class bound
  {
  };
} // namespace bnd

#endif // BNDboundHPP
