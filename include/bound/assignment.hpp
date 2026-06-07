//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDassignmentHPP
#define BNDassignmentHPP

#include "bound/generic.hpp"
#include "bound/grid.hpp"
#include "bound/format.hpp"

namespace bnd::detail
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
  // needs_runtime_domain_check<L, P, A>
  //
  // True iff any out-of-range handler would fire on an out-of-range value:
  // an action of any kind, a policy bit that triggers clamp/wrap/sentinel,
  // or the default-throw path under `checked` (which is itself off when
  // `ignore_domain` is set). When this is false — typically under `unsafe`
  // with no action — the runtime range branch in `assign` is dead code and
  // can be skipped, which lets the auto-vectorizer kick in on hot loops.
  //---------------------------------------------------------------------------
  template <boundable L, typename P, typename A>
  inline constexpr bool needs_runtime_domain_check =
         clamp_action   <plain<A>>
      || wrap_action    <plain<A>>
      || sentinel_action<plain<A>>
      || error_action   <plain<A>>
      || HasPolicy<L, P, clamp>
      || HasPolicy<L, P, wrap>
      || HasPolicy<L, P, sentinel>
      || (HasPolicy<L, P, checked> && !HasPolicy<L, P, ignore_domain>);

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

    public:
      // Exposed (not private) so the boundable-rhs wrap path can reuse the
      // rational specialization's modular wrap on fractional/notch grids, and
      // so the wrap path can reuse store_checked after computing the wrapped
      // value.
      template<typename P, typename A>
      static constexpr void apply_wrap(L& lhs, R rhs, P&& policy, A&& action);

      template<typename P, typename A = no_action>
      static constexpr bool store_checked(L& lhs, R rhs, P&& policy, A&& action = {});

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
        if constexpr (storage_of<L> == storage::rational)
          return Lower<R>;
        else if constexpr (storage_of<R> == storage::rational)
          return -(Lower<L>/Notch<L>).value();
        else
          return ((Lower<R> - Lower<L>)/Notch<L>).value();
      }

      static constexpr rational calcFactor()
      {
        if constexpr (storage_of<L> == storage::rational)
          return Notch<R>;
        else if constexpr (storage_of<R> == storage::rational)
          return (rational{1}/Notch<L>).value();
        else
          return (Notch<R>/Notch<L>).value();
      }

    public:
      static constexpr rational Offset = calcOffset();
      static constexpr rational Factor = calcFactor();

      // Raw-space mapping is integer-only (no rational arithmetic needed)
      static constexpr bool is_integer_mapping =
          storage_of<L> != storage::rational && storage_of<R> != storage::rational
          && abs_den(Factor.Denominator) == 1 && abs_den(Offset.Denominator) == 1;

      // Map rhs.Raw into L's raw space (requires is_integer_mapping).
      //
      // The Offset/Factor formula assumes both sides use offset encoding —
      // i.e. R.Raw is the R-offset and the result is the L-offset. Two
      // adjustments make it work for direct-storage operands:
      //
      //   1. If R is storage_of<R> != storage::offset, rhs_raw is already R-value
      //      (not R-offset). Subtract Lower<R> first so the formula sees
      //      a true R-offset.
      //   2. If L is storage_of<L> != storage::offset, L.Raw must be L-value (not
      //      L-offset). Add Lower<L> after the formula. (Equivalently:
      //      raw_from_offset<L>.)
      //
      // Both adjustments use only integer arithmetic because is_integer_mapping
      // already guarantees Notch and Lower have integer denominators.
      static constexpr imax map_raw(auto rhs_raw)
      {
        imax r_offset = static_cast<imax>(rhs_raw);
        if constexpr (storage_of<R> != storage::offset)
          r_offset -= RawLo<R>;

        // Offset is an exact integer here (is_integer_mapping), so Offset.trunc()
        // is a constexpr constant (0 / +N / -N) — folds to the same add.
        imax l_offset = static_cast<imax>(Factor.Numerator) * r_offset + Offset.trunc();

        if constexpr (storage_of<L> != storage::offset)
          return l_offset + RawLo<L>;
        else
          return l_offset;
      }

    private:
      template<typename A>
      static constexpr void apply_clamp(L& lhs, R const& rhs, A&& action);

      template<typename P, typename A>
      static constexpr void apply_wrap(L& lhs, R const& rhs, P&& policy, A&& action);

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
    if constexpr (clamp_action<plain<A>>)
      action.fn(lhs, overshoot);
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
    if constexpr (wrap_action<plain<A>>)
      action.fn(lhs, excess);
  }

  template<boundable L, std::integral R>
  template<typename P, typename A>
  constexpr bool assignment<L,R>::handle_out_of_range(L& lhs, R rhs, imax lower, imax upper,
                                                     P&& policy, A&& action)
  {
    using PA = plain<A>;
    if constexpr (clamp_action<PA>)
    { apply_clamp(lhs, rhs, lower, upper, action); return true; }
    else if constexpr (wrap_action<PA>)
    { apply_wrap(lhs, rhs, lower, upper, action); return true; }
    else if constexpr (sentinel_action<PA>)
    {
      lhs = L::from_raw(sentinel_raw<L>());
      action.fn(lhs, static_cast<imax>(rhs));
      return true;
    }
    else if constexpr (error_action<PA>)
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
    if constexpr (storage_of<L> != storage::offset)
      lhs = L::from_raw(raw_cast<L>(rhs));
    else if constexpr (Lower<L> == Upper<L>)
      lhs = L::from_raw(0);   // notch_storage point grid: 0 is the only offset
    else if constexpr (HasQFormatFastPath<L>)
      lhs = L::from_raw(q_format_encode<L>(static_cast<imax>(rhs)));
    else // storage::offset, generic rational path
    {
      rational raw = ((rhs - Interval<L>.Lower)/Notch<L>).value();
      lhs = L::from_raw(raw_cast<L>(raw.Numerator / static_cast<umax>(raw.Denominator)));
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

    // The out-of-range check runs unconditionally — clamp/wrap/sentinel
    // policies handle it via apply_*, which is constexpr-clean. Only the
    // unhandled-checked path winds up calling `policy.report`, which
    // contains its own `std::is_constant_evaluated()` guard.
    if constexpr (not Interval<L>.includes(Interval<R>))
    {
      if constexpr (IsIntegerInterval<L>)
      {
        // Skip the runtime range branch entirely when every handler would
        // be dead anyway — the dead branch otherwise inhibits autovec.
        if constexpr (needs_runtime_domain_check<L, plain<P>, plain<A>>)
        {
          constexpr imax lower = LowerImax<L>;
          constexpr imax upper = UpperImax<L>;
          if (static_cast<imax>(rhs) < lower || static_cast<imax>(rhs) > upper)
            if (handle_out_of_range(lhs, rhs, lower, upper, policy, action)) return lhs;
        }
      }
      else if (not Interval<L>.includes(rhs))
      {
        // Non-integer L bounds: route through the rational path so fractional
        // Lower/Upper drive clamp/sentinel/error correctly.
        return assignment<L, rational>::assign(lhs, rational{rhs}, policy, action);
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
      overshoot = (rhs - clamped).value_or(rational{0});
    else
      overshoot = rhs - clamped;

    if constexpr (storage_of<L> == storage::rational)
      lhs = L::from_raw(clamped);
    else if constexpr (Lower<L> == Upper<L>)
      lhs = L::from_raw(0);
    else
    {
      rational raw = ((clamped - Lower<L>)/Notch<L>).value();
      umax den = static_cast<umax>(raw.Denominator);
      // Clamp happened first (above), then we round the clamped value onto
      // a notch. Order matters: rounding-then-clamping could push a
      // boundary-adjacent midpoint *past* the boundary by one notch.
      // `raw_from_offset<L>` produces the proper Raw for both offset-encoded
      // and direct-encoded storage (the latter adds Lower<L> back).
      lhs = L::from_raw(raw_from_offset<L>(round_quotient<L, P>(raw.Numerator, den)));
    }

    if constexpr (clamp_action<plain<A>>)
      action.fn(lhs, overshoot);
  }

  // apply_wrap for real R — modular reduction into [Lower, Lower + range)
  // followed by store_checked so the rounding policy still applies if rhs
  // doesn't land on a notch after wrapping. range = Upper - Lower + Notch.
  template <boundable L, typename R>
    requires real<R>
  template<typename P, typename A>
  constexpr void assignment<L,R>::apply_wrap(L& lhs, R rhs, P&& policy, A&& action)
  {
    rational rhs_r{rhs};
    rational lower_r = Lower<L>;
    rational range   = ((Upper<L> - lower_r).value() + Notch<L>).value();
    // q = floor((rhs - lower) / range), wrapped = rhs - q * range
    rational shifted = (rhs_r - lower_r).value();
    imax q = (shifted / range).value().floor();
    rational wrapped = (rhs_r - (rational{q} * range).value()).value();

    // Re-enter the rational-rhs specialization for the actual store so the
    // notch / rounding policy logic is exercised once.
    assignment<L, rational>::store_checked(lhs, wrapped, policy, action);

    if constexpr (wrap_action<plain<A>>)
      action.fn(lhs, q);
  }

  template <boundable L, typename R>
    requires real<R>
  template<typename P, typename A>
  constexpr bool assignment<L,R>::store_checked(L& lhs, R rhs, P&& policy, A&& action)
  {
    if constexpr (storage_of<L> == storage::rational)
    { lhs = L::from_raw(rhs); return true; }
    else if constexpr (Lower<L> == Upper<L>)
    {
      // Singleton grid: Raw layout depends on encoding. For offset
      // encoding the lone slot is Raw=0; for direct storage Raw is the
      // value itself (`Lower`).
      if constexpr (storage_of<L> != storage::offset)
        lhs = L::from_raw(raw_cast<L>(RawLo<L>));
      else
        lhs = L::from_raw(0);
      return true;
    }
    else
    {
      rational raw = ((rhs - Lower<L>)/Notch<L>).value();
      umax den = static_cast<umax>(raw.Denominator);
      // `raw_from_offset<L>` handles both offset-encoded and direct-encoded
      // storage — for direct, it adds Lower<L> back to produce the value.
      if (den == 1)
      { lhs = L::from_raw(raw_from_offset<L>(raw.Numerator)); return true; }

      constexpr bool has_round_flag =
           HasPolicy<L, P, round_nearest> || HasPolicy<L, P, round_floor>
        || HasPolicy<L, P, round_ceil>    || HasPolicy<L, P, round_half_even>
        || HasPolicy<L, P, ignore_round>;
      if constexpr (has_round_flag)
        lhs = L::from_raw(raw_from_offset<L>(round_quotient<L, P>(raw.Numerator, den)));
      else if (policy.round_check())
      {
        auto msg = bnd::to_string(rhs) + " does not land on notch " + bnd::to_string(Notch<L>);
        if constexpr (error_action<plain<A>>)
        { action.fn(lhs, errc::rounding_error, std::string_view(msg)); return false; }
        policy.report(errc::rounding_error, msg);
        return false;
      }
      else
        lhs = L::from_raw(raw_from_offset<L>(round_quotient<L, P>(raw.Numerator, den)));
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
    if (not Interval<L>.includes(rhs))
    {
      using PA = plain<A>;
      if constexpr (clamp_action<PA>)
      { apply_clamp(lhs, rhs, policy, action); return lhs; }
      else if constexpr (sentinel_action<PA>)
      {
        lhs = L::from_raw(sentinel_raw<L>());
        action.fn(lhs, rhs);
        return lhs;
      }
      else if constexpr (error_action<PA>)
      {
        auto msg = bnd::to_string(rhs) + " is not in " + bnd::to_string(Interval<L>);
        action.fn(lhs, errc::domain_error, std::string_view(msg));
        return lhs;
      }
      else if constexpr (HasPolicy<L, P, clamp>)
      { apply_clamp(lhs, rhs, policy, action); return lhs; }
      else if constexpr (HasPolicy<L, P, wrap>)
      { apply_wrap(lhs, rhs, policy, action); return lhs; }
      else if (domain_fail(lhs, policy,
                 bnd::to_string(rhs) + " is not in " + bnd::to_string(Interval<L>)))
        return lhs;
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
    // `RawLo`/`RawHi` are raw-space constants by construction (Lower/Upper
    // for direct storage, 0/NotchCount for notch-offset), so they're
    // already the correct Raw — no `raw_from_offset` adjustment needed.
    lhs = L::from_raw((as_rational(rhs) < Lower<L>)
      ? raw_cast<L>(RawLo<L>) : raw_cast<L>(RawHi<L>));
    auto overshoot = as_rational(rhs) - as_rational(lhs);
    if constexpr (clamp_action<plain<A>>)
      action.fn(lhs, overshoot);
  }

  template<boundable L, boundable R>
  template<typename P, typename A>
  constexpr void assignment<L,R>::apply_wrap(L& lhs, R const& rhs, P&& policy, A&& action)
  {
    // The integer modular wrap (range = Upper - Lower + 1, integer values) is
    // only correct on a unit-integer grid — notch 1 with integer bounds, so
    // consecutive integers are adjacent grid points. Any other grid (fractional
    // notch, non-integer bounds) routes through the rational modular wrap.
    if constexpr (IsIntegerInterval<L> && abs_den(Notch<L>.Denominator) == 1
                  && Notch<L>.Numerator == 1)
    {
      // Unit-integer fast path: modular wrap on the integer value.
      imax rhs_imax = as_rational(rhs).trunc();
      constexpr imax lower = LowerImax<L>;
      constexpr imax upper = UpperImax<L>;
      imax range = upper - lower + 1;
      imax shifted = rhs_imax - lower;
      imax wrapped = ((shifted % range) + range) % range;
      imax excess  = (shifted < 0) ? ((shifted - range + 1) / range) : (shifted / range);
      from_value(lhs, wrapped + lower);
      if constexpr (wrap_action<plain<A>>)
        action.fn(lhs, excess);
    }
    else
    {
      // Fractional / notch-aligned destination: reuse the rational
      // modular-wrap path (range = Upper - Lower + Notch; store_checked
      // applies the rounding policy). Mirrors how the integral-rhs assign
      // routes non-integer L through assignment<L, rational>.
      assignment<L, rational>::apply_wrap(lhs, as_rational(rhs), policy, action);
    }
  }

  template<boundable L, boundable R>
  template<typename P, typename A>
  constexpr bool assignment<L,R>::try_clamp_or_fail(L& lhs, R const& rhs, P&& policy, A&& action)
  {
    using PA = plain<A>;
    if constexpr (clamp_action<PA>)
    { apply_clamp(lhs, rhs, action); return true; }
    else if constexpr (wrap_action<PA>)
    { apply_wrap(lhs, rhs, policy, action); return true; }
    else if constexpr (sentinel_action<PA>)
    {
      lhs = L::from_raw(sentinel_raw<L>());
      action.fn(lhs, as_rational(rhs));
      return true;
    }
    else if constexpr (error_action<PA>)
    {
      auto msg = bnd::to_string(as_rational(rhs))
               + " is not in " + bnd::to_string(Interval<L>);
      action.fn(lhs, errc::domain_error, std::string_view(msg));
      return true;
    }
    else if constexpr (HasPolicy<L, P, clamp>)
    { apply_clamp(lhs, rhs, action); return true; }
    else if constexpr (HasPolicy<L, P, wrap>)
    { apply_wrap(lhs, rhs, policy, action); return true; }
    return domain_fail(lhs, policy,
      bnd::to_string(as_rational(rhs)) + " is not in " + bnd::to_string(Interval<L>));
  }

  template<boundable L, boundable R>
  template<typename P>
  constexpr void assignment<L,R>::store(L& lhs, R const& rhs, P&&)
  {
    if constexpr (is_integer_mapping)
    {
      // exact: Factor and Offset have integer denominators, no rounding ambiguity
      if constexpr (Offset == 0 && Factor == 1)
        lhs = L::from_raw(raw_cast<L>(rhs.raw()));
      else
        lhs = L::from_raw(raw_cast<L>(map_raw(rhs.raw())));
    }
    else
    {
      rational rat = *(Offset + *(Factor * rhs.raw()));
      umax ad = static_cast<umax>(abs_den(rat.Denominator));
      umax q;
      if constexpr (HasPolicy<L, P, round_nearest>)
        q = (rat.Numerator + ad / 2) / ad;     // half-away-from-zero
      else
        q = rat.Numerator / ad;                // truncation toward zero
      // `rat` is the L-offset; `raw_from_offset<L>` converts to L.Raw,
      // adding Lower<L> back for direct-storage targets.
      lhs = L::from_raw((rat.Denominator < 0)
        ? raw_from_offset<L>(-static_cast<imax>(q))
        : raw_from_offset<L>(q));
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
    static_assert(abs_den(Factor.Denominator) == 1 || HasPolicy<L, P, ignore_round>
                  || point_exactly_assignable<L, R>,
      "incompatible notches: use with_truncate() or policy<ignore_round>() to allow rounding");

    if constexpr (not Interval<L>.includes(Interval<R>))
    {
      if constexpr (needs_runtime_domain_check<L, plain<P>, plain<A>>)
      {
        if constexpr (is_integer_mapping)
        {
          if (imax mapped = map_raw(rhs.raw()); mapped < RawLo<L> || mapped > RawHi<L>)
            if (try_clamp_or_fail(lhs, rhs, policy, action)) return lhs;
        }
        else if (not Interval<L>.includes(as_rational(rhs)))
          if (try_clamp_or_fail(lhs, rhs, policy, action)) return lhs;
      }
    }

    store(lhs, rhs, policy);
    return lhs;
  }
} // namespace bnd::detail

#endif // BNDassignmentHPP
