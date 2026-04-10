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
/*  template <typename L, typename R = L>
  struct lazy_addition
  {
    enum result_state { unknown, L_empty, R_empty, result_underflow, result_overflow, result_exists, result_cached};
    using result = bound<Grid<L::value_type> + Grid<R::value_type>>;

    L Lhs; // no unique address
    R Rhs; // no unique address
    result Result; // no unique address
    result_state State{result_state::unknown};

    constexpr bool has_value() { return true; }
    sentinel() { return slim::sentinal_trait<result>::sentinel(); }
    is_sentinel(){ return slim::sentinal_trait<result>::sentinel(); }

    //requires invokable f(lazy_addition)
    or_else(F){ return F(*this);}


    static constexpr slim::optional<result, lazy_addition<L,R>> add(L rhs, R rhs) { return {lhs, rhs}; }
  };
*/
  template <boundable L, boundable R = L>
  struct addition
  {
    using result = bound<Grid<L> + Grid<R>>;

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
    if constexpr (std::is_same_v<typename raw_t<result>::value_type, rational>)
      res.Raw = raw_cast<result>(lhs.to_rational() + rhs.to_rational());
    else
    {

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
