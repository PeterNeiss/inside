//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDdivisionHPP
#define BNDdivisionHPP

#include "bound/common.hpp"
#include "bound/grid.hpp"
#include "bound/policy.hpp"

namespace bnd
{
  template <boundable L, boundable R = L>
  struct division
  {
    using result = bound<{(Interval<L> / Interval<R>).value(), 0}>;
    static_assert(std::is_same_v<typename result::raw_type, rational>);

    template <policy_flag F = none>
    static constexpr slim::optional<result> div(L, R, policy<F> = {});
  };

  //---------------------------------------------------------------------------
  // div
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<policy_flag F>
  constexpr auto division<L,R>::div(L lhs, R rhs, policy<F>) -> slim::optional<result>
  {
    auto q = static_cast<rational>(lhs) / static_cast<rational>(rhs);
    if (!q) return slim::nullopt;
    result res; res.Raw = *q; return res;
  }
} // namespace bnd

#endif // BNDdivisionHPP
