//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDadditionHPP
#define BNDadditionHPP

#include "bound/common.hpp"
#include "bound/utility/grid.hpp"
#include "bound/policy.hpp"

namespace bnd
{
  template <boundable L, boundable R = L>
  struct addition
  {
    using result = bound<get_grid(L{}) + get_grid(R{})>;

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
    if constexpr (std::is_same_v<raw_t<result>, rational>)
      res.Raw = raw_cast<result>(lhs.to_rational() + rhs.to_rational());
    else
    {
      static constexpr umax lhs_widen = (get_notch(L{}) == 0) ? 0 : (get_notch(result{}) / get_notch(L{})).Numerator;
      static constexpr umax rhs_widen = (get_notch(R{}) == 0) ? 0 : (get_notch(result{}) / get_notch(R{})).Numerator;

      // TODO Check argument
      // Because result type calculation did not overflow at compile time,
      // both widen multiplications dont overflow and
      // their sum does not overflow
      res.Raw = raw_cast<result>(lhs.Raw * lhs_widen + rhs.Raw * rhs_widen);
    }

    return res;
  }
} // namespace bnd

#endif // BNDadditionHPP
