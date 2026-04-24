//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDassignmentHPP
#define BNDassignmentHPP

#include "bound/grid.hpp"
#include "bound/format.hpp"

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
    template<typename P, typename A = no_action>
    static constexpr L& assign(L& lhs, R const& rhs, P&&, A&& action = {});
  };

  template <boundable L, typename R>
    requires std::floating_point<R> || std::same_as<rational, R>
  struct assignment<L,R>
  {
    template<typename P, typename A = no_action>
    static constexpr L& assign(L& lhs, R const& rhs, P&&, A&& action = {});
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

      template<typename P, typename A = no_action>
      static constexpr L& assign(L& lhs, R const& rhs, P&&, A&& action = {});
  };

  //---------------------------------------------------------------------------
  // assign(boundable, integral)
  //---------------------------------------------------------------------------
  template<boundable L, std::integral R>
  template<typename P, typename A>
  constexpr L& assignment<L,R>::assign(L& lhs, R const& rhs, P&& policy, A&& action)
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
        if constexpr (abs_den(Lower<L>.Denominator) == 1 && abs_den(Upper<L>.Denominator) == 1)
        {
          // integer interval: compare without rational construction
          constexpr imax lower = (Lower<L>.Denominator > 0)
            ? static_cast<imax>(Lower<L>.Numerator)
            : -static_cast<imax>(Lower<L>.Numerator);
          constexpr imax upper = (Upper<L>.Denominator > 0)
            ? static_cast<imax>(Upper<L>.Numerator)
            : -static_cast<imax>(Upper<L>.Numerator);
          if (static_cast<imax>(rhs) < lower || static_cast<imax>(rhs) > upper)
          {
            if constexpr ((BoundPolicy<L> & clamp) || plain<P>::test(clamp))
            {
              imax clamped = static_cast<imax>(rhs) < lower ? lower : upper;
              if constexpr (!std::is_same_v<plain<A>, no_action>)
                action(static_cast<imax>(rhs) - clamped);

              if constexpr (is_direct_storage<L>)
                lhs.Raw = raw_cast<L>(clamped - lower);
              else
              {
                rational raw = ((clamped - Interval<L>.Lower)/Notch<L>).value();
                lhs.Raw = raw_cast<L>(raw.Numerator / static_cast<umax>(raw.Denominator));
              }
              return lhs;
            }
            else if constexpr ((BoundPolicy<L> & wrap) || plain<P>::test(wrap))
            {
              constexpr imax range = upper - lower + 1;
              imax shifted = static_cast<imax>(rhs) - lower;
              imax wrapped = ((shifted % range) + range) % range;
              imax excess = (shifted < 0) ? ((shifted - range + 1) / range) : (shifted / range);
              if constexpr (!std::is_same_v<plain<A>, no_action>)
                action(excess);

              if constexpr (is_direct_storage<L>)
                lhs.Raw = raw_cast<L>(wrapped + lower);
              else
              {
                rational raw = (((wrapped + lower) - Interval<L>.Lower)/Notch<L>).value();
                lhs.Raw = raw_cast<L>(raw.Numerator / static_cast<umax>(raw.Denominator));
              }
              return lhs;
            }
            else if (policy.domain_check())
            {
              policy.domain_error(bnd::to_string(rhs) + " is not in " + bnd::to_string(Interval<L>));
              return lhs;
            }
          }
        }
        else
        {
          if (not Interval<L>.includes(rhs))
          {
            if constexpr ((BoundPolicy<L> & clamp) || plain<P>::test(clamp))
            {
              imax clamped = (rhs < Lower<L>) ?
                static_cast<imax>(Lower<L>.Numerator) : static_cast<imax>(Upper<L>.Numerator);
              if constexpr (!std::is_same_v<plain<A>, no_action>)
                action(static_cast<imax>(rhs) - clamped);

              rational raw = ((clamped - Interval<L>.Lower)/Notch<L>).value();
              lhs.Raw = raw_cast<L>(raw.Numerator / static_cast<umax>(raw.Denominator));
              return lhs;
            }
            else if (policy.domain_check())
            {
              policy.domain_error(bnd::to_string(rhs) + " is not in " + bnd::to_string(Interval<L>));
              return lhs;
            }
          }
        }
        // else success
      }
      // else always_success
    }

    if constexpr (Lower<L> == Upper<L>)
      lhs.Raw = 0;
    else if constexpr (is_raw_rational<L>)
      lhs.Raw = rhs;
    else if constexpr (is_direct_storage<L>)
      lhs.Raw = raw_cast<L>(rhs);
    else
    {
      rational raw = ((rhs - Interval<L>.Lower)/Notch<L>).value();
      lhs.Raw = raw_cast<L>(raw.Numerator / static_cast<umax>(raw.Denominator));
    }
    return lhs;
  }

  //---------------------------------------------------------------------------
  // assign(boundable, floating_point | rational)
  //---------------------------------------------------------------------------
  template <boundable L, typename R>
    requires std::floating_point<R> || std::same_as<rational, R>
  template<typename P, typename A>
  constexpr L& assignment<L,R>::assign(L& lhs, R const& rhs, P&& policy, A&& action)
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
      if (not Interval<L>.includes(rhs))
      {
        if constexpr ((BoundPolicy<L> & clamp) || plain<P>::test(clamp))
        {
          R clamped = (rhs < Lower<L>) ? static_cast<R>(Lower<L>) : static_cast<R>(Upper<L>);
          if constexpr (!std::is_same_v<plain<A>, no_action>)
            action(rhs - clamped);

          if constexpr (Lower<L> == Upper<L>)
            lhs.Raw = 0;
          else if constexpr (is_raw_rational<L>)
            lhs.Raw = clamped;
          else
          {
            rational raw = ((clamped - Lower<L>)/Notch<L>).value();
            lhs.Raw = raw_cast<L>(raw.Numerator / static_cast<umax>(raw.Denominator));
          }
          return lhs;
        }
        else if (policy.domain_check())
        {
          policy.domain_error(bnd::to_string(rhs) + " is not in " + bnd::to_string(Interval<L>));
          return lhs;
        }
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
      lhs.Raw = raw_cast<L>(raw.Numerator / static_cast<umax>(raw.Denominator));
    }
    return lhs;
  }

  //---------------------------------------------------------------------------
  // assign(boundable, boundable)
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<typename P, typename A>
  constexpr L& assignment<L,R>::assign(L& lhs, R const& rhs, P&& policy, A&& action)
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
        if constexpr (not is_raw_rational<L> && not is_raw_rational<R>
                      && abs_den(Factor.Denominator) == 1 && abs_den(Offset.Denominator) == 1)
        {
          // integer domain check in raw space
          imax mapped;
          if constexpr (Offset == 0_r)
            mapped = static_cast<imax>(Factor.Numerator) * static_cast<imax>(rhs.Raw);
          else if constexpr (Offset.Denominator > 0)
            mapped = static_cast<imax>(Offset.Numerator) + static_cast<imax>(Factor.Numerator) * static_cast<imax>(rhs.Raw);
          else
            mapped = static_cast<imax>(Factor.Numerator) * static_cast<imax>(rhs.Raw) - static_cast<imax>(Offset.Numerator);

          constexpr imax raw_lo = is_direct_storage<L> ? static_cast<imax>(Lower<L>) : 0;
          constexpr imax raw_hi = is_direct_storage<L> ? static_cast<imax>(Upper<L>) : static_cast<imax>(MaxNotch<L>);
          if (mapped < raw_lo || mapped > raw_hi)
          {
            if constexpr ((BoundPolicy<L> & clamp) || plain<P>::test(clamp))
            {
              lhs.Raw = (static_cast<rational>(rhs) < Lower<L>) ?
                raw_cast<L>(raw_lo) : raw_cast<L>(raw_hi);
              if constexpr (!std::is_same_v<plain<A>, no_action>)
                action(static_cast<rational>(rhs) - static_cast<rational>(lhs));
              return lhs;
            }
            else if (policy.domain_check())
            {
              policy.domain_error(bnd::to_string(static_cast<rational>(rhs)) + " is not in " + bnd::to_string(Interval<L>));
              return lhs;
            }
          }
          // round check: abs_den(Factor.Denominator) == 1 so no rounding possible
        }
        else
        {
          // may_fail
          if (not Interval<L>.includes(static_cast<rational>(rhs)))
          {
            if constexpr ((BoundPolicy<L> & clamp) || plain<P>::test(clamp))
            {
              constexpr auto raw_lo = is_direct_storage<L> ? raw_cast<L>(static_cast<imax>(Lower<L>)) : raw_cast<L>(0);
              constexpr auto raw_hi = is_direct_storage<L> ? raw_cast<L>(static_cast<imax>(Upper<L>)) : raw_cast<L>(MaxNotch<L>);
              lhs.Raw = (static_cast<rational>(rhs) < Lower<L>) ? raw_lo : raw_hi;
              if constexpr (!std::is_same_v<plain<A>, no_action>)
                action(static_cast<rational>(rhs) - static_cast<rational>(lhs));
              return lhs;
            }
            else if (policy.domain_check())
            {
              policy.domain_error(bnd::to_string(static_cast<rational>(rhs)) + " is not in " + bnd::to_string(Interval<L>));
              return lhs;
            }
          }

          if (policy.round_check() && abs_den(Factor.Denominator) != 1)
          {
            // failed
            policy.round_error(bnd::to_string(static_cast<rational>(rhs)) + " would round");
            return lhs;
          }
        }
        // else success
      }
      // else always_success
    }

    if constexpr (not is_raw_rational<L> && not is_raw_rational<R>
                  && abs_den(Factor.Denominator) == 1 && abs_den(Offset.Denominator) == 1)
    {
      if constexpr (Offset == 0_r && Factor == 1_r)
        lhs.Raw = raw_cast<L>(rhs.Raw);
      else if constexpr (Offset == 0_r)
        lhs.Raw = raw_cast<L>(static_cast<imax>(Factor.Numerator) * static_cast<imax>(rhs.Raw));
      else if constexpr (Offset.Denominator > 0)
        lhs.Raw = raw_cast<L>(static_cast<imax>(Offset.Numerator) + static_cast<imax>(Factor.Numerator) * static_cast<imax>(rhs.Raw));
      else
        lhs.Raw = raw_cast<L>(static_cast<imax>(Factor.Numerator) * static_cast<imax>(rhs.Raw) - static_cast<imax>(Offset.Numerator));
    }
    else
      lhs.Raw = raw_cast<L>(*(Offset + *(Factor * rhs.Raw)));

    return lhs;
  }
} // namespace bnd

#endif // BNDassignmentHPP
