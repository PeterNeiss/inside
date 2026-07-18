//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDassignmentHPP
#define BNDassignmentHPP

#include "bound/generic.hpp"
#include "bound/grid.hpp"

namespace bnd::detail
{
  //---------------------------------------------------------------------------
  // assignment — narrowing/coercion between bounded and arithmetic types. Three
  // specialisations dispatch on the source (integral / fractional / boundable),
  // each routing through `store` (in-range) and `handle_out_of_range` /
  // `apply_clamp` / `apply_wrap` (policy). The boundable path also exposes
  // `is_integer_mapping` / `map_raw` — a pure-integer formula in the hot path.
  //
  // NOTE: every member is defined *inline* in its specialization body (rather
  // than out-of-line). MSVC cannot reliably match out-of-line definitions of
  // member function templates to constrained partial specializations
  // (C2244/C2995/C3855); inlining sidesteps that and keeps the code portable.
  //---------------------------------------------------------------------------
  // needs_runtime_domain_check<L, P, A>: true iff any out-of-range handler would
  // fire (an action, a clamp/wrap/sentinel bit, or default-throw under checked).
  // When false (typically `unsafe`, no action) the runtime range branch in
  // `assign` is dead code and skipped, letting the autovectorizer kick in.
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

  // Shared out-of-range policy cascade. Order: clamp/wrap/sentinel/error
  // *actions*, then clamp/wrap *policy* bits, then `domain_fail`. The four
  // callers-supplied callables cover how clamp/wrap store, the sentinel-action
  // value, and the error-message rhs view. `Wrappable` is false on the fractional
  // path (no wrap *action* branch). Returns true when a handler resolved the write.
  template <bool Wrappable, boundable L, typename P, typename A,
            typename DoClamp, typename DoWrap, typename SentinelVal, typename MsgView>
  constexpr bool dispatch_out_of_range(L& lhs, P&& policy, A&& action,
                                       DoClamp do_clamp, DoWrap do_wrap,
                                       SentinelVal sentinel_val,
                                       [[maybe_unused]] MsgView msg_view)
  {
    using PA = plain<A>;
    if constexpr (clamp_action<PA>)
    { do_clamp(); return true; }
    else if constexpr (Wrappable && wrap_action<PA>)
    { do_wrap(); return true; }
    else if constexpr (sentinel_action<PA>)
    {
      lhs = L::from_raw(sentinel_raw<L>());
      action.fn(lhs, sentinel_val());
      return true;
    }
    else if constexpr (error_action<PA>)
    {
      action.fn(lhs, errc::domain_error, errc_message(errc::domain_error));
      return true;
    }
    else if constexpr (HasPolicy<L, P, clamp>)
    { do_clamp(); return true; }
    else if constexpr (HasPolicy<L, P, wrap>)
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
  // assign(boundable, integral)
  //---------------------------------------------------------------------------
  template <boundable L, std::integral R>
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
        if constexpr (clamp_action<plain<A>>)
          action.fn(lhs, overshoot);
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
          if constexpr (wrap_action<plain<A>>) action.fn(lhs, imax{0});
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
        if constexpr (wrap_action<plain<A>>)
          action.fn(lhs, excess);
      }

      template<typename P, typename A>
      static constexpr bool handle_out_of_range(L& lhs, R rhs, imax lower, imax upper,
                                                P&& policy, A&& action)
      {
        return dispatch_out_of_range<true>(lhs, policy, action,
          [&]{ apply_clamp(lhs, rhs, lower, upper, action); },
          [&]{ apply_wrap (lhs, rhs, lower, upper, action); },
          [&]{ return static_cast<imax>(rhs); },
          [&]{ return rhs; });
      }

      static constexpr void store(L& lhs, R rhs)
      {
        if constexpr (!index_raw<L>)
          lhs = L::from_raw(raw_cast<L>(rhs));
        else if constexpr (Lower<L> == Upper<L>)
          lhs = L::from_raw(0);   // notch_storage point grid: 0 is the only offset
        else if constexpr (HasQFormatFastPath<L>)
          lhs = L::from_raw(q_format_encode<L>(static_cast<imax>(rhs)));
        else // index storage, generic rational path
        {
          rational raw = ((rhs - Interval<L>.Lower)/Notch<L>).value();
          lhs = L::from_raw(raw_cast<L>(raw.Numerator / static_cast<umax>(raw.Denominator)));
        }
      }

    public:
      template<typename P, typename A = no_action>
      static constexpr L& assign(L& lhs, R const& rhs, P&& policy, A&& action = {})
      {
        static_assert(not excludes(Interval<L>, Interval<R>));

        // The out-of-range check runs unconditionally — clamp/wrap/sentinel
        // policies handle it via apply_*, which is constexpr-clean. Only the
        // unhandled-checked path winds up calling `policy.report`, which
        // contains its own `std::is_constant_evaluated()` guard.
        if constexpr (not includes(Interval<L>, Interval<R>))
        {
          if constexpr (IsIntegerInterval<L>)
          {
            // Skip the runtime range branch entirely when every handler would
            // be dead anyway — the dead branch otherwise inhibits autovec.
            if constexpr (needs_runtime_domain_check<L, plain<P>, plain<A>>)
            {
              constexpr imax lower = LowerImax<L>;
              constexpr imax upper = UpperImax<L>;
              if (static_cast<imax>(rhs) < lower || static_cast<imax>(rhs) > upper) [[unlikely]]
                if (handle_out_of_range(lhs, rhs, lower, upper, policy, action)) return lhs;
            }
          }
          else if (not includes(Interval<L>, rhs))
          {
            // Non-integer L bounds: route through the rational path so fractional
            // Lower/Upper drive clamp/sentinel/error correctly.
            return assignment<L, rational>::assign(lhs, rational{rhs}, policy, action);
          }
        }

        store(lhs, rhs);
        return lhs;
      }
  };

  //---------------------------------------------------------------------------
  // assign(boundable, floating_point | rational)
  //---------------------------------------------------------------------------
  template <boundable L, typename R>
    requires fractional<R>
  struct assignment<L,R>
  {
    private:
      template<typename P, typename A>
      static constexpr void apply_clamp(L& lhs, R rhs, P&&, A&& action)
      {
        R clamped = (rhs < Lower<L>) ? static_cast<R>(Lower<L>) : static_cast<R>(Upper<L>);
        R overshoot;
        if constexpr (std::same_as<R, rational>)
          overshoot = (rhs - clamped).value_or(rational{0});
        else
          overshoot = rhs - clamped;

        // The clamp target is an interval endpoint — a grid point — so the slot is 0
        // or NotchCount, no rounding. real takes the endpoint as a double, rational
        // the exact constant (a double round-trip would lose non-dyadic endpoints);
        // raw_from_offset<L> adds Lower back for direct-encoded storage.
        if constexpr (fp_raw<L>)
          lhs = L::from_raw((rhs < Lower<L>) ? static_cast<double>(Lower<L>)
                                             : static_cast<double>(Upper<L>));
        else if constexpr (rational_raw<L>)
          lhs = L::from_raw((rhs < Lower<L>) ? Lower<L> : Upper<L>);
        else
          lhs = L::from_raw(raw_from_offset<L>(
              (rhs < Lower<L>) ? umax{0} : NotchCount<L>));

        if constexpr (clamp_action<plain<A>>)
          action.fn(lhs, overshoot);
      }

    public:
      // Exposed (not private) so the boundable-rhs wrap path can reuse the
      // rational specialization's modular wrap on fractional/notch grids, and
      // so the wrap path can reuse store_checked after computing the wrapped
      // value.
      //
      // apply_wrap for real R — modular reduction into [Lower, Lower + range)
      // followed by store_checked so the rounding policy still applies if rhs
      // doesn't land on a notch after wrapping. range = Upper - Lower + Notch.
      template<typename P, typename A>
      static constexpr void apply_wrap(L& lhs, R rhs, P&& policy, A&& action)
      {
        rational rhs_r{rhs};
        rational lower_r = Lower<L>;
        rational range   = ((Upper<L> - lower_r).value() + Notch<L>).value();
        // q = floor((rhs - lower) / range), wrapped = rhs - q * range
        rational shifted = (rhs_r - lower_r).value();
        imax q = floor((shifted / range).value());
        rational wrapped = (rhs_r - (rational{q} * range).value()).value();

        // Re-enter the rational-rhs specialization for the actual store so the
        // notch / rounding policy logic is exercised once.
        assignment<L, rational>::store_checked(lhs, wrapped, policy, action);

        if constexpr (wrap_action<plain<A>>)
          action.fn(lhs, q);
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
      struct wide_quotient { umax slot; umax remainder; umax divisor; bool ok; };

      static constexpr wide_quotient wide_offset_quotient(rational const& rv)
      {
        if constexpr (Notch<L> == 0)
          return {};                       // continuous grids never index slots
        else
        {
          constexpr umax n_l     = Lower<L>.Numerator;
          constexpr umax a_dl    = abs_den(Lower<L>.Denominator);
          constexpr bool low_neg = Lower<L>.Denominator < 0;
          constexpr umax n_n     = Notch<L>.Numerator;   // Notch > 0: d_n > 0
          constexpr umax d_n     = static_cast<umax>(Notch<L>.Denominator);

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
              offset = u128{val.hi + low.hi + (val.lo + low.lo < val.lo ? 1u : 0u),
                            val.lo + low.lo};
            else if (!rhs_neg && !low_neg)
            {
              if (cmp128(val, low) < 0) return {};         // rhs < Lower
              offset = u128{val.hi - low.hi - (val.lo < low.lo ? 1u : 0u),
                            val.lo - low.lo};
            }
            else if (rhs_neg && low_neg)
            {
              if (cmp128(low, val) < 0) return {};         // rhs < Lower
              offset = u128{low.hi - val.hi - (low.lo < val.lo ? 1u : 0u),
                            low.lo - val.lo};
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
            if (dividend.overflowed)
              return {};

            const divmod128_result qr = divmod128(dividend.value, divisor);
            if (qr.quotient.hi != 0)
              return {};                    // slot beyond any 64-bit index space
            return {qr.quotient.lo, qr.remainder, divisor, true};
          }
        }
      }

      template<typename P, typename A = no_action>
      static constexpr bool store_checked(L& lhs, R rhs, P&& policy, A&& action = {})
      {
        if constexpr (rational_raw<L> && Notch<L> == 0)
        { lhs = L::from_raw(rhs); return true; }   // continuous: store verbatim
        else if constexpr (fp_raw<L>)
        {
          // real target: raw IS the value — snap to the dyadic grid (range handling
          // already ran in the assign cascade; finite guard mirrors store_f64's).
          const double v = static_cast<double>(rhs);
          if (!(v - v == 0))
            detail::raise(errc::not_finite, "non-finite double");
          lhs = L::from_raw(Grid<L>.snap_double(v));
          return true;
        }
        else if constexpr (Lower<L> == Upper<L>)
        {
          // Singleton grid: offset encoding → Raw=0; rational/direct → Raw = Lower.
          if constexpr (rational_raw<L>)
            lhs = L::from_raw(Lower<L>);
          else if constexpr (!index_raw<L>)
            lhs = L::from_raw(raw_cast<L>(RawLo<L>));
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
              lhs = L::from_raw((Lower<L> + (rational{k} * Notch<L>).value()).value());
            else
              lhs = L::from_raw(raw_from_offset<L>(k));
          };

          constexpr bool has_round_flag =
               HasPolicy<L, P, round_nearest> || HasPolicy<L, P, round_floor>
            || HasPolicy<L, P, round_ceil>    || HasPolicy<L, P, round_half_even>
            || HasPolicy<L, P, snap>;

          // Q-format integer shortcut: with integer Lower and notch 1/K the offset is
          // (num − Lo·aden)·(K/g) / (aden/g), g = gcd(aden, K) — one gcd + integer ops
          // instead of two rational ops. round_quotient is invariant under reduction,
          // so the slot is bit-identical to the rational path. Oversized denominators
          // fall through (the kMaxDen guard keeps every product inside imax).
          if constexpr (HasQFormatFastPath<L> && !fp_raw<L> && Notch<L> != 0)
          {
            constexpr imax K  = abs_den(Notch<L>.Denominator);
            constexpr imax Lo = LowerImax<L>;
            constexpr umax kKM = []{
              // 2 · K · M with saturation (M bounds |value| and the offset span)
              umax k = static_cast<umax>(K);
              umax m = static_cast<umax>(
                  ceil(((bnd::detail::abs(Lower<L>) > bnd::detail::abs(Upper<L>)
                      ? bnd::detail::abs(Lower<L>) : bnd::detail::abs(Upper<L>))
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
              const imax num  = (rv.Denominator < 0) ? -rv.Numerator : rv.Numerator;
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
          // that envelope reports errc::overflow — never a nullopt deref,
          // which would escape noexcept callers (the math engines) as
          // terminate. Rounding here is the offset rule (round_offset), the
          // same semantics round_quotient falls back to past 64 bits.
          const auto quotient = (rhs - Lower<L>)/Notch<L>;
          if (!quotient.has_value()) [[unlikely]]
          {
            const wide_quotient wide = wide_offset_quotient(rational{rhs});
            if (!wide.ok)
            {
              if constexpr (error_action<plain<A>>)
              { action.fn(lhs, errc::overflow, errc_message(errc::overflow)); return false; }
              policy.report(errc::overflow);
              return false;
            }
            if (wide.remainder == 0)
            { store_slot(wide.slot); return true; }
            if constexpr (has_round_flag)
            { store_slot(round_offset<L, P>(wide.slot, wide.remainder, wide.divisor)); return true; }
            if (policy.round_check()) [[unlikely]]
            {
              if constexpr (error_action<plain<A>>)
              { action.fn(lhs, errc::rounding_error, errc_message(errc::rounding_error)); return false; }
              policy.report(errc::rounding_error);
              return false;
            }
            store_slot(round_offset<L, P>(wide.slot, wide.remainder, wide.divisor));
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
            if constexpr (error_action<plain<A>>)
            { action.fn(lhs, errc::rounding_error, errc_message(errc::rounding_error)); return false; }
            policy.report(errc::rounding_error);
            return false;
          }
          else
            store_slot(round_quotient<L, P>(raw.Numerator, den));
          return true;
        }
      }

      template<typename P, typename A = no_action>
      static constexpr L& assign(L& lhs, R const& rhs, P&& policy, A&& action = {})
      {
        if (not includes(Interval<L>, rhs)) [[unlikely]]
        {
          // Fractional path has no wrap *action* branch (Wrappable = false).
          if (dispatch_out_of_range<false>(lhs, policy, action,
                [&]{ apply_clamp(lhs, rhs, policy, action); },
                [&]{ apply_wrap (lhs, rhs, policy, action); },
                [&]{ return rhs; },
                [&]{ return rhs; }))
            return lhs;
        }

        store_checked(lhs, rhs, policy, action);
        return lhs;
      }
  };

  //---------------------------------------------------------------------------
  // assign(boundable, boundable)
  //---------------------------------------------------------------------------
  template <boundable L, boundable R>
  struct assignment<L,R>
  {
    private:
      // Offset/Factor map rhs.Raw → lhs.Raw via `lhs.Raw = Factor·rhs.Raw + Offset`.
      // Branches: L rational (pass value through), R rational (pre-divide by
      // Notch<L>), both integer (the hot path, collapses to integer math).
      static constexpr rational calcOffset()
      {
        if constexpr (rational_raw<L>)
          return Lower<R>;
        else if constexpr (Notch<L> == 0)
          // Continuous fp_raw L: no grid to land on, mapping unused (store
          // routes through snap_double). 0 avoids the /Notch<L> divide-by-zero.
          return rational{0};
        else if constexpr (rational_raw<R>)
          return -(Lower<L>/Notch<L>).value();
        else
          return ((Lower<R> - Lower<L>)/Notch<L>).value();
      }

      static constexpr rational calcFactor()
      {
        if constexpr (rational_raw<L>)
          return Notch<R>;
        else if constexpr (Notch<L> == 0)
          // Continuous fp_raw L (see calcOffset). A denominator-1 Factor also
          // makes assign_notch_ok vacuously true (any value representable).
          return rational{0};
        else if constexpr (rational_raw<R>)
          return (rational{1}/Notch<L>).value();
        else
          return (Notch<R>/Notch<L>).value();
      }

    public:
      static constexpr rational Offset = calcOffset();
      static constexpr rational Factor = calcFactor();

      // Raw-space integer-only mapping — requires integer raw storage on both
      // sides (not rational, not real).
      static constexpr bool is_integer_mapping =
          !rational_raw<L> && !rational_raw<R>
          && !fp_raw<L> && !fp_raw<R>
          && abs_den(Factor.Denominator) == 1 && abs_den(Offset.Denominator) == 1;

      // Map rhs.Raw into L's raw space (requires is_integer_mapping). The
      // Offset/Factor formula assumes offset encoding both sides; for direct
      // storage, subtract Lower<R> first (R-value → R-offset) and add Lower<L>
      // after (raw_from_offset<L>). All integer (is_integer_mapping guarantees it).
      static constexpr imax map_raw(auto rhs_raw)
      {
        imax r_offset = rhs_raw;
        if constexpr (!index_raw<R>)
          r_offset -= RawLo<R>;

        // Offset is an exact integer here, so trunc(Offset) is a constexpr constant.
        imax l_offset = static_cast<imax>(Factor.Numerator) * r_offset + trunc(Offset);

        if constexpr (!index_raw<L>)
          return l_offset + RawLo<L>;
        else
          return l_offset;
      }

    private:
      // Grid of the wrap "excess"/carry handed to an on_wrap action:
      // floor((value − Lower) / range) for value ∈ R's interval (range = span + notch).
      // Both operands are bounds, so — like the clamp overshoot — the carry has a
      // known range and is delivered as a bound, not a raw imax.
      static constexpr grid wrap_excess_grid()
      {
        constexpr rational range = ((Upper<L> - Lower<L>).value() + Notch<L>).value();
        return grid{ floor(((Lower<R> - Lower<L>).value() / range).value()),
                     floor(((Upper<R> - Lower<L>).value() / range).value()) };
      }

      template<typename A>
      static constexpr void apply_clamp(L& lhs, R const& rhs, A&& action)
      {
        // RawLo/RawHi are already the correct Raw (no raw_from_offset). Real storage
        // takes the endpoint as a double (RawLo/Hi truncate fractional dyadic endpoints).
        if constexpr (fp_raw<L>)
          lhs = L::from_raw((as_rational(rhs) < Lower<L>)
            ? static_cast<double>(Lower<L>) : static_cast<double>(Upper<L>));
        else
          lhs = L::from_raw((as_rational(rhs) < Lower<L>)
            ? raw_cast<L>(RawLo<L>) : raw_cast<L>(RawHi<L>));
        // Overshoot (rhs − clamped) as a bound, via the result-grid inference of normal
        // bound arithmetic: both operands are bounds, so the overshoot is too. It is always
        // in-grid and on-notch for Grid<R> − Grid<L>, so the construction is exact.
        if constexpr (clamp_action<plain<A>>)
        {
          constexpr grid OG = (Grid<R> - Grid<L>).value();
          bnd::bound<OG> overshoot{ (as_rational(rhs) - as_rational(lhs)).value() };
          action.fn(lhs, overshoot);
        }
      }

      template<typename P, typename A>
      static constexpr void apply_wrap(L& lhs, R const& rhs, P&& policy, A&& action)
      {
        // The integer modular wrap (range = Upper - Lower + 1, integer values) is
        // only correct on a unit-integer grid — notch 1 with integer bounds, so
        // consecutive integers are adjacent grid points. Any other grid (fractional
        // notch, non-integer bounds) routes through the rational modular wrap.
        if constexpr (IsIntegerInterval<L> && abs_den(Notch<L>.Denominator) == 1
                      && Notch<L>.Numerator == 1)
        {
          // Unit-integer fast path: modular wrap on the integer value.
          imax rhs_imax = trunc(as_rational(rhs));
          constexpr imax lower = LowerImax<L>;
          constexpr imax upper = UpperImax<L>;
          imax range = upper - lower + 1;
          imax shifted = rhs_imax - lower;
          imax wrapped = ((shifted % range) + range) % range;
          imax excess  = (shifted < 0) ? ((shifted - range + 1) / range) : (shifted / range);
          from_value(lhs, wrapped + lower);
          if constexpr (wrap_action<plain<A>>)
            action.fn(lhs, bnd::bound<wrap_excess_grid()>{excess});   // carry as a bound
        }
        else if constexpr (wrap_action<plain<A>>)
        {
          // Fractional destination with a wrap action: reuse the rational modular-wrap
          // path for the store/rounding, but wrap its imax carry `q` into a bound before
          // handing it to the user action.
          assignment<L, rational>::apply_wrap(lhs, as_rational(rhs), policy,
            bnd::on_wrap([&](auto& self, imax q){
              action.fn(self, bnd::bound<wrap_excess_grid()>{q});
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
          [&]{ return as_rational(rhs); },
          [&]{ return as_rational(rhs); });
      }

      template<typename P>
      static constexpr void store(L& lhs, R const& rhs, P&&)
      {
        if constexpr (fp_raw<L>)
          // real target: raw IS the value — decode the source and snap to the dyadic
          // grid (the offset machinery below mis-encodes a double raw).
          lhs = L::from_raw(Grid<L>.snap_double(as_double(rhs)));
        else if constexpr (is_integer_mapping)
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
          // Round the L-offset to a notch index in VALUE space via round_quotient
          // (same as the scalar path), honouring every rounding mode.
          umax q = round_quotient<L, P>(rat.Numerator, ad);
          // rat is the L-offset; raw_from_offset<L> adds Lower<L> back for direct storage.
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
        static_assert(HasPolicy<L, P, wrap> || HasPolicy<L, P, clamp>
                      || not excludes(Interval<L>, Interval<R>),
          "rhs interval lies entirely outside lhs interval and the policy cannot bring it into range");
        static_assert(abs_den(Factor.Denominator) == 1 || HasPolicy<L, P, snap>
                      || point_exactly_assignable<L, R>,
          "incompatible notches: use with_snap() or policy<snap>() to allow rounding");

        if constexpr (not includes(Interval<L>, Interval<R>))
        {
          if constexpr (needs_runtime_domain_check<L, plain<P>, plain<A>>)
          {
            if constexpr (is_integer_mapping)
            {
              if (imax mapped = map_raw(rhs.raw()); mapped < RawLo<L> || mapped > RawHi<L>)
                if (try_clamp_or_fail(lhs, rhs, policy, action)) return lhs;
            }
            else if (not includes(Interval<L>, as_rational(rhs)))
              if (try_clamp_or_fail(lhs, rhs, policy, action)) return lhs;
          }
        }

        store(lhs, rhs, policy);
        return lhs;
      }
  };
} // namespace bnd::detail

#endif // BNDassignmentHPP
