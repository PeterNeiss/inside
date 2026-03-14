//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDboundHPP
#define BNDboundHPP

#include "bound/utility/rational.hpp"
//#include "bound/concepts.hpp"
// concept always_addable<A,B>, maybe_addable, never_addable

namespace bnd
{
  template
  <
    rational Lower = {},
    rational Upper = Lower,
    rational Notch = {1} 
  >
  class bound
  {
    static_assert(Lower <= Upper); // TODO remove: check in interval
    static_assert(0 <= Notch); 
    static_assert(divides_evenly((Upper - Lower), Notch));
  };

  //TODO wrap_bound, sat_bound
  //safe_loop, force_add,
} // namespace bnd

#endif // BNDboundHPP
