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
        is_raw_rational<result>
        && ((F | BoundPolicy<L> | BoundPolicy<R>) & checked);

    template <policy_flag F = none>
    using return_type_for = std::conditional_t<needs_overflow_check<F>,
                                               slim::optional<result>,
                                               result>;

    static constexpr imax lhs_widen = static_cast<imax>((Notch<result> / Notch<L>).value_or(1_r).Numerator);
    static constexpr imax rhs_widen = static_cast<imax>((Notch<result> / Notch<R>).value_or(1_r).Numerator);

    template <policy_flag F = none>
    static constexpr return_type_for<F> add(L, R, policy<F> = {});
  };

  //---------------------------------------------------------------------------
  // add
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<policy_flag F>
  constexpr auto addition<L,R>::add(L lhs, R rhs, policy<F>) -> return_type_for<F>
  {
    if constexpr (is_raw_rational<result>)
    {
      if constexpr (needs_overflow_check<F>)
      {
        auto sum = static_cast<rational>(lhs) + static_cast<rational>(rhs);
        if (!sum) return slim::nullopt;
        result res; res.Raw = raw_cast<result>(*sum); return res;
      }
      else
      {
        result res;
        res.Raw = raw_cast<result>(rational::add_unchecked(
            static_cast<rational>(lhs), static_cast<rational>(rhs)));
        return res;
      }
    }
    else if constexpr (is_raw_rational<L> || is_raw_rational<R>)
    {
      auto sum = static_cast<rational>(lhs) + static_cast<rational>(rhs);
      result res;
      res.Raw = raw_cast<result>(((*sum - Lower<result>) / Notch<result>).value().Numerator);
      return res;
    }
    else if constexpr (is_direct_storage<L> || is_direct_storage<R> || is_direct_storage<result>)
    {
      result res;
      from_value(res, to_value(lhs) + to_value(rhs));
      return res;
    }
    else
    {
      result res;
      res.Raw = raw_cast<result>(static_cast<imax>(lhs.Raw) * lhs_widen + static_cast<imax>(rhs.Raw) * rhs_widen);
      return res;
    }
  }
} // namespace bnd

#endif // BNDadditionHPP
