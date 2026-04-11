//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDadditionHPP
#define BNDadditionHPP

#include "bound/common.hpp"
#include "bound/grid.hpp"
#include "bound/policy.hpp"

namespace bnd
{
  template <boundable L, boundable R = L>
  struct addition
  {
    using result = bound<(Grid<L> + Grid<R>).value()>;

    static constexpr umax lhs_widen = (Notch<result> / Notch<L>).value_or(1_r).Numerator;
    static constexpr umax rhs_widen = (Notch<result> / Notch<R>).value_or(1_r).Numerator;

    template <policy_flag F = none>
    static constexpr result add(L, R, policy<F> = {});
  };

  //---------------------------------------------------------------------------
  // add
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<policy_flag F>
  constexpr auto addition<L,R>::add(L lhs, R rhs, policy<F>) -> result
  {
    result res;
    res.Raw = slim::nullopt;

    if (!lhs.Raw.has_value() || !rhs.Raw.has_value())
      return res;

    if constexpr (is_raw_rational<result>)
      res.Raw = raw_cast<result>(static_cast<rational>(lhs) + static_cast<rational>(rhs));
    else
    {
      // TODO Check argument
      // Because result type calculation did not overflow at compile time,
      // both widen multiplications dont overflow and
      // their sum does not overflow
      res.Raw = raw_cast<result>(*(lhs.Raw * lhs_widen) + *(rhs.Raw * rhs_widen));
    }

    return res;
  }
} // namespace bnd

#endif // BNDadditionHPP
