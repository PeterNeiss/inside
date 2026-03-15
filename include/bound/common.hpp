//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDcommonHPP
#define BNDcommonHPP

#include "bound/policy/waiver.hpp"
#include "bound/utility/math.hpp"
#include "bound/utility/rational.hpp"
#include "bound/utility/interval.hpp"
#include "bound/utility/grid.hpp"

namespace bnd
{
  template
  <
    rational Lower = {},
    rational Upper = Lower,
    rational Notch = {1}
  >
  struct bound;
} // namespace bnd

#endif // BNDcommonHPP
