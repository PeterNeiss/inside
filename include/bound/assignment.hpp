//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDassignmentHPP
#define BNDassignmentHPP

#include "bound/utility/grid.hpp"
#include "bound/utility/print.hpp"
#include "bound/policy.hpp"

namespace bnd
{
  //---------------------------------------------------------------------------
  // assignment 
  //---------------------------------------------------------------------------
  template <typename L, typename R>
  struct assignment;

  template <boundable L, std::integral R>
  struct assignment<L,R>
  {
    template<typename P>
    static constexpr L& assign(L& lhs, R const& rhs, P&&);
  };

  template <boundable L, typename R>
    requires std::floating_point<R> || std::same_as<rational, R>
  struct assignment<L,R>
  {
    template<policy_flag F = none>
    static constexpr L& assign(L& lhs, R const& rhs, policy<F> = {});
  };

  template <boundable L, boundable R>
  struct assignment<L,R>
  {
    template<policy_flag F = none>
    static constexpr L& assign(L& lhs, R const& rhs, policy<F> = {});
  };

  template <arithmetic L, boundable R>
  struct assignment<L,R>
  {
    template<policy_flag F = none>
    static constexpr L& assign(L& lhs, R const& rhs, policy<F> = {});
  };

  //---------------------------------------------------------------------------
  // assign(boundable, integral) 
  //---------------------------------------------------------------------------
  template<boundable L, std::integral R>
  template<typename P>
  constexpr L& assignment<L,R>::assign(L& lhs, R const& rhs, P&& policy)
  { 
    constexpr interval rhs_interval = 
      {std::numeric_limits<R>::lowest(), std::numeric_limits<R>::max()};

    static_assert(not L::Interval.excludes(rhs_interval));

    if constexpr (not L::Interval.includes(rhs_interval))
    {
      // TODO saturation and wrap
      // check runtime rhs
      if (policy.domain_check() && not L::Interval.includes(rhs))
      {
        policy.domain_error(std::to_string(rhs) + " is not in " + bnd::to_string(L::Interval));
        return lhs;
      }
    }

    lhs.Raw = L::Grid.template to_raw<typename L::raw_type>(rhs); 
    return lhs;
  }

  //---------------------------------------------------------------------------
  // assign(boundable, floating_point | rational) 
  //---------------------------------------------------------------------------
  template <boundable L, typename R>
    requires std::floating_point<R> || std::same_as<rational, R>
  template<policy_flag F>
  constexpr L& assignment<L,R>::assign(L& lhs, R const& rhs, policy<F> waiver)
  { 
    // can never construct: false
    // can always construct: false
    // can maybe construct
    if consteval
    {
      // always check at compile time
      if (not L::Interval.includes(rhs))
        throw "value not in interval";
    }  
    else 
    {
      if (not waiver.test(ignore_domain) && not L::Interval.includes(rhs))
        throw std::system_error
        (
          EDOM, std::generic_category(), 
          std::to_string(rhs) + "is not in interval"
        );
    }

    lhs.Raw =  L::Grid.template to_raw<typename L::raw_type>(rhs); 
    return lhs;
  }

  //---------------------------------------------------------------------------
  // assign(boundable, boundable) 
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<policy_flag F>
  constexpr L& assignment<L,R>::assign(L& lhs, R const& rhs, policy<F>)
  { 
    lhs = rhs; 
    return lhs;
  }
} // namespace bnd

#endif // BNDassignmentHPP
