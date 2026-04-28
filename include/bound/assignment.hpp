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
    private:
      template<typename A>
      static constexpr void apply_clamp(L& lhs, R rhs, imax lower, imax upper, A&& action);

      template<typename A>
      static constexpr void apply_wrap(L& lhs, R rhs, imax lower, imax upper, A&& action);

      template<typename P, typename A>
      static constexpr bool handle_out_of_range(L& lhs, R rhs, imax lower, imax upper,
                                                P&& policy, A&& action);

      static constexpr void store(L& lhs, R rhs);

    public:
      template<typename P, typename A = no_action>
      static constexpr L& assign(L& lhs, R const& rhs, P&&, A&& action = {});
  };

  template <boundable L, typename R>
    requires std::floating_point<R> || std::same_as<rational, R>
  struct assignment<L,R>
  {
    private:
      template<typename P, typename A>
      static constexpr void apply_clamp(L& lhs, R rhs, P&& policy, A&& action);

      template<typename P>
      static constexpr bool store_checked(L& lhs, R rhs, P&& policy);

    public:
      template<typename P, typename A = no_action>
      static constexpr L& assign(L& lhs, R const& rhs, P&&, A&& action = {});
  };

  template <boundable L, boundable R>
  struct assignment<L,R>
  {
    private:
      static constexpr rational calcOffset()
      {
        if constexpr (is_raw_rational<L>)
          return Lower<R>;
        else if constexpr (is_raw_rational<R>)
          return -(Lower<L>/Notch<L>).value();
        else
          return ((Lower<R> - Lower<L>)/Notch<L>).value();
      }

      static constexpr rational calcFactor()
      {
        if constexpr (is_raw_rational<L>)
          return Notch<R>;
        else if constexpr (is_raw_rational<R>)
          return (1_r/Notch<L>).value();
        else
          return (Notch<R>/Notch<L>).value();
      }

    public:
      static constexpr rational Offset = calcOffset();
      static constexpr rational Factor = calcFactor();

      // Raw-space mapping is integer-only (no rational arithmetic needed)
      static constexpr bool is_integer_mapping =
          not is_raw_rational<L> && not is_raw_rational<R>
          && abs_den(Factor.Denominator) == 1 && abs_den(Offset.Denominator) == 1;

      // Map rhs.Raw into L's raw space (requires is_integer_mapping)
      static constexpr imax map_raw(auto rhs_raw)
      {
        if constexpr (Offset == 0_r)
          return static_cast<imax>(Factor.Numerator) * static_cast<imax>(rhs_raw);
        else if constexpr (Offset.Denominator > 0)
          return static_cast<imax>(Offset.Numerator)
               + static_cast<imax>(Factor.Numerator) * static_cast<imax>(rhs_raw);
        else
          return static_cast<imax>(Factor.Numerator) * static_cast<imax>(rhs_raw)
               - static_cast<imax>(Offset.Numerator);
      }

    private:
      template<typename A>
      static constexpr void apply_clamp(L& lhs, R const& rhs, A&& action);

      template<typename P, typename A>
      static constexpr bool try_clamp_or_fail(L& lhs, R const& rhs, P&& policy, A&& action);

      static constexpr void store(L& lhs, R const& rhs);

    public:
      template<typename P, typename A = no_action>
      static constexpr L& assign(L& lhs, R const& rhs, P&&, A&& action = {});
  };

  //---------------------------------------------------------------------------
  // assign(boundable, integral) — helpers
  //---------------------------------------------------------------------------
  template<boundable L, std::integral R>
  template<typename A>
  constexpr void assignment<L,R>::apply_clamp(L& lhs, R rhs, imax lower, imax upper, A&& action)
  {
    imax clamped = static_cast<imax>(rhs) < lower ? lower : upper;
    if constexpr (!std::is_same_v<plain<A>, no_action>)
      action(static_cast<imax>(rhs) - clamped);
    from_value(lhs, clamped);
  }

  template<boundable L, std::integral R>
  template<typename A>
  constexpr void assignment<L,R>::apply_wrap(L& lhs, R rhs, imax lower, imax upper, A&& action)
  {
    imax range = upper - lower + 1;
    imax shifted = static_cast<imax>(rhs) - lower;
    imax wrapped = ((shifted % range) + range) % range;
    imax excess = (shifted < 0) ? ((shifted - range + 1) / range) : (shifted / range);
    if constexpr (!std::is_same_v<plain<A>, no_action>)
      action(excess);
    from_value(lhs, wrapped + lower);
  }

  template<boundable L, std::integral R>
  template<typename P, typename A>
  constexpr bool assignment<L,R>::handle_out_of_range(L& lhs, R rhs, imax lower, imax upper,
                                                     P&& policy, A&& action)
  {
    if constexpr (has_policy<L, P, clamp>)
    { apply_clamp(lhs, rhs, lower, upper, action); return true; }
    else if constexpr (has_policy<L, P, wrap>)
    { apply_wrap(lhs, rhs, lower, upper, action); return true; }
    else
      return domain_fail(lhs, policy,
        bnd::to_string(rhs) + " is not in " + bnd::to_string(Interval<L>));
  }

  template<boundable L, std::integral R>
  constexpr void assignment<L,R>::store(L& lhs, R rhs)
  {
    if constexpr (Lower<L> == Upper<L>)
      lhs.Raw = 0;
    else if constexpr (is_direct_storage<L>)
      lhs.Raw = raw_cast<L>(rhs);
    else // is_notch_storage
    {
      rational raw = ((rhs - Interval<L>.Lower)/Notch<L>).value();
      lhs.Raw = raw_cast<L>(raw.Numerator / static_cast<umax>(raw.Denominator));
    }
  }

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
      if (not Interval<L>.includes(rhs))
        throw "value not in interval";
    }
    else
    {
      if constexpr (not Interval<L>.includes(Interval<R>))
      {
        if constexpr (is_integer_interval<L>)
        {
          constexpr imax lower = (Lower<L>.Denominator > 0)
            ? static_cast<imax>(Lower<L>.Numerator)
            : -static_cast<imax>(Lower<L>.Numerator);
          constexpr imax upper = (Upper<L>.Denominator > 0)
            ? static_cast<imax>(Upper<L>.Numerator)
            : -static_cast<imax>(Upper<L>.Numerator);
          if (static_cast<imax>(rhs) < lower || static_cast<imax>(rhs) > upper)
            if (handle_out_of_range(lhs, rhs, lower, upper, policy, action)) return lhs;
        }
        else if (not Interval<L>.includes(rhs))
          if (handle_out_of_range(lhs, rhs,
                static_cast<imax>(Lower<L>.Numerator),
                static_cast<imax>(Upper<L>.Numerator), policy, action)) return lhs;
      }
    }

    store(lhs, rhs);
    return lhs;
  }

  //---------------------------------------------------------------------------
  // assign(boundable, floating_point | rational) — helpers
  //---------------------------------------------------------------------------
  template <boundable L, typename R>
    requires std::floating_point<R> || std::same_as<rational, R>
  template<typename P, typename A>
  constexpr void assignment<L,R>::apply_clamp(L& lhs, R rhs, P&&, A&& action)
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
      umax den = static_cast<umax>(raw.Denominator);
      if constexpr (has_policy<L, P, round_nearest>)
        lhs.Raw = raw_cast<L>((raw.Numerator + den/2) / den);
      else
        lhs.Raw = raw_cast<L>(raw.Numerator / den);
    }
  }

  template <boundable L, typename R>
    requires std::floating_point<R> || std::same_as<rational, R>
  template<typename P>
  constexpr bool assignment<L,R>::store_checked(L& lhs, R rhs, P&& policy)
  {
    if constexpr (Lower<L> == Upper<L>)
    { lhs.Raw = 0; return true; }
    else if constexpr (is_raw_rational<L>)
    { lhs.Raw = rhs; return true; }
    else
    {
      rational raw = ((rhs - Lower<L>)/Notch<L>).value();
      umax den = static_cast<umax>(raw.Denominator);
      if (den == 1)
      { lhs.Raw = raw_cast<L>(raw.Numerator); return true; }

      if constexpr (has_policy<L, P, round_nearest>)
        lhs.Raw = raw_cast<L>((raw.Numerator + den/2) / den);
      else if constexpr (has_policy<L, P, ignore_round>)
        lhs.Raw = raw_cast<L>(raw.Numerator / den);
      else if (policy.round_check())
      {
        policy.report(errc::rounding_error,
          bnd::to_string(rhs) + " does not land on notch " + bnd::to_string(Notch<L>));
        return false;
      }
      else
        lhs.Raw = raw_cast<L>(raw.Numerator / den);
      return true;
    }
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
      if (not Interval<L>.includes(rhs))
        throw "value not in interval";
    }
    else
    {
      if (not Interval<L>.includes(rhs))
      {
        if constexpr (has_policy<L, P, clamp>)
        { apply_clamp(lhs, rhs, policy, action); return lhs; }
        else if (domain_fail(lhs, policy,
                   bnd::to_string(rhs) + " is not in " + bnd::to_string(Interval<L>)))
          return lhs;
      }
    }

    store_checked(lhs, rhs, policy);
    return lhs;
  }

  //---------------------------------------------------------------------------
  // assign(boundable, boundable) — helpers
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<typename A>
  constexpr void assignment<L,R>::apply_clamp(L& lhs, R const& rhs, A&& action)
  {
    lhs.Raw = (static_cast<rational>(rhs) < Lower<L>)
      ? raw_cast<L>(RawLo<L>) : raw_cast<L>(RawHi<L>);
    if constexpr (!std::is_same_v<plain<A>, no_action>)
      action(static_cast<rational>(rhs) - static_cast<rational>(lhs));
  }

  template<boundable L, boundable R>
  template<typename P, typename A>
  constexpr bool assignment<L,R>::try_clamp_or_fail(L& lhs, R const& rhs, P&& policy, A&& action)
  {
    if constexpr (has_policy<L, P, clamp>)
    { apply_clamp(lhs, rhs, action); return true; }
    return domain_fail(lhs, policy,
      bnd::to_string(static_cast<rational>(rhs)) + " is not in " + bnd::to_string(Interval<L>));
  }

  template<boundable L, boundable R>
  constexpr void assignment<L,R>::store(L& lhs, R const& rhs)
  {
    if constexpr (is_integer_mapping)
    {
      if constexpr (Offset == 0_r && Factor == 1_r)
        lhs.Raw = raw_cast<L>(rhs.Raw);
      else
        lhs.Raw = raw_cast<L>(map_raw(rhs.Raw));
    }
    else
      lhs.Raw = raw_cast<L>(*(Offset + *(Factor * rhs.Raw)));
  }

  //---------------------------------------------------------------------------
  // assign(boundable, boundable)
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<typename P, typename A>
  constexpr L& assignment<L,R>::assign(L& lhs, R const& rhs, P&& policy, A&& action)
  {
    static_assert(not Interval<L>.excludes(Interval<R>));
    static_assert(abs_den(Factor.Denominator) == 1 || has_policy<L, P, ignore_round>,
      "incompatible notches: use with_round() or policy<ignore_round>() to allow rounding");

    if consteval
    {
      if (not Interval<L>.includes(Interval<R>))
        throw "value not in interval";
    }
    else
    {
      if constexpr (not Interval<L>.includes(Interval<R>))
      {
        if constexpr (is_integer_mapping)
        {
          if (imax mapped = map_raw(rhs.Raw); mapped < RawLo<L> || mapped > RawHi<L>)
            if (try_clamp_or_fail(lhs, rhs, policy, action)) return lhs;
        }
        else if (not Interval<L>.includes(static_cast<rational>(rhs)))
          if (try_clamp_or_fail(lhs, rhs, policy, action)) return lhs;
      }
    }

    store(lhs, rhs);
    return lhs;
  }
} // namespace bnd

#endif // BNDassignmentHPP
