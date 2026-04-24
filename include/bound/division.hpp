//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDdivisionHPP
#define BNDdivisionHPP

#include "bound/generic.hpp"
#include "bound/grid.hpp"
#include "bound/policy.hpp"

namespace bnd
{
  template <boundable L, boundable R = L, policy_flag F = none>
  struct division
  {
    static constexpr bool native_div =
        ((F | BoundPolicy<L> | BoundPolicy<R>) & ignore_round)
        && !is_raw_rational<L> && !is_raw_rational<R>
        && abs_den(Notch<L>.Denominator) == 1
        && abs_den(Notch<R>.Denominator) == 1
        && abs_den(Lower<L>.Denominator) == 1
        && abs_den(Lower<R>.Denominator) == 1;

    static constexpr grid result_grid = native_div
        ? grid{static_cast<imax>((*(Grid<L> / Grid<R>)).Interval.Lower),
               static_cast<imax>((*(Grid<L> / Grid<R>)).Interval.Upper)}
        : *(Grid<L> / Grid<R>);

    using result = bound<result_grid>;

    template <policy_flag G = F>
    static constexpr slim::optional<result> div(L, R, policy<G> = {});
  };

  //---------------------------------------------------------------------------
  // div
  //---------------------------------------------------------------------------
  template<boundable L, boundable R, policy_flag F>
  template<policy_flag G>
  constexpr auto division<L,R,F>::div(L lhs, R rhs, policy<G>) -> slim::optional<result>
  {
    if constexpr (native_div)
    {
      imax rhs_val = to_value(rhs);
      if (rhs_val == 0) return slim::nullopt;
      result res;
      from_value(res, to_value(lhs) / rhs_val);
      return res;
    }
    else
    {
      auto q = static_cast<rational>(lhs) / static_cast<rational>(rhs);
      if (!q) return slim::nullopt;
      result res; res.Raw = *q; return res;
    }
  }
} // namespace bnd

#endif // BNDdivisionHPP
