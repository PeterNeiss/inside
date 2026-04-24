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
      imax lhs_val, rhs_val;
      if constexpr (is_direct_storage<L>)
        lhs_val = static_cast<imax>(lhs.Raw);
      else
        lhs_val = static_cast<imax>(lhs.Raw) * static_cast<imax>(Notch<L>) + static_cast<imax>(Lower<L>);

      if constexpr (is_direct_storage<R>)
        rhs_val = static_cast<imax>(rhs.Raw);
      else
        rhs_val = static_cast<imax>(rhs.Raw) * static_cast<imax>(Notch<R>) + static_cast<imax>(Lower<R>);

      if (rhs_val == 0) return slim::nullopt;
      result res;
      if constexpr (is_direct_storage<result>)
        res.Raw = raw_cast<result>(lhs_val / rhs_val);
      else
        res.Raw = raw_cast<result>(lhs_val / rhs_val - static_cast<imax>(Lower<result>));
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
