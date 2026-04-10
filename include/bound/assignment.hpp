//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDassignmentHPP
#define BNDassignmentHPP

#include "bound/grid.hpp"
#include "bound/print.hpp"

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
        if constexpr (is_raw_rational<L>)
        { return get_lower(R{}); }
        if constexpr (is_raw_rational<R>)
        { return - get_lower(L{})/get_notch(L{}); }

        return ((Lower<R> - Lower<L>)/Notch<L>).value();
      }

      static constexpr rational calcFactor()
      {
        if constexpr (std::is_same_v<L,R>)
        { return 0; }
        if constexpr (is_raw_rational<L>)
        { return get_notch(R{}); }
        if constexpr (is_raw_rational<R>)
        { return 1_r/get_notch(L{}); }

        return (Notch<R>/Notch<L>).value();
      }

    public:
      static constexpr rational Offset = calcOffset();
      static constexpr rational Factor = calcFactor();

      template<typename P>
      static constexpr L& assign(L& lhs, R const& rhs, P&&);
  };

  //---------------------------------------------------------------------------
  // assign(boundable, integral)
  //---------------------------------------------------------------------------
  template<boundable L, std::integral R>
  template<typename P>
  constexpr L& assignment<L,R>::assign(L& lhs, R const& rhs, P&& policy)
  {
    static_assert(not Interval<L>.excludes(Interval<R>));

    if consteval
    {
      // compile_time => always_check value
      if (not Interval<L>.includes(rhs))
        throw "value not in interval";
    }
    else
    {
      // run_time => compile_time_check (always_success or may_fail)
      if constexpr (not Interval<L>.includes(Interval<R>))
      {
        // may_fail
        if (policy.domain_check() && not Interval<L>.includes(rhs))
        {
          // failed
          policy.domain_error(bnd::to_string(rhs) + " is not in " + bnd::to_string(Interval<L>));
          return lhs;
        }
        // else success
      }
      // else always_success
    }

    if constexpr (Lower<L> == Upper<L>)
      lhs.Raw = 0;
    else if constexpr (is_raw_rational<L>)
      lhs.Raw = rhs;
    else
    {
      rational raw = ((rhs - Interval<L>.Lower)/Notch<L>).value();
      lhs.Raw = raw_cast<L>(raw.Numerator / raw.Denominator);
    }
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
      if (not Interval<L>.includes(rhs))
        throw "value not in interval";
    }
    else
    {
      // run_time => may_fail
      if (policy.domain_check() && not Interval<L>.includes(rhs))
      {
        // failed
        policy.domain_error(bnd::to_string(rhs) + " is not in " + bnd::to_string(Interval<L>));
        return lhs;
      }
      // else success
    }

    if constexpr (Lower<L> == Upper<L>)
      lhs.Raw = 0;
    else if constexpr (is_raw_rational<L>)
      lhs.Raw = rhs;
    else
    {
      rational raw = ((rhs - Lower<L>)/Notch<L>).value();
      lhs.Raw = raw_cast<L>(raw.Numerator / raw.Denominator);
    }
    return lhs;
  }

  //---------------------------------------------------------------------------
  // assign(boundable, boundable)
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<typename P>
  constexpr L& assignment<L,R>::assign(L& lhs, R const& rhs, P&& policy)
  {
    static_assert(not Interval<L>.excludes(Interval<R>));

    if consteval
    {
      // compile_time => always_check value
      if (not Interval<L>.includes(Interval<R>))
        throw "value not in interval";
    }
    else
    {
      // run_time => compile_time_check (always_success or may_fail)
      if constexpr (not Interval<L>.includes(Interval<R>))
      {
        // may_fail
        if (policy.domain_check() && not Interval<L>.includes(static_cast<rational>(rhs)))
        {
          // failed
          policy.domain_error(bnd::to_string(static_cast<rational>(rhs)) + " is not in " + bnd::to_string(Interval<L>));
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

    lhs.Raw = raw_cast<L>(Offset + Factor * rhs.Raw);
    return lhs;
  }
} // namespace bnd

#endif // BNDassignmentHPP
