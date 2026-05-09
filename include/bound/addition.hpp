//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDadditionHPP
#define BNDadditionHPP

#include "bound/generic.hpp"
#include "bound/grid.hpp"
#include "bound/policy.hpp"

namespace bnd
{
  template <boundable L, boundable R = L>
  struct addition
  {
    using result = bound<(Grid<L> + Grid<R>).value()>;

    template <policy_flag F>
    static constexpr bool needs_overflow_check =
        IsRawRational<result>
        && ((F | BoundPolicy<L> | BoundPolicy<R>) & checked);

    template <policy_flag F = none>
    using return_type_for = std::conditional_t<needs_overflow_check<F>,
                                               slim::optional<result>,
                                               result>;

    template <policy_flag F, typename A>
    using add_return_t = std::conditional_t<is_overflow_action<plain<A>>,
                                            result,
                                            return_type_for<F>>;

    static constexpr imax lhs_widen = static_cast<imax>((Notch<result> / Notch<L>).value_or(1_r).Numerator);
    static constexpr imax rhs_widen = static_cast<imax>((Notch<result> / Notch<R>).value_or(1_r).Numerator);

    template <policy_flag F = none, typename E = empty_ref, typename A = no_action>
    static constexpr add_return_t<F, A> add(L, R, policy<F, E> = {}, A&& = {});
  };

  //---------------------------------------------------------------------------
  // add
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<policy_flag F, typename E, typename A>
  constexpr auto addition<L,R>::add(L lhs, R rhs, policy<F, E> policy, A&& action) -> add_return_t<F, A>
  {
    result res;
    if constexpr (IsRawRational<result>)
    {
      if constexpr (needs_overflow_check<F>)
      {
        auto sum = rational::add(lhs,rhs);
        if (!sum)
        {
          if constexpr (is_overflow_action<plain<A>>)
          { action.fn(res, errc::overflow); return res; }
          else
          {
            if constexpr (uses_error_ref_v<bnd::policy<F, E>>)
              policy.report(errc::overflow, "rational overflow in add");
            return slim::nullopt;
          }
        }
        res.Raw = *sum;
      }
      else
        res.Raw = rational::add_unchecked(lhs, rhs);
    }
    else if constexpr (IsRawRational<L> || IsRawRational<R>)
    {
      auto sum = rational::add_unchecked(lhs,rhs);
      res.Raw = raw_cast<result>(((sum - Lower<result>) / Notch<result>).value().Numerator);
    }
    else if constexpr (IsDirectStorage<L> || IsDirectStorage<R> || IsDirectStorage<result>)
    {
      from_value(res, to_value(lhs) + to_value(rhs));
    }
    else
    {
      res.Raw = raw_cast<result>(static_cast<imax>(lhs.Raw) * lhs_widen + static_cast<imax>(rhs.Raw) * rhs_widen);
    }
    return res;
  }
} // namespace bnd

#endif // BNDadditionHPP
