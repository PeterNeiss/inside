//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDassignmentHPP
#define BNDassignmentHPP

#include "bound/utility/grid.hpp"
#include "bound/utility/print.hpp"

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
    template<typename P>
    static constexpr L& assign(L& lhs, R const& rhs, P&&);
  };

  template <boundable L, boundable R>
  struct assignment<L,R>
  {
    private:
      static constexpr rational calcOffset()
      {
        if constexpr (std::is_same_v<L,R>)
        { return 0; }
        if constexpr (std::is_same_v<typename L::raw_type, rational>)
        { return R::Grid.Interval.Lower; }
        if constexpr (std::is_same_v<typename R::raw_type, rational>)
        { return - L::Grid.Interval.Lower/L::Grid.Notch; }

        return (R::Grid.Interval.Lower - L::Grid.Interval.Lower)/L::Grid.Notch;
      }

      static constexpr rational calcFactor()
      {
        if constexpr (std::is_same_v<L,R>)
        { return 0; }
        if constexpr (std::is_same_v<typename L::raw_type, rational>)
        { return R::Grid.Notch; }
        if constexpr (std::is_same_v<typename R::raw_type, rational>)
        { return 1_r/L::Grid.Notch; }

        return R::Grid.Notch/L::Grid.Notch;
      }

    public:
      static constexpr rational Offset = calcOffset();
      static constexpr rational Factor = calcFactor();

      template<typename P>
      static constexpr L& assign(L& lhs, R const& rhs, P&&);
  };
/*
  template <arithmetic L, boundable R>
  struct assignment<L,R>
  {
    template<policy_flag F = none>
    static constexpr L& assign(L& lhs, R const& rhs, policy<F> = {});
  };
*/
  //---------------------------------------------------------------------------
  // assign(boundable, integral)
  //---------------------------------------------------------------------------
  template<boundable L, std::integral R>
  template<typename P>
  constexpr L& assignment<L,R>::assign(L& lhs, R const& rhs, P&& policy)
  {
    constexpr interval lhs_interval = get_interval(L{});
    constexpr interval rhs_interval =
      {std::numeric_limits<R>::lowest(), std::numeric_limits<R>::max()};

    static_assert(not lhs_interval.excludes(rhs_interval));

    if consteval
    {
      // compile_time => always_check value
      if (not lhs_interval.includes(rhs))
        throw "value not in interval";
    }
    else
    {
      // run_time => compile_time_check (always_success or may_fail)
      if constexpr (not lhs_interval.includes(rhs_interval))
      {
        // may_fail
        if (policy.domain_check() && not lhs_interval.includes(rhs))
        {
          // failed
          policy.domain_error(bnd::to_string(rhs) + " is not in " + bnd::to_string(lhs_interval));
          return lhs;
        }
        // else success
      }
      // else always_success
    }

    //TODO check for rounding
    lhs.Raw = L::Grid.template to_raw<typename L::raw_type>(rhs);
    return lhs;
  }

  //---------------------------------------------------------------------------
  // assign(boundable, floating_point | rational)
  //---------------------------------------------------------------------------
  template <boundable L, typename R>
    requires std::floating_point<R> || std::same_as<rational, R>
  template<typename P>
  constexpr L& assignment<L,R>::assign(L& lhs, R const& rhs, P&& policy)
  {
    if consteval
    {
      // compile_time => always_check value
      if (not L::Grid.Interval.includes(rhs))
        throw "value not in interval";
    }
    else
    {
      // run_time => may_fail
      if (policy.domain_check() && not L::Grid.Interval.includes(rhs))
      {
        // failed
        policy.domain_error(bnd::to_string(rhs) + " is not in " + bnd::to_string(L::Grid.Interval));
        return lhs;
      }
      // else success
    }

    lhs.Raw = L::Grid.template to_raw<typename L::raw_type>(rhs);
    return lhs;
  }

  //---------------------------------------------------------------------------
  // assign(boundable, boundable)
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<typename P>
  constexpr L& assignment<L,R>::assign(L& lhs, R const& rhs, P&& policy)
  {
    static_assert(not L::Grid.Interval.excludes(R::Grid.Interval));

    if consteval
    {
      // compile_time => always_check value
      if (not L::Grid.Interval.includes(R::Grid.Interval))
        throw "value not in interval";
    }
    else
    {
      // run_time => compile_time_check (always_success or may_fail)
      if constexpr (not L::Grid.Interval.includes(R::Grid.Interval))
      {
        // may_fail
        if (policy.domain_check() && not L::Grid.Interval.includes(static_cast<rational>(rhs)))
        {
          // failed
          policy.domain_error(bnd::to_string(static_cast<rational>(rhs)) + " is not in " + bnd::to_string(L::Grid.Interval));
          return lhs;
        }

        if (policy.round_check() && Factor.Denominator != 1)
        {
          // failed
          policy.round_error(bnd::to_string(static_cast<rational>(rhs)) + " would round");
          return lhs;
        }
        // else success
      }
      // else always_success
    }

    lhs.Raw = static_cast<L::raw_type>(Offset + Factor * rhs.Raw);
    return lhs;
  }
} // namespace bnd

#endif // BNDassignmentHPP
