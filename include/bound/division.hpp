//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDdivisionHPP
#define BNDdivisionHPP

#include "bound/common.hpp"
#include "bound/utility/grid.hpp"
#include "bound/policy.hpp"

namespace bnd
{
  template <boundable L, boundable R = L>
  struct division
  {
    using result = bound<{Interval<L> / Interval<R>, 0}>;
    static_assert (std::is_same_v<typename result::raw_type, rational>);

    static constexpr result to_result(auto raw)
    { result res; res.Raw = static_cast<result::raw_type>(raw); return res; }

    template <policy_flag F = none>
    static constexpr result div(L, R, policy<F> = {});
  };

  //---------------------------------------------------------------------------
  // div
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<policy_flag F>
  constexpr auto division<L,R>::div(L lhs, R rhs, policy<F>) -> result
  {
    return to_result(static_cast<rational>(lhs) / static_cast<rational>(rhs));
  }
} // namespace bnd

#endif // BNDdivisionHPP
