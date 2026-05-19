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
  // assignment — narrowing/coercion between bounded and arithmetic types.
  //
  // Three specialisations dispatch on the source type:
  //   assignment<L, integral R>           — raw integer rhs (fast path)
  //   assignment<L, real R>               — float, double, rational rhs
  //   assignment<L, boundable R>          — bound-to-bound, with raw remap
  //
  // Each specialisation routes through `store` (the in-range case) and
  // `handle_out_of_range` / `apply_clamp` / `apply_wrap` (the policy case).
  // The boundable-to-boundable path also exposes `is_integer_mapping` and
  // `map_raw`, so when both sides have integer raws the conversion is a
  // pure-integer formula — no rational arithmetic in the hot path.
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
    requires real<R>
  struct assignment<L,R>
  {
    private:
      template<typename P, typename A>
      static constexpr void apply_clamp(L& lhs, R rhs, P&& policy, A&& action);

      template<typename P, typename A = no_action>
      static constexpr bool store_checked(L& lhs, R rhs, P&& policy, A&& action = {});

    public:
      template<typename P, typename A = no_action>
      static constexpr L& assign(L& lhs, R const& rhs, P&&, A&& action = {});
  };

  template <boundable L, boundable R>
  struct assignment<L,R>
  {
    private:
      // Offset/Factor map an rhs.Raw to an lhs.Raw via
      //   lhs.Raw = Factor * rhs.Raw + Offset.
      // Three branches cover the three storage shapes:
      //   1. L stores rational  — pass the rhs value through unchanged.
      //   2. R stores rational  — pre-divide by Notch<L> so the formula
      //                           stays in raw space.
      //   3. both integer       — the integer branch (the hot path); the
      //                           formula collapses to integer math in
      //                           `is_integer_mapping` callers below.
      static constexpr rational calcOffset()
      {
        if constexpr (IsRawRational<L>)
          return Lower<R>;
        else if constexpr (IsRawRational<R>)
          return -(Lower<L>/Notch<L>).value();
        else
          return ((Lower<R> - Lower<L>)/Notch<L>).value();
      }

      static constexpr rational calcFactor()
      {
        if constexpr (IsRawRational<L>)
          return Notch<R>;
        else if constexpr (IsRawRational<R>)
          return (1_r/Notch<L>).value();
        else
          return (Notch<R>/Notch<L>).value();
      }

    public:
      static constexpr rational Offset = calcOffset();
      static constexpr rational Factor = calcFactor();

      // Raw-space mapping is integer-only (no rational arithmetic needed)
      static constexpr bool is_integer_mapping =
          not IsRawRational<L> && not IsRawRational<R>
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

      template<typename A>
      static constexpr void apply_wrap(L& lhs, R const& rhs, A&& action);

      template<typename P, typename A>
      static constexpr bool try_clamp_or_fail(L& lhs, R const& rhs, P&& policy, A&& action);

      template<typename P>
      static constexpr void store(L& lhs, R const& rhs, P&& policy);

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
    imax overshoot = static_cast<imax>(rhs) - clamped;
    from_value(lhs, clamped);
    if constexpr (is_clamp_action<plain<A>>)
      action.fn(lhs, overshoot);
    else if constexpr (!std::is_same_v<plain<A>, no_action>)
      action(overshoot);
  }

  template<boundable L, std::integral R>
  template<typename A>
  constexpr void assignment<L,R>::apply_wrap(L& lhs, R rhs, imax lower, imax upper, A&& action)
  {
    imax range = upper - lower + 1;
    imax shifted = static_cast<imax>(rhs) - lower;
    imax wrapped = ((shifted % range) + range) % range;
    imax excess = (shifted < 0) ? ((shifted - range + 1) / range) : (shifted / range);
    from_value(lhs, wrapped + lower);
    if constexpr (is_wrap_action<plain<A>>)
      action.fn(lhs, excess);
    else if constexpr (!std::is_same_v<plain<A>, no_action>)
      action(excess);
  }

  template<boundable L, std::integral R>
  template<typename P, typename A>
  constexpr bool assignment<L,R>::handle_out_of_range(L& lhs, R rhs, imax lower, imax upper,
                                                     P&& policy, A&& action)
  {
    using PA = plain<A>;
    if constexpr (is_clamp_action<PA>)
    { apply_clamp(lhs, rhs, lower, upper, action); return true; }
    else if constexpr (is_wrap_action<PA>)
    { apply_wrap(lhs, rhs, lower, upper, action); return true; }
    else if constexpr (is_sentinel_action<PA>)
    {
      lhs.Raw = sentinel_raw<L>();
      action.fn(lhs, static_cast<imax>(rhs));
      return true;
    }
    else if constexpr (is_error_action<PA>)
    {
      auto msg = bnd::to_string(rhs) + " is not in " + bnd::to_string(Interval<L>);
      action.fn(lhs, errc::domain_error, std::string_view(msg));
      return true;
    }
    else if constexpr (HasPolicy<L, P, clamp>)
    { apply_clamp(lhs, rhs, lower, upper, action); return true; }
    else if constexpr (HasPolicy<L, P, wrap>)
    { apply_wrap(lhs, rhs, lower, upper, action); return true; }
    else
      return domain_fail(lhs, policy,
        bnd::to_string(rhs) + " is not in " + bnd::to_string(Interval<L>));
  }

  template<boundable L, std::integral R>
  constexpr void assignment<L,R>::store(L& lhs, R rhs)
  {
    if constexpr (IsDirectStorage<L>)
      lhs.Raw = raw_cast<L>(rhs);
    else if constexpr (Lower<L> == Upper<L>)
      lhs.Raw = 0;   // notch_storage point grid: 0 is the only offset
    // Integer fast path for the common fixed-point shape: Lower has integer
    // value (den == 1) and Notch has unit numerator (e.g. 1/256, 1/16384).
    // raw = (rhs - lower_int) * notch_denominator — pure integer math, no
    // rational construction.
    else if constexpr (abs_den(Lower<L>.Denominator) == 1
                       && Notch<L>.Numerator == 1)
    {
      constexpr imax lower_int = (Lower<L>.Denominator < 0)
        ? -static_cast<imax>(Lower<L>.Numerator)
        :  static_cast<imax>(Lower<L>.Numerator);
      constexpr imax nd = abs_den(Notch<L>.Denominator);   // Notch.Numerator == 1
      lhs.Raw = raw_cast<L>((static_cast<imax>(rhs) - lower_int) * nd);
    }
    else // IsNotchStorage, generic rational path
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
        if constexpr (IsIntegerInterval<L>)
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
        {
          // Non-integer L bounds: route through the rational path so fractional
          // Lower/Upper drive clamp/sentinel/error correctly.
          return assignment<L, rational>::assign(lhs, rational{rhs}, policy, action);
        }
      }
    }

    store(lhs, rhs);
    return lhs;
  }

  //---------------------------------------------------------------------------
  // assign(boundable, floating_point | rational) — helpers
  //---------------------------------------------------------------------------
  template <boundable L, typename R>
    requires real<R>
  template<typename P, typename A>
  constexpr void assignment<L,R>::apply_clamp(L& lhs, R rhs, P&&, A&& action)
  {
    R clamped = (rhs < Lower<L>) ? static_cast<R>(Lower<L>) : static_cast<R>(Upper<L>);
    R overshoot;
    if constexpr (std::same_as<R, rational>)
      overshoot = (rhs - clamped).value_or(rational{0u});
    else
      overshoot = rhs - clamped;

    if constexpr (IsRawRational<L>)
      lhs.Raw = clamped;
    else if constexpr (Lower<L> == Upper<L>)
      lhs.Raw = 0;
    else
    {
      rational raw = ((clamped - Lower<L>)/Notch<L>).value();
      umax den = static_cast<umax>(raw.Denominator);
      // Clamp happened first (above), then we round the clamped value onto
      // a notch. Order matters: rounding-then-clamping could push a
      // boundary-adjacent midpoint *past* the boundary by one notch.
      if constexpr (HasPolicy<L, P, round_nearest>)
        lhs.Raw = raw_cast<L>((raw.Numerator + den/2) / den);
      else
        lhs.Raw = raw_cast<L>(raw.Numerator / den);
    }

    if constexpr (is_clamp_action<plain<A>>)
      action.fn(lhs, overshoot);
    else if constexpr (!std::is_same_v<plain<A>, no_action>)
      action(overshoot);
  }

  template <boundable L, typename R>
    requires real<R>
  template<typename P, typename A>
  constexpr bool assignment<L,R>::store_checked(L& lhs, R rhs, P&& policy, A&& action)
  {
    if constexpr (IsRawRational<L>)
    { lhs.Raw = rhs; return true; }
    else if constexpr (Lower<L> == Upper<L>)
    { lhs.Raw = 0; return true; }
    else
    {
      rational raw = ((rhs - Lower<L>)/Notch<L>).value();
      umax den = static_cast<umax>(raw.Denominator);
      if (den == 1)
      { lhs.Raw = raw_cast<L>(raw.Numerator); return true; }

      if constexpr (HasPolicy<L, P, round_nearest>)
        lhs.Raw = raw_cast<L>((raw.Numerator + den/2) / den);
      else if constexpr (HasPolicy<L, P, ignore_round>)
        lhs.Raw = raw_cast<L>(raw.Numerator / den);
      else if (policy.round_check())
      {
        auto msg = bnd::to_string(rhs) + " does not land on notch " + bnd::to_string(Notch<L>);
        if constexpr (is_error_action<plain<A>>)
        { action.fn(lhs, errc::rounding_error, std::string_view(msg)); return false; }
        policy.report(errc::rounding_error, msg);
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
    requires real<R>
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
        using PA = plain<A>;
        if constexpr (is_clamp_action<PA>)
        { apply_clamp(lhs, rhs, policy, action); return lhs; }
        else if constexpr (is_sentinel_action<PA>)
        {
          lhs.Raw = sentinel_raw<L>();
          action.fn(lhs, rhs);
          return lhs;
        }
        else if constexpr (is_error_action<PA>)
        {
          auto msg = bnd::to_string(rhs) + " is not in " + bnd::to_string(Interval<L>);
          action.fn(lhs, errc::domain_error, std::string_view(msg));
          return lhs;
        }
        else if constexpr (HasPolicy<L, P, clamp>)
        { apply_clamp(lhs, rhs, policy, action); return lhs; }
        else if (domain_fail(lhs, policy,
                   bnd::to_string(rhs) + " is not in " + bnd::to_string(Interval<L>)))
          return lhs;
      }
    }

    store_checked(lhs, rhs, policy, action);
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
    auto overshoot = static_cast<rational>(rhs) - static_cast<rational>(lhs);
    if constexpr (is_clamp_action<plain<A>>)
      action.fn(lhs, overshoot);
    else if constexpr (!std::is_same_v<plain<A>, no_action>)
      action(overshoot);
  }

  template<boundable L, boundable R>
  template<typename A>
  constexpr void assignment<L,R>::apply_wrap(L& lhs, R const& rhs, A&& action)
  {
    static_assert(IsIntegerInterval<L>,
      "wrap with bound rhs requires an integer-aligned destination interval");
    imax rhs_imax = static_cast<imax>(static_cast<rational>(rhs));
    constexpr imax lower = static_cast<imax>(Lower<L>);
    constexpr imax upper = static_cast<imax>(Upper<L>);
    imax range = upper - lower + 1;
    imax shifted = rhs_imax - lower;
    imax wrapped = ((shifted % range) + range) % range;
    imax excess  = (shifted < 0) ? ((shifted - range + 1) / range) : (shifted / range);
    from_value(lhs, wrapped + lower);
    if constexpr (is_wrap_action<plain<A>>)
      action.fn(lhs, excess);
    else if constexpr (!std::is_same_v<plain<A>, no_action>)
      action(excess);
  }

  template<boundable L, boundable R>
  template<typename P, typename A>
  constexpr bool assignment<L,R>::try_clamp_or_fail(L& lhs, R const& rhs, P&& policy, A&& action)
  {
    using PA = plain<A>;
    if constexpr (is_clamp_action<PA>)
    { apply_clamp(lhs, rhs, action); return true; }
    else if constexpr (is_wrap_action<PA>)
    { apply_wrap(lhs, rhs, action); return true; }
    else if constexpr (is_sentinel_action<PA>)
    {
      lhs.Raw = sentinel_raw<L>();
      action.fn(lhs, static_cast<rational>(rhs));
      return true;
    }
    else if constexpr (is_error_action<PA>)
    {
      auto msg = bnd::to_string(static_cast<rational>(rhs))
               + " is not in " + bnd::to_string(Interval<L>);
      action.fn(lhs, errc::domain_error, std::string_view(msg));
      return true;
    }
    else if constexpr (HasPolicy<L, P, clamp>)
    { apply_clamp(lhs, rhs, action); return true; }
    else if constexpr (HasPolicy<L, P, wrap>)
    { apply_wrap(lhs, rhs, action); return true; }
    return domain_fail(lhs, policy,
      bnd::to_string(static_cast<rational>(rhs)) + " is not in " + bnd::to_string(Interval<L>));
  }

  template<boundable L, boundable R>
  template<typename P>
  constexpr void assignment<L,R>::store(L& lhs, R const& rhs, P&&)
  {
    if constexpr (is_integer_mapping)
    {
      // exact: Factor and Offset have integer denominators, no rounding ambiguity
      if constexpr (Offset == 0_r && Factor == 1_r)
        lhs.Raw = raw_cast<L>(rhs.Raw);
      else
        lhs.Raw = raw_cast<L>(map_raw(rhs.Raw));
    }
    else
    {
      rational rat = *(Offset + *(Factor * rhs.Raw));
      if constexpr (HasPolicy<L, P, round_nearest>)
      {
        // half-away-from-zero: bump magnitude when 2*remainder >= denom
        umax ad = static_cast<umax>(abs_den(rat.Denominator));
        umax q  = (rat.Numerator + ad / 2) / ad;
        lhs.Raw = (rat.Denominator < 0)
          ? raw_cast<L>(-static_cast<imax>(q))
          : raw_cast<L>(q);
      }
      else
        lhs.Raw = raw_cast<L>(rat);
    }
  }

  //---------------------------------------------------------------------------
  // assign(boundable, boundable)
  //---------------------------------------------------------------------------
  template<boundable L, boundable R>
  template<typename P, typename A>
  constexpr L& assignment<L,R>::assign(L& lhs, R const& rhs, P&& policy, A&& action)
  {
    static_assert(not Interval<L>.excludes(Interval<R>));
    static_assert(abs_den(Factor.Denominator) == 1 || HasPolicy<L, P, ignore_round>,
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

    store(lhs, rhs, policy);
    return lhs;
  }
} // namespace bnd

#endif // BNDassignmentHPP
