// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_DETAIL_ASSIGNMENT_HPP
#define BEMAN_INSIDE_DETAIL_ASSIGNMENT_HPP

#include <beman/inside/generic.hpp>
#include <beman/inside/grid.hpp>

namespace beman::inside::detail
{
  //---------------------------------------------------------------------------
  // assignment — narrowing/coercion between bounded and arithmetic types. Three
  // specialisations dispatch on the source (integral / fractional / insidable),
  // each routing through `store` (in-range) and `handle_out_of_range` /
  // `apply_clamp` / `apply_wrap` (policy). The insidable path also exposes
  // `is_integer_mapping` / `map_raw` — a pure-integer formula in the hot path.
  //---------------------------------------------------------------------------
  // needs_runtime_domain_check<L, P, A>: true iff any out-of-range handler would
  // fire (an action, a clamp/wrap bit, or default-throw under checked).
  // When false (typically `unsafe`, no action) the runtime range branch in
  // `assign` is dead code and skipped, letting the autovectorizer kick in.
  //---------------------------------------------------------------------------
  template <insidable L, typename P, typename A>
  inline constexpr bool needs_runtime_domain_check =
         clamp_action   <plain_t<A>>
      || wrap_action    <plain_t<A>>
      || error_action   <plain_t<A>>
      || has_policy<L, P, clamp>
      || has_policy<L, P, wrap>
      || ((plain_t<P>::test(checked)
           || is_checked(policy_of<L> | (plain_t<P>::test(detail::unsafe_marker) ? detail::unsafe_marker : none)))
          && !has_policy<L, P, ignore_domain>);

  // Shared out-of-range policy cascade. Order: clamp/wrap/error *actions*, then
  // clamp/wrap *policy* bits, then `domain_fail`. The three caller-supplied
  // callables cover how clamp/wrap store and the error-message rhs view. `Wrappable` is false on the fractional
  // path (no wrap *action* branch). Returns true when a handler resolved the write.
  template <bool Wrappable, insidable L, typename P, typename A,
            typename DoClamp, typename DoWrap, typename MsgView>
  constexpr bool dispatch_out_of_range(L& lhs, P&& policy, A&& action,
                                       DoClamp do_clamp, DoWrap do_wrap,
                                       [[maybe_unused]] MsgView msg_view)
  {
    using PA = plain_t<A>;
    if constexpr (clamp_action<PA>)
    { do_clamp(); return true; }
    else if constexpr (Wrappable && wrap_action<PA>)
    { do_wrap(); return true; }
    else if constexpr (error_action<PA>)
    {
      action.Fn(lhs, errc::domain_error, errc_message(errc::domain_error));
      return true;
    }
    else if constexpr (has_policy<L, P, clamp>)
    { do_clamp(); return true; }
    else if constexpr (has_policy<L, P, wrap>)
    { do_wrap(); return true; }
    else
      return domain_fail(lhs, policy);
  }

  //---------------------------------------------------------------------------
  // assignment
  //---------------------------------------------------------------------------
  template <typename L, typename R>
  struct assignment;

  //---------------------------------------------------------------------------
  // assign(insidable, integral)
  //---------------------------------------------------------------------------
  template <insidable L, std::integral R>
  struct assignment<L,R>
  {
    private:
      template<typename A>
      static constexpr void apply_clamp(L& lhs, R rhs, imax lower, imax upper, A&& action)
      {
        // Pre: rhs is out of [lower, upper] (only called from handle_out_of_range),
        // so the two-way pick is the full clamp.
        imax clamped = static_cast<imax>(rhs) < lower ? lower : upper;
        imax overshoot = static_cast<imax>(rhs) - clamped;
        from_value(lhs, clamped);
        if constexpr (clamp_action<plain_t<A>>)
          action.Fn(lhs, overshoot);
      }

      template<typename A>
      static constexpr void apply_wrap(L& lhs, R rhs, imax lower, imax upper, A&& action)
      {
        // Overflow-safe modular wrap: `upper-lower+1` and `rhs-lower` can exceed imax,
        // so the reduction runs in umax (both fit umax for any valid grid; the result
        // lands back in [lower, upper] ⊂ imax).
        const umax urange = static_cast<umax>(upper) - static_cast<umax>(lower) + 1u;
        const imax ri = static_cast<imax>(rhs);
        if (urange == 0)                              // span == 2^64−1: wrap is identity
        {
          from_value(lhs, ri);
          if constexpr (wrap_action<plain_t<A>>) action.Fn(lhs, imax{0});
          return;
        }
        umax w;
        imax excess;
        if (ri >= lower)
        {
          const umax dist = static_cast<umax>(ri) - static_cast<umax>(lower);  // true, ≥ 0
          w = dist % urange;
          excess = static_cast<imax>(dist / urange);
        }
        else
        {
          const umax dist = static_cast<umax>(lower) - static_cast<umax>(ri);  // true, > 0
          const umax m = dist % urange;
          w = (m == 0) ? 0u : (urange - m);
          excess = -static_cast<imax>((dist + urange - 1u) / urange);          // −ceil(dist/range)
        }
        from_value(lhs, static_cast<imax>(static_cast<umax>(lower) + w));
        if constexpr (wrap_action<plain_t<A>>)
          action.Fn(lhs, excess);
      }

      template<typename P, typename A>
      static constexpr bool handle_out_of_range(L& lhs, R rhs, imax lower, imax upper,
                                                P&& policy, A&& action)
      {
        return dispatch_out_of_range<true>(lhs, policy, action,
          [&]{ apply_clamp(lhs, rhs, lower, upper, action); },
          [&]{ apply_wrap (lhs, rhs, lower, upper, action); },
          [&]{ return rhs; });
      }

      static constexpr void store(L& lhs, R rhs)
      {
        if constexpr (!index_raw<L>)
          lhs = L::from_raw(raw_cast<L>(rhs));
        else if constexpr (lower_of<L> == upper_of<L>)
          lhs = L::from_raw(0);   // notch_storage point grid: 0 is the only offset
        else if constexpr (has_qformat_fast_path<L>)
          lhs = L::from_raw(q_format_encode<L>(static_cast<imax>(rhs)));
        else // index storage on a notch 1/K grid: the offset is an exact integer
        {
          rational raw = ((rhs - interval_of<L>.Lower)/notch_of<L>).value();
          lhs = L::from_raw(raw_cast<L>(raw.Numerator));
        }
      }

    public:
      // An integer lands on L's grid whenever the notch is 1/K over an integer
      // Lower (or the grid is continuous); otherwise it may fall between notches
      // and must round or report exactly like the same value given as a rational.
      static constexpr bool integers_on_grid =
          notch_of<L> == 0 || (notch_of<L>.Numerator == 1 && abs_den(lower_of<L>.Denominator) == 1);

      template<typename P, typename A = no_action>
      static constexpr L& assign(L& lhs, R const& rhs, P&& policy, A&& action = {})
      {
        static_assert(not excludes(interval_of<L>, interval_of<R>));

        if constexpr (!integers_on_grid)
          return assignment<L, rational>::assign(lhs, rational{rhs}, policy, std::forward<A>(action));
        else
        {
          // The out-of-range check runs unconditionally — clamp/wrap
          // policies handle it via apply_*, which is constexpr-clean. Only the
          // unhandled-checked path winds up calling `policy.report`, which
          // contains its own `std::is_constant_evaluated()` guard.
          if constexpr (not includes(interval_of<L>, interval_of<R>))
          {
            if constexpr (is_integer_interval<L>)
            {
              // Skip the runtime range branch entirely when every handler would
              // be dead anyway — the dead branch otherwise inhibits autovec.
              if constexpr (needs_runtime_domain_check<L, plain_t<P>, plain_t<A>>)
              {
                constexpr imax lower = lower_imax<L>;
                constexpr imax upper = upper_imax<L>;
                if (static_cast<imax>(rhs) < lower || static_cast<imax>(rhs) > upper) [[unlikely]]
                {
                  // The integer clamp/wrap formulas need consecutive integers to be
                  // adjacent grid points (notch 1); a finer notch wraps modulo
                  // span + notch on the rational path.
                  if constexpr (notch_of<L> == 1)
                  {
                    if (handle_out_of_range(lhs, rhs, lower, upper, policy, action)) return lhs;
                  }
                  else
                    return assignment<L, rational>::assign(lhs, rational{rhs}, policy, action);
                }
              }
            }
            else if (not includes(interval_of<L>, rhs))
            {
              // Non-integer L bounds: route through the rational path so fractional
              // Lower/Upper drive clamp/error correctly.
              return assignment<L, rational>::assign(lhs, rational{rhs}, policy, action);
            }
          }

          store(lhs, rhs);
          return lhs;
        }
      }
  };

  //---------------------------------------------------------------------------
  // assign(insidable, floating_point | rational)
  //---------------------------------------------------------------------------
  template <insidable L, typename R>
    requires fractional<R>
  struct assignment<L,R>
  {
    private:
      template<typename P, typename A>
      static constexpr void apply_clamp(L& lhs, R rhs, P&&, A&& action)
      {
        R clamped = (rhs < lower_of<L>) ? static_cast<R>(lower_of<L>) : static_cast<R>(upper_of<L>);
        R overshoot;
        if constexpr (std::same_as<R, rational>)
          overshoot = (rhs - clamped).value_or(rational{0});
        else
          overshoot = rhs - clamped;

        // The clamp target is an interval endpoint — a grid point — so the slot is 0
        // or max_index_v, no rounding. f64 takes the endpoint as a double, rational
        // the exact constant (a double round-trip would lose non-dyadic endpoints);
        // raw_from_offset<L> adds Lower back for direct-encoded storage.
        if constexpr (fp_raw<L>)
          lhs = L::from_raw((rhs < lower_of<L>) ? static_cast<double>(lower_of<L>)
                                             : static_cast<double>(upper_of<L>));
        else if constexpr (rational_raw<L>)
          lhs = L::from_raw((rhs < lower_of<L>) ? lower_of<L> : upper_of<L>);
        else
          lhs = L::from_raw(raw_from_offset<L>(
              (rhs < lower_of<L>) ? umax{0} : max_index_v<L>));

        if constexpr (clamp_action<plain_t<A>>)
          action.Fn(lhs, overshoot);
      }

    public:
      // Exposed (not private) so the insidable-rhs wrap path can reuse the
      // rational specialization's modular wrap on fractional/notch grids, and
      // so the wrap path can reuse store_checked after computing the wrapped
      // value.
      //
      // apply_wrap for a fractional R — modular reduction into [Lower, Lower + range)
      // followed by store_checked so the rounding policy still applies if rhs
      // doesn't land on a notch after wrapping. range = Upper - Lower + Notch.
      template<typename P, typename A>
      static constexpr void apply_wrap(L& lhs, R rhs, P&& policy, A&& action)
      {
        // Round onto the lattice first (by the policy, like every other store),
        // then fold: an on-lattice value folds onto a grid point, so rounding
        // can never carry it past Upper.
        rational rhs_r{rhs};
        if constexpr (has_policy<L, P, snap>)
          rhs_r = round_to_lattice<L, P>(rhs_r);
        rational lower_r = lower_of<L>;
        rational range   = ((upper_of<L> - lower_r).value() + notch_of<L>).value();
        // q = floor((rhs - lower) / range), wrapped = rhs - q * range
        rational shifted = (rhs_r - lower_r).value();
        imax q = floor((shifted / range).value());
        rational wrapped = (rhs_r - (rational{q} * range).value()).value();

        // Re-enter the rational-rhs specialization for the actual store so the
        // notch / rounding policy logic is exercised once.
        assignment<L, rational>::store_checked(lhs, wrapped, policy, action);

        if constexpr (wrap_action<plain_t<A>>)
          action.Fn(lhs, q);
      }

      // 128-bit rounded store — the offset slot of an in-range rhs computed
      // directly in wide arithmetic when the exact 64-bit rational formation
      // of (rhs − Lower)/Notch overflows (full-mantissa fp-derived sources on
      // grids with large |Lower|):
      //     slot + remainder/divisor = (rhs − Lower)·d_n / (a_dr·a_dl·n_n)
      // The compile-time divisor factors are gcd-reduced first, so the only
      // wide operations are one 128×64 multiply and one 128÷64 divide.
      // ok == false when the reduced divisor or dividend exceeds the 128-bit
      // envelope (or rhs is out of range — callers check range first).
      struct wide_quotient { umax Slot; umax Remainder; umax Divisor; bool Ok; };

      static constexpr wide_quotient wide_offset_quotient(rational const& rv)
      {
        if constexpr (notch_of<L> == 0)
          return {};                       // continuous grids never index slots
        else
        {
          constexpr umax n_l     = lower_of<L>.Numerator;
          constexpr umax a_dl    = abs_den(lower_of<L>.Denominator);
          constexpr bool low_neg = lower_of<L>.Denominator < 0;
          constexpr umax n_n     = notch_of<L>.Numerator;   // Notch > 0: d_n > 0
          constexpr umax d_n     = static_cast<umax>(notch_of<L>.Denominator);

          // Compile-time divisor part; a grid whose a_dl·n_n cannot fit umax
          // is beyond the wide envelope entirely.
          constexpr umax den_ct = []{
            umax p;
            return mul_overflow(a_dl, n_n, &p) ? umax{0} : p;
          }();
          if constexpr (den_ct == 0)
            return {};
          else
          {
            constexpr umax g1   = std::gcd(d_n, den_ct);
            constexpr umax d_n1 = d_n / g1;
            constexpr umax den1 = den_ct / g1;

            const umax a_dr    = abs_den(rv.Denominator);
            const bool rhs_neg = rv.Denominator < 0;

            // Offset numerator over the common denominator a_dr·a_dl:
            //   s_r·n_r·a_dl − s_l·n_l·a_dr  (≥ 0 for in-range rhs).
            // Each product is < 2^127 (numerator < 2^64, denominator ≤ imax),
            // so the same-sign sum below cannot carry out of 128 bits.
            const u128 val = umul(rv.Numerator, a_dl);
            const u128 low = umul(n_l, a_dr);
            u128 offset;
            if (!rhs_neg && low_neg)
              offset = u128{val.Hi + low.Hi + (val.Lo + low.Lo < val.Lo ? 1u : 0u),
                            val.Lo + low.Lo};
            else if (!rhs_neg && !low_neg)
            {
              if (cmp128(val, low) < 0) return {};         // rhs < Lower
              offset = u128{val.Hi - low.Hi - (val.Lo < low.Lo ? 1u : 0u),
                            val.Lo - low.Lo};
            }
            else if (rhs_neg && low_neg)
            {
              if (cmp128(low, val) < 0) return {};         // rhs < Lower
              offset = u128{low.Hi - val.Hi - (low.Lo < val.Lo ? 1u : 0u),
                            low.Lo - val.Lo};
            }
            else
              return {};                                   // rhs < 0 ≤ Lower

            const umax g2    = std::gcd(d_n1, a_dr);
            const umax d_n2  = d_n1 / g2;
            const umax a_dr1 = a_dr / g2;

            umax divisor;
            if (mul_overflow(a_dr1, den1, &divisor)
                || divisor > static_cast<umax>(std::numeric_limits<imax>::max()))
              return {};

            const mul128_result dividend = mul128(offset, d_n2);
            if (dividend.Overflowed)
              return {};

            const divmod128_result qr = divmod128(dividend.Value, divisor);
            if (qr.Quotient.Hi != 0)
              return {};                    // slot beyond any 64-bit index space
            return {qr.Quotient.Lo, qr.Remainder, divisor, true};
          }
        }
      }

      template<typename P, typename A = no_action>
      static constexpr bool store_checked(L& lhs, R rhs, P&& policy, A&& action = {})
      {
        if constexpr (rational_raw<L> && notch_of<L> == 0)
        { lhs = L::from_raw(rhs); return true; }   // continuous: store verbatim
        else if constexpr (fp_raw<L>)
        {
          // f64 target: raw IS the value — snap to the dyadic grid (range handling
          // already ran in the assign cascade; finite guard mirrors store_f64's).
          const double v = static_cast<double>(rhs);
          if (!(v - v == 0)) [[unlikely]]                  // assign() screens these first
          { policy.report(errc::not_finite); return false; }
          lhs = L::from_raw(snap_double<grid_of<L>, rounding_for<L, P>>(v));
          return true;
        }
        else if constexpr (lower_of<L> == upper_of<L>)
        {
          // Singleton grid: offset encoding → Raw=0; rational/direct → Raw = Lower.
          if constexpr (rational_raw<L>)
            lhs = L::from_raw(lower_of<L>);
          else if constexpr (!index_raw<L>)
            lhs = L::from_raw(raw_cast<L>(raw_lo<L>));
          else
            lhs = L::from_raw(0);
          return true;
        }
        else
        {
          // Store the k-th notch slot: rational storage holds the snapped value;
          // raw_from_offset<L> covers offset- and direct-encoded integers.
          auto store_slot = [&](auto k)
          {
            if constexpr (rational_raw<L>)
              lhs = L::from_raw((lower_of<L> + (rational{k} * notch_of<L>).value()).value());
            else
              lhs = L::from_raw(raw_from_offset<L>(k));
          };

          constexpr bool has_round_flag =
               has_policy<L, P, round_nearest> || has_policy<L, P, round_floor>
            || has_policy<L, P, round_ceil>    || has_policy<L, P, round_half_even>
            || has_policy<L, P, snap>;

          // Q-format integer shortcut: with integer Lower and notch 1/K the offset is
          // (num − Lo·aden)·(K/g) / (aden/g), g = gcd(aden, K) — one gcd + integer ops
          // instead of two rational ops. round_quotient is invariant under reduction,
          // so the slot is bit-identical to the rational path. Oversized denominators
          // fall through (the kMaxDen guard keeps every product inside imax).
          if constexpr (has_qformat_fast_path<L> && !fp_raw<L> && notch_of<L> != 0)
          {
            constexpr imax K  = abs_den(notch_of<L>.Denominator);
            constexpr imax Lo = lower_imax<L>;
            constexpr umax kKM = []{
              // 2 · K · M with saturation (M bounds |value| and the offset span)
              umax k = static_cast<umax>(K);
              umax m = static_cast<umax>(
                  ceil(((detail::abs(lower_of<L>) > detail::abs(upper_of<L>)
                      ? detail::abs(lower_of<L>) : detail::abs(upper_of<L>))
                   ))) * 2 + 2;
              if (k > std::numeric_limits<umax>::max() / m)
                return std::numeric_limits<umax>::max();
              umax km = k * m;
              return (km > std::numeric_limits<umax>::max() / 2)
                       ? std::numeric_limits<umax>::max() : km * 2;
            }();
            constexpr umax kMaxDen =
                static_cast<umax>(std::numeric_limits<imax>::max()) / kKM;

            const rational rv{rhs};                       // exact (copy for rational R)
            const umax aden = abs_den(rv.Denominator);
            if (kMaxDen != 0 && aden <= kMaxDen)
            {
              const umax g    = std::gcd(aden, static_cast<umax>(K));
              const umax den2 = aden / g;
              const imax k2   = K / static_cast<imax>(g);
              const imax num  = signed_numerator(rv);
              const umax onum =                          // ≥ 0: rhs ≥ Lower (in range)
                  static_cast<umax>((num - Lo * static_cast<imax>(aden)) * k2);
              if (den2 == 1)
              { store_slot(onum); return true; }
              if constexpr (has_round_flag)
              { store_slot(round_quotient<L, P>(onum, den2)); return true; }
              // strict policy, off-notch: fall through to the rational path for
              // the error message / action plumbing (cold).
            }
          }

          // The exact quotient can overflow the 64-bit rational range (huge
          // source denominator × fine notch). Recompute the slot directly in
          // 128-bit (wide_offset_quotient above); only a result beyond even
          // that envelope reports errc::overflow — never an unchecked expected deref,
          // which would escape noexcept callers (the math engines) as
          // terminate. Rounding here is the offset rule (round_offset), the
          // same semantics round_quotient falls back to past 64 bits.
          const auto quotient = (rhs - lower_of<L>)/notch_of<L>;
          if (!quotient.has_value()) [[unlikely]]
          {
            const wide_quotient wide = wide_offset_quotient(rational{rhs});
            if (!wide.Ok)
            {
              if constexpr (error_action<plain_t<A>>)
              { action.Fn(lhs, errc::overflow, errc_message(errc::overflow)); return false; }
              policy.report(errc::overflow);
              return false;
            }
            if (wide.Remainder == 0)
            { store_slot(wide.Slot); return true; }
            if constexpr (has_round_flag)
            { store_slot(round_offset<L, P>(wide.Slot, wide.Remainder, wide.Divisor)); return true; }
            if (policy.round_check()) [[unlikely]]
            {
              if constexpr (error_action<plain_t<A>>)
              { action.Fn(lhs, errc::rounding_error, errc_message(errc::rounding_error)); return false; }
              policy.report(errc::rounding_error);
              return false;
            }
            store_slot(round_offset<L, P>(wide.Slot, wide.Remainder, wide.Divisor));
            return true;
          }
          rational raw = *quotient;
          umax den = static_cast<umax>(raw.Denominator);
          if (den == 1)
          { store_slot(raw.Numerator); return true; }

          if constexpr (has_round_flag)
            store_slot(round_quotient<L, P>(raw.Numerator, den));
          else if (policy.round_check()) [[unlikely]]
          {
            if constexpr (error_action<plain_t<A>>)
            { action.Fn(lhs, errc::rounding_error, errc_message(errc::rounding_error)); return false; }
            policy.report(errc::rounding_error);
            return false;
          }
          else
            store_slot(round_quotient<L, P>(raw.Numerator, den));
          return true;
        }
      }

    private:
      // Range test for the source value. A floating source compares in double when
      // both endpoints are exact doubles (then the comparison is exact), instead of
      // converting the value to a rational first.
      static constexpr bool double_bounds_exact =
          std::floating_point<R>
          && rational{static_cast<double>(lower_of<L>)} == lower_of<L>
          && rational{static_cast<double>(upper_of<L>)} == upper_of<L>;

      // A rational source on integer endpoints compares by multiplying the
      // endpoint by the denominator (n/d ≤ m ⇔ n ≤ m·d; an overflowing m·d
      // exceeds any n) — exact, and no division.
      static constexpr bool integer_bounds =
          std::same_as<R, rational>
          && abs_den(lower_of<L>.Denominator) == 1 && abs_den(upper_of<L>.Denominator) == 1;

      static constexpr bool out_of_interval(R const& rhs)
      {
        if constexpr (double_bounds_exact)
        {
          constexpr double lo = static_cast<double>(lower_of<L>);
          constexpr double hi = static_cast<double>(upper_of<L>);
          return rhs < lo || rhs > hi;
        }
        else if constexpr (integer_bounds)
        {
          constexpr imax lo = signed_numerator(lower_of<L>);
          constexpr imax hi = signed_numerator(upper_of<L>);
          const umax n = rhs.Numerator, d = abs_den(rhs.Denominator);
          auto le = [&](umax m) { umax p; return mul_overflow(m, d, &p) || n <= p; };
          auto ge = [&](umax m) { umax p; return !mul_overflow(m, d, &p) && n >= p; };
          if (rhs.Denominator < 0 && n != 0)            // value −n/d < 0
          {
            bool in_lo, in_hi;
            if constexpr (lo >= 0) in_lo = false; else in_lo = le(safe_abs(lo));
            if constexpr (hi >= 0) in_hi = true;  else in_hi = ge(safe_abs(hi));
            return !(in_lo && in_hi);
          }
          bool in_lo, in_hi;                            // value n/d ≥ 0
          if constexpr (lo <= 0) in_lo = true;  else in_lo = ge(static_cast<umax>(lo));
          if constexpr (hi < 0)  in_hi = false; else in_hi = le(static_cast<umax>(hi));
          return !(in_lo && in_hi);
        }
        else
          return not includes(interval_of<L>, rhs);
      }

    public:
      template<typename P, typename A = no_action>
      static constexpr L& assign(L& lhs, R const& rhs, P&& policy, A&& action = {})
      {
        // NaN / ±inf: no rational value to round or range-check. clamp saturates
        // an infinity; everything else reports not_finite through the policy.
        if constexpr (std::floating_point<R>)
          if (!(rhs - rhs == 0)) [[unlikely]]
          {
            if constexpr (has_policy<L, P, clamp>)
              if (rhs == rhs)
                return assignment<L, rational>::assign(lhs, rhs > 0 ? upper_of<L> : lower_of<L>, policy);
            if constexpr (error_action<plain_t<A>>)
              action.Fn(lhs, errc::not_finite, errc_message(errc::not_finite));
            else
              policy.report(errc::not_finite);
            return lhs;
          }

        if (out_of_interval(rhs)) [[unlikely]]
        {
          // Round first: a value just outside may round onto an endpoint.
          if constexpr (rounds_before_range_check<L, plain_t<P>>)
            if (const auto rr = raw_if_rounds_inside<L, plain_t<P>>(rational{rhs}); rr.Ok)
            { lhs = L::from_raw(rr.Raw); return lhs; }
          // Fractional path has no wrap *action* branch (Wrappable = false).
          if (dispatch_out_of_range<false>(lhs, policy, action,
                [&]{ apply_clamp(lhs, rhs, policy, action); },
                [&]{ apply_wrap (lhs, rhs, policy, action); },
                [&]{ return rhs; }))
            return lhs;
        }

        store_checked(lhs, rhs, policy, action);
        return lhs;
      }
  };

  //---------------------------------------------------------------------------
  // assign(insidable, insidable)
  //---------------------------------------------------------------------------
  template <insidable L, insidable R>
  struct assignment<L,R>
  {
    private:
      // Offset/Factor map rhs.Raw → lhs.Raw via `lhs.Raw = Factor·rhs.Raw + Offset`.
      // Branches: L rational (pass value through), R rational (pre-divide by
      // notch_of<L>), both integer (the hot path, collapses to integer math).
      static constexpr rational calcOffset()
      {
        if constexpr (rational_raw<L>)
          return lower_of<R>;
        else if constexpr (notch_of<L> == 0)
          // Continuous fp_raw L: no grid to land on, mapping unused (store
          // routes through snap_double). 0 avoids the /notch_of<L> divide-by-zero.
          return rational{0};
        else if constexpr (rational_raw<R>)
          return -(lower_of<L>/notch_of<L>).value();
        else
          return ((lower_of<R> - lower_of<L>)/notch_of<L>).value();
      }

      static constexpr rational calcFactor()
      {
        if constexpr (rational_raw<L>)
          return notch_of<R>;
        else if constexpr (notch_of<L> == 0)
          // Continuous fp_raw L (see calcOffset). A denominator-1 Factor also
          // makes assign_notch_ok vacuously true (any value representable).
          return rational{0};
        else if constexpr (rational_raw<R>)
          return (rational{1}/notch_of<L>).value();
        else
          return (notch_of<R>/notch_of<L>).value();
      }

    public:
      static constexpr rational Offset = calcOffset();
      static constexpr rational Factor = calcFactor();

      // Raw-space integer-only mapping — requires integer raw storage on both
      // sides (not rational, not f64).
      static constexpr bool is_integer_mapping =
          !rational_raw<L> && !rational_raw<R>
          && !fp_raw<L> && !fp_raw<R>
          && abs_den(Factor.Denominator) == 1 && abs_den(Offset.Denominator) == 1;

      // Non-integer mapping folded to one integer multiply-add:
      //   Offset + Factor·raw = (o_s·f_d + raw·f_n·o_d) / (o_d·f_d)
      // with every coefficient compile-time. round_quotient is invariant under
      // fraction reduction, so rounding the unreduced pair is bit-identical to
      // reducing through the two rational ops first. ok gates on every product
      // (including the worst-case runtime numerator over R's raw range)
      // provably fitting imax; mul/add/den are zeroed when not ok.
      struct affine_map_t { imax Mul; imax Add; imax Den; bool Ok; };
      static constexpr affine_map_t affine_map = []{
        constexpr affine_map_t no{0, 0, 0, false};
        if constexpr (rational_raw<L> || rational_raw<R> || fp_raw<L> || fp_raw<R>
                      || notch_of<L> == 0 || is_integer_mapping)
          return no;
        else
        {
          constexpr umax cap = static_cast<umax>(std::numeric_limits<imax>::max());
          if (Factor.Numerator > cap || Offset.Numerator > cap)
            return no;
          const imax f_n = static_cast<imax>(Factor.Numerator);  // Factor > 0
          const imax f_d = abs_den(Factor.Denominator);
          const imax o_s = signed_numerator(Offset);
          const imax o_d = abs_den(Offset.Denominator);
          affine_map_t m{0, 0, 0, true};
          if (mul_overflow(f_n, o_d, &m.Mul) || mul_overflow(o_s, f_d, &m.Add)
              || mul_overflow(o_d, f_d, &m.Den))
            return no;
          // worst-case |numerator| over R's raw range
          constexpr imax hi_mag = raw_hi<R> < 0 ? -raw_hi<R> : raw_hi<R>;
          constexpr imax lo_mag = raw_lo<R> < 0 ? -raw_lo<R> : raw_lo<R>;
          const imax rmax = hi_mag > lo_mag ? hi_mag : lo_mag;
          imax term, num;
          if (mul_overflow(rmax, m.Mul, &term)
              || add_overflow(term, m.Add < 0 ? -m.Add : m.Add, &num))
            return no;
          // round_quotient equivalence: rounding is reduction-invariant, but
          // its value-index-vs-offset branch CHOICE keys on m·di + num fitting
          // imax — mirror those checks for the unreduced den so both forms
          // take the same branch (ties on negatives differ across branches).
          constexpr auto zl = (lower_of<L> / notch_of<L>).value_or(rational{0});
          if (abs_den(zl.Denominator) == 1)
          {
            if (zl.Numerator > cap)
              return no;
            const imax mbias = signed_numerator(zl);
            imax mdi, total;
            if (mul_overflow(mbias, m.Den, &mdi) || add_overflow(mdi, num, &total))
              return no;
          }
          return m;
        }
      }();

      // Map rhs.Raw into L's raw space (requires is_integer_mapping). The
      // Offset/Factor formula assumes offset encoding both sides; for direct
      // storage, subtract lower_of<R> first (R-value → R-offset) and add lower_of<L>
      // after (raw_from_offset<L>). All integer (is_integer_mapping guarantees it).
      static constexpr imax map_raw(auto rhs_raw)
      {
        imax r_offset = rhs_raw;
        if constexpr (!index_raw<R>)
          r_offset -= raw_lo<R>;

        // Offset is an exact integer here, so trunc(Offset) is a constexpr constant.
        imax l_offset = static_cast<imax>(Factor.Numerator) * r_offset + trunc(Offset);

        if constexpr (!index_raw<L>)
          return l_offset + raw_lo<L>;
        else
          return l_offset;
      }

    private:
      // Grid of the wrap "excess"/carry handed to an on_wrap action:
      // floor((value − Lower) / range) for value ∈ R's interval (range = span + notch).
      // Both operands are insides, so — like the clamp overshoot — the carry has a
      // known range and is delivered as an inside, not a raw imax.
      static constexpr grid wrap_excess_grid()
      {
        constexpr rational range = ((upper_of<L> - lower_of<L>).value() + notch_of<L>).value();
        return grid{ floor(((lower_of<R> - lower_of<L>).value() / range).value()),
                     floor(((upper_of<R> - lower_of<L>).value() / range).value()) };
      }

      template<typename A>
      static constexpr void apply_clamp(L& lhs, R const& rhs, A&& action)
      {
        // raw_lo/raw_hi are already the correct Raw (no raw_from_offset). Real storage
        // takes the endpoint as a double (raw_lo/Hi truncate fractional dyadic endpoints).
        if constexpr (fp_raw<L>)
          lhs = L::from_raw((as_rational(rhs) < lower_of<L>)
            ? static_cast<double>(lower_of<L>) : static_cast<double>(upper_of<L>));
        else
          lhs = L::from_raw((as_rational(rhs) < lower_of<L>)
            ? raw_cast<L>(raw_lo<L>) : raw_cast<L>(raw_hi<L>));
        // Overshoot (rhs − clamped) as an inside, via the result-grid inference of normal
        // inside arithmetic: both operands are insides, so the overshoot is too. It is always
        // in-grid and on-notch for grid_of<R> − grid_of<L>, so the construction is exact.
        if constexpr (clamp_action<plain_t<A>>)
        {
          constexpr grid OG = (grid_of<R> - grid_of<L>).value();
          beman::inside::inside<OG> overshoot{ (as_rational(rhs) - as_rational(lhs)).value() };
          action.Fn(lhs, overshoot);
        }
      }

      template<typename P, typename A>
      static constexpr void apply_wrap(L& lhs, R const& rhs, P&& policy, A&& action)
      {
        // The integer modular wrap (range = Upper - Lower + 1, integer values) is
        // only correct on a unit-integer grid — notch 1 with integer bounds, so
        // consecutive integers are adjacent grid points — and for a source whose
        // values are integers (no rounding to do). Anything else routes through
        // the rational modular wrap, which rounds by the policy first.
        if constexpr (is_integer_interval<L> && abs_den(notch_of<L>.Denominator) == 1
                      && notch_of<L>.Numerator == 1 && !fp_raw<R> && is_integer_aligned<R>)
        {
          // Unit-integer fast path: modular wrap on the integer value.
          imax rhs_imax = trunc(as_rational(rhs));
          constexpr imax lower = lower_imax<L>;
          constexpr imax upper = upper_imax<L>;
          imax range = upper - lower + 1;
          imax shifted = rhs_imax - lower;
          // floor division: one divide yields both the wrap and the carry
          imax excess  = shifted / range;
          imax wrapped = shifted % range;
          if (wrapped < 0) { wrapped += range; --excess; }
          from_value(lhs, wrapped + lower);
          if constexpr (wrap_action<plain_t<A>>)
            action.Fn(lhs, beman::inside::inside<wrap_excess_grid()>{excess});   // carry as an inside
        }
        else if constexpr (wrap_action<plain_t<A>>)
        {
          // Fractional destination with a wrap action: reuse the rational modular-wrap
          // path for the store/rounding, but wrap its imax carry `q` into an inside before
          // handing it to the user action.
          assignment<L, rational>::apply_wrap(lhs, as_rational(rhs), policy,
            beman::inside::on_wrap([&](auto& self, imax q){
              action.Fn(self, beman::inside::inside<wrap_excess_grid()>{q});
            }));
        }
        else
        {
          // Fractional destination, no wrap action: delegate unchanged.
          assignment<L, rational>::apply_wrap(lhs, as_rational(rhs), policy, action);
        }
      }

      template<typename P, typename A>
      static constexpr bool try_clamp_or_fail(L& lhs, R const& rhs, P&& policy, A&& action)
      {
        return dispatch_out_of_range<true>(lhs, policy, action,
          [&]{ apply_clamp(lhs, rhs, action); },
          [&]{ apply_wrap (lhs, rhs, policy, action); },
          [&]{ return as_rational(rhs); });
      }

      template<typename P>
      static constexpr void store(L& lhs, R const& rhs, P&& policy)
      {
        if constexpr (fp_raw<L>)
          // f64 target: raw IS the value — decode the source and snap to the dyadic
          // grid (the offset machinery below mis-encodes a double raw).
          lhs = L::from_raw(snap_double<grid_of<L>, rounding_for<L, P>>(as_double(rhs)));
        else if constexpr (rational_raw<L>)
          // rational target: raw IS the value — snap the decoded source through
          // the rational-rhs store (the offset machinery below would round the
          // VALUE to a notch index and store that number as the raw).
          assignment<L, rational>::store_checked(lhs, as_rational(rhs), policy,
                                                 no_action{});
        else if constexpr (is_integer_mapping)
        {
          // exact: Factor and Offset have integer denominators, no rounding ambiguity
          if constexpr (Offset == 0 && Factor == 1)
            lhs = L::from_raw(raw_cast<L>(rhs.raw()));
          else
            lhs = L::from_raw(raw_cast<L>(map_raw(rhs.raw())));
        }
        else if constexpr (affine_map.Ok)
        {
          // Folded non-integer mapping: one multiply-add, then the same
          // round_quotient (invariant under reduction — bit-identical to the
          // rational chain below).
          const imax num = affine_map.Add
                         + static_cast<imax>(rhs.raw()) * affine_map.Mul;
          const umax q = round_quotient<L, P>(
              static_cast<umax>(num < 0 ? -num : num),
              static_cast<umax>(affine_map.Den));
          lhs = L::from_raw(num < 0 ? raw_from_offset<L>(-static_cast<imax>(q))
                                    : raw_from_offset<L>(q));
        }
        else
        {
          rational rat = *(Offset + *(Factor * rhs.raw()));
          umax ad = static_cast<umax>(abs_den(rat.Denominator));
          // Round the L-offset to a notch index in VALUE space via round_quotient
          // (same as the scalar path), honouring every rounding mode.
          umax q = round_quotient<L, P>(rat.Numerator, ad);
          // rat is the L-offset; raw_from_offset<L> adds lower_of<L> back for direct storage.
          lhs = L::from_raw((rat.Denominator < 0)
            ? raw_from_offset<L>(-static_cast<imax>(q))
            : raw_from_offset<L>(q));
        }
      }

    public:
      template<typename P, typename A = no_action>
      static constexpr L& assign(L& lhs, R const& rhs, P&& policy, A&& action = {})
      {
        // wrap/clamp bring any value into range, so a disjoint rhs interval is fine
        // for them (matches the integral-rhs path); only strict policies reject it.
        static_assert(has_policy<L, P, wrap> || has_policy<L, P, clamp>
                      || not excludes(interval_of<L>, interval_of<R>),
          "rhs interval lies entirely outside lhs interval and the policy cannot bring it into range");
        static_assert(abs_den(Factor.Denominator) == 1 || has_policy<L, P, snap>
                      || point_exactly_assignable<L, R>,
          "incompatible notches: use with_snap() or policy<snap>() to allow rounding");

        // A `f64` source holds its value as a double raw, which the raw-mapping
        // formulas below would misread as an index: take the double path.
        if constexpr (fp_raw<R>)
          return assignment<L, double>::assign(lhs, as_double(rhs), policy, std::forward<A>(action));
        else if constexpr (not includes(interval_of<L>, interval_of<R>))
        {
          if constexpr (needs_runtime_domain_check<L, plain_t<P>, plain_t<A>>)
          {
            if constexpr (is_integer_mapping)
            {
              if (imax mapped = map_raw(rhs.raw()); mapped < raw_lo<L> || mapped > raw_hi<L>)
                if (try_clamp_or_fail(lhs, rhs, policy, action)) return lhs;
            }
            else if (const rational v = as_rational(rhs); not includes(interval_of<L>, v))
            {
              // Round first: a value just outside may round onto an endpoint.
              // (The integer mapping above lands on the lattice: nothing to round.)
              if constexpr (rounds_before_range_check<L, plain_t<P>>)
                if (const auto rr = raw_if_rounds_inside<L, plain_t<P>>(v); rr.Ok)
                { lhs = L::from_raw(rr.Raw); return lhs; }
              if (try_clamp_or_fail(lhs, rhs, policy, action)) return lhs;
            }
          }
        }

        store(lhs, rhs, policy);
        return lhs;
      }
  };
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_DETAIL_ASSIGNMENT_HPP
