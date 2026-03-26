//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDassignmentHPP
#define BNDassignmentHPP

#include "bound/utility/grid.hpp"
#include "bound/utility/print.hpp"
#include "bound/waiver.hpp"

#include <system_error>

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
    template<waiver_flag F = none>
    static constexpr L& assign(L& lhs, R const& rhs, waiver_type<F> = {});
  };

  template <boundable L, typename R>
    requires std::floating_point<R> || std::same_as<rational, R>
  struct assignment<L,R>
  {
    template<waiver_flag F = none>
    static constexpr L& assign(L& lhs, R const& rhs, waiver_type<F> = {});
  };

  template <boundable L, boundable R>
  struct assignment<L,R>
  {
    template<waiver_flag F = none>
    static constexpr L& assign(L& lhs, R const& rhs, waiver_type<F> = {});
  };

  template <arithmetic L, boundable R>
  struct assignment<L,R>
  {
    template<waiver_flag F = none>
    static constexpr L& assign(L& lhs, R const& rhs, waiver_type<F> = {});
  };

  //---------------------------------------------------------------------------
  // assign(boundable, integral) 
  //---------------------------------------------------------------------------
  template<boundable L, std::integral R>
  template<waiver_flag F>
  constexpr L& assignment<L,R>::assign(L& lhs, R const& rhs, waiver_type<F> waiver)
  { 
    constexpr interval rhs_interval = 
      {std::numeric_limits<R>::lowest(), std::numeric_limits<R>::max()};

    if constexpr (L::Interval.excludes(rhs_interval))
    {
      throw "Can never construct";
    }
    else if constexpr (not L::Interval.includes(rhs_interval) && waiver.domain_check())
    {
      // check runtime rhs
      if (not L::Interval.includes(rhs))
        throw std::system_error
        (
          EDOM, std::generic_category(), 
          std::to_string(rhs) + " is not in " + bnd::to_string(L::Interval)
        );
    }

    lhs.Raw = L::Grid.template to_raw<typename L::raw_type>(rhs); 
    return lhs;
  }

  //---------------------------------------------------------------------------
  // assign(boundable, floating_point | rational) 
  //---------------------------------------------------------------------------
  template <boundable L, typename R>
    requires std::floating_point<R> || std::same_as<rational, R>
  template<waiver_flag F>
  constexpr L& assignment<L,R>::assign(L& lhs, R const& rhs, waiver_type<F> waiver)
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
  template<waiver_flag F>
  constexpr L& assignment<L,R>::assign(L& lhs, R const& rhs, waiver_type<F>)
  { 
    lhs = rhs; 
    return lhs;
  }
} // namespace bnd

#endif // BNDassignmentHPP
