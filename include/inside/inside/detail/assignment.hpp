// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INSIDE_INSIDE_DETAIL_ASSIGNMENT_HPP
#define INSIDE_INSIDE_DETAIL_ASSIGNMENT_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/inside/detail/assign_exact.hpp> // the exact store and the out-of-range cascade
#include <inside/inside/generic.hpp>
#include <inside/inside/detail/wide_value.hpp>
#include <inside/inside/grid.hpp>

#include <bit>
#include <utility>

namespace ins::detail
{
  // assignment: narrowing and coercion between bounded and arithmetic types.
  // Three specializations dispatch on the source (integral, fractional, insidable);
  // each stores an in-range value and hands an out-of-range one to the policy
  // (apply_clamp, apply_wrap, or a report).
  // The insidable path also exposes is_integer_mapping and map_raw, a pure-integer formula in the hot path.

  // The integer folds of a wrap without 128-bit steps:
  // a Q format's slot (rhs − Lower)·K after a reduction by the period,
  // and the lattice codec's (n·K − M)/S after reducing n by S·period. Each product is checked to fit.
  template <insidable L, typename R>
  inline constexpr bool q_format_fold = []
  {
    if constexpr (!q_format_fits<L> || wide_builtin<R> || (std::is_unsigned_v<R> && sizeof(R) >= 8))
      return false;
    else
    {
      std::int64_t p;
      return max_index_of<L> < (std::uint64_t{1} << 62)
          && !mul_overflow(slot_period<L>, static_cast<std::int64_t>(abs_den(notch64<L>.Den)), &p);
    }
  }();
  template <insidable L>
  inline constexpr bool codec_fold = []
  {
    if constexpr (!lattice_codec<L>.Ok)
      return false;
    else
    {
      constexpr auto c{lattice_codec<L>};
      std::int64_t   np, x;
      return max_index_of<L> < (std::uint64_t{1} << 62) && !mul_overflow(c.S, slot_period<L>, &np)
          && !mul_overflow(np, c.K, &x) && x < (std::int64_t{1} << 61) - c.M;
    }
  }();

  // assign(insidable, integral)
  template <insidable L, std::integral R>
  struct assignment<L, R>
  {
  private:
    // Exact integer arithmetic for the bounds and the cold clamp and wrap paths:
    // an integer interval may reach past int64 on either side (e.g. {0, 2^64−1}), and rhs − Lower may need 65 bits.
    using fold = unit_fold<L, source_lo<R>, source_hi<R>>;
    using W    = typename fold::W;

    // Hot-path range test. Bounds within int64 compare as plain integers;
    // a 64-bit unsigned source can exceed int64 and compares sign-safe instead.
    // (Not std::cmp_* throughout: GCC then misses folding lo ≤ x ≤ hi into one unsigned compare.)
    // Bounds past int64 compare exactly in the fold's work type.
    static constexpr bool out_of_range(R rhs) noexcept
    {
      if constexpr (fits_int64(interval_of<L>) && !wide_builtin<R>)
      {
        constexpr std::int64_t lower{lower_int64<L>}, upper{upper_int64<L>};
        if constexpr (std::is_unsigned_v<R> && sizeof(R) >= sizeof(std::int64_t))
          return std::cmp_less(rhs, lower) || std::cmp_greater(rhs, upper);
        else
          return static_cast<std::int64_t>(rhs) < lower || static_cast<std::int64_t>(rhs) > upper;
      }
      else
      {
        const W v{static_cast<W>(rhs)};
        return v < fold::lower || fold::upper < v;
      }
    }

    template <typename A>
    static constexpr void apply_clamp(L& lhs, R rhs, A&& action)
    {
      // Pre: rhs is out of [Lower, Upper] (only called from handle_out_of_range),
      // so the two-way pick is the full clamp.
      const W    v{static_cast<W>(rhs)};
      const bool low{v < fold::lower};
      lhs = L::from_raw(raw_of_slot<L>(low ? W{0} : fold::span), unsafe);
      if constexpr (clamp_action<plain_t<A>>) // the source minus the limit, exact
        action.Fn(lhs, make_clamp_overshoot<L, R>(exact_of_source(rhs) + -exact_of(lhs)));
    }

    template <typename A>
    static constexpr void apply_wrap(L& lhs, R rhs, A&& action)
    {
      // Modular wrap on the exact offset rhs − Lower into span + 1 slots, and the exact carry (wrap_carry_grid).
      const auto [carry, w] = fold::fold(static_cast<W>(rhs));
      lhs = L::from_raw(raw_of_slot<L>(w), unsafe);
      if constexpr (wrap_action<plain_t<A>>)
        action.Fn(lhs, make_wrap_carry<L, R>(carry));
    }

    template <typename P, typename A>
    static constexpr bool handle_out_of_range(L& lhs, R rhs, P&& policy, A&& action)
    {
      return dispatch_out_of_range<true>(
        lhs, policy, action, [&] { apply_clamp(lhs, rhs, action); }, [&] { apply_wrap(lhs, rhs, action); });
    }

    static constexpr void store(L& lhs, R rhs)
    {
      if constexpr (value_storage<L>)
        lhs = L::from_raw(raw_cast<L>(rhs), unsafe);
      else if constexpr (point_grid<L>)
        lhs = L::from_raw(0, unsafe); // a point grid: 0 is the only offset
      else if constexpr (q_format_fits<L>)
        lhs = L::from_raw(q_format_encode<L>(static_cast<std::int64_t>(rhs)), unsafe);
      else
      {
        // Index storage on a notch 1/K grid over an integer Lower:
        // the offset (rhs − Lower)·K of an in-range rhs is below 2^64,
        // so uint64 steps, exact modulo 2^64, give it exactly.
        constexpr std::uint64_t K{static_cast<std::uint64_t>(abs_den(notch64<L>.Den))};
        constexpr std::uint64_t lo{lower64<L>.Den < 0 ? std::uint64_t{0} - lower64<L>.Num
                                                      : lower64<L>.Num}; // Lower mod 2^64
        lhs = L::from_raw(raw_from_offset<L>((static_cast<std::uint64_t>(rhs) - lo) * K), unsafe);
      }
    }

  public:
    // An integer lands on L's grid whenever the notch is 1/K over an integer Lower (or the grid is continuous);
    // otherwise it may fall between notches and must round or report exactly like the same value given as a
    // rational.
    static constexpr bool integers_on_grid = !notched<L> || (notch64<L>.Num == 1 && abs_den(lower64<L>.Den) == 1);

    template <typename P, typename A = no_action>
    static constexpr L& assign(L& lhs, const R& rhs, P&& policy, A&& action = {})
    {
      // wrap and clamp bring any value into range (matches assign_intervals_ok).
      static_assert(uses_flag<L, P, wrap> || uses_flag<L, P, clamp> || not excludes(interval_of<L>, interval_of<R>),
                    "rhs type's range lies entirely outside lhs interval and the policy cannot bring it into range");

      // (Separate arms: naming integers_on_grid instantiates L's 64-bit views.)
      // A source past 64 bits (__int128) has no 64-bit rational: off L's integers it stores exactly.
      if constexpr (wide_valued<L>)
        return assign_exact<R>(lhs, exact_of(rhs), policy, std::forward<A>(action));
      else if constexpr (wide_builtin<R> && !integers_on_grid)
        return assign_exact<R>(lhs, exact_of(rhs), policy, std::forward<A>(action));
      else if constexpr (!integers_on_grid && lattice_codec<L>.Ok && !std::is_unsigned_v<R>)
      {
        // A lattice an integer may miss (a notch of 60 or 7/2, an unanchored one):
        // the slot offset (rhs·K − M)/S, one division by the constant S.
        // Only an rhs past the limits (which a rounding policy may still bring onto the grid) takes the general path.
        constexpr auto         c{lattice_codec<L>};
        constexpr std::int64_t nlo{static_cast<std::int64_t>(ceil(lower64<L>))};
        constexpr std::int64_t nhi{static_cast<std::int64_t>(floor(upper64<L>))};
        const std::int64_t     n{static_cast<std::int64_t>(rhs)};
        if (n < nlo || n > nhi) [[unlikely]]
        {
          // Past a limit: a clamp stores the endpoint (rounding first would land there too);
          // a wrap folds an exact slot.
          if constexpr (uses_flag<L, P, clamp> && !clamp_action<plain_t<A>> && !wrap_action<plain_t<A>>)
          {
            lhs = L::from_raw(endpoint_raw<L>(n < nlo), unsafe);
            return lhs;
          }
          else if constexpr (uses_flag<L, P, wrap> && !clamp_action<plain_t<A>> && !wrap_action<plain_t<A>>
                             && codec_fold<L>)
          {
            // n and n + S·period land on the same slot: reduce n by j whole S·periods first.
            // The slot (n·K − M)/S moves by j·period·K, a whole number of periods, so the fold is the same;
            // a rounding takes n's sign and the parity of the unreduced floor.
            constexpr std::int64_t np{c.S * slot_period<L>};
            std::int64_t           j{n / np}, nr{n % np};
            if (nr < 0)
            {
              nr += np;
              --j;
            }
            const std::int64_t onum{nr * c.K - c.M};
            std::int64_t       q{onum / c.S}, r{onum % c.S};
            if (r < 0)
            {
              r += c.S;
              --q;
            }
            if (r == 0)
            {
              lhs = L::from_raw(raw_from_offset<L>(fold_slot<L>(q)), unsafe);
              return lhs;
            }
            if constexpr (uses_flag<L, P, snap>)
            {
              constexpr round_mode m{rounding_for<L, P>};
              constexpr bool       step_odd{((slot_period<L> * c.K) & 1) != 0};
              const bool odd{(((q ^ (step_odd ? j : std::int64_t{0})) & 1) != 0) != lower_index_odd<L>};
              q += rounds_up(
                m,
                n < 0,
                classify_remainder(m, static_cast<std::uint64_t>(r), static_cast<std::uint64_t>(c.S)),
                odd);
              lhs = L::from_raw(raw_from_offset<L>(fold_slot<L>(q)), unsafe);
              return lhs;
            }
          }
          return assignment<L, rational>::assign(lhs, rational{rhs}, policy, std::forward<A>(action));
        }
        const std::uint64_t onum{static_cast<std::uint64_t>(n * c.K - c.M)}; // ≥ 0: n ≥ Lower
        const std::uint64_t q{onum / static_cast<std::uint64_t>(c.S)}, r{onum % static_cast<std::uint64_t>(c.S)};
        if (r == 0) [[likely]]
          lhs = L::from_raw(raw_from_offset<L>(q), unsafe);
        else if (!uses_flag<L, P, snap> && policy.checks_rounding())
          report_failure(lhs, policy, action, error::rounding_error);
        else // rounded by the policy (unsafe without a mode: toward zero)
          lhs = L::from_raw(raw_from_offset<L>(round_split<L, P>(q, r, static_cast<std::uint64_t>(c.S))), unsafe);
        return lhs;
      }
      else if constexpr (!integers_on_grid)
        return assignment<L, rational>::assign(lhs, rational{rhs}, policy, std::forward<A>(action));
      else
      {
        // The out-of-range check runs unconditionally: the clamp and wrap policies handle it through apply_*,
        // which is constexpr-clean.
        // Only the unhandled checked path ends up calling policy.report, which has its own
        // std::is_constant_evaluated() guard.
        if constexpr (not includes(interval_of<L>, interval_of<R>))
        {
          if constexpr (integer_limits<L>)
          {
            // Skip the run-time range branch entirely when every handler would be dead anyway:
            // the dead branch otherwise inhibits autovectorization.
            if constexpr (needs_runtime_range_check<L, plain_t<P>, plain_t<A>>)
            {
              if (out_of_range(rhs)) [[unlikely]]
              {
                // The integer clamp and wrap formulas need consecutive integers to be adjacent grid points (notch 1);
                // a finer notch wraps modulo span + notch on the rational path.
                if constexpr (notch64<L> == 1)
                {
                  if (handle_out_of_range(lhs, rhs, policy, action))
                    return lhs;
                }
                else if constexpr (q_format_fold<L, R> && !clamp_action<plain_t<A>> && !wrap_action<plain_t<A>>
                                   && (uses_flag<L, P, clamp> || uses_flag<L, P, wrap>))
                {
                  // A 1/K notch over integer limits: the clamp is an endpoint,
                  // and the fold is on the slot (rhs − Lower)·K, reduced by the period first so it stays in int64.
                  if constexpr (uses_flag<L, P, clamp>)
                    lhs = L::from_raw(endpoint_raw<L>(std::cmp_less(rhs, lower_int64<L>)), unsafe);
                  else
                  {
                    constexpr std::int64_t k = abs_den(notch64<L>.Den);
                    const std::int64_t     t{static_cast<std::int64_t>(fold_slot<L>(
                      static_cast<std::int64_t>(static_cast<std::uint64_t>(rhs)
                                                - static_cast<std::uint64_t>(lower_int64<L>))))};
                    lhs = L::from_raw(raw_from_offset<L>(fold_slot<L>(t * k)), unsafe);
                  }
                  return lhs;
                }
                else if constexpr (wide_builtin<R>)
                  return assign_exact<R>(lhs, exact_of(rhs), policy, action);
                else
                  return assignment<L, rational>::assign(lhs, rational{rhs}, policy, action);
              }
            }
          }
          else if constexpr (wide_builtin<R>)
          {
            return assign_exact<R>(lhs, exact_of(rhs), policy, action); // fractional limits: exactly
          }
          else if (not includes(interval_of<L>, rhs))
          {
            // Limits that are no integers:
            // the rational path, so a fractional Lower or Upper drives the clamp or the error correctly.
            return assignment<L, rational>::assign(lhs, rational{rhs}, policy, action);
          }
        }

        store(lhs, rhs);
        return lhs;
      }
    }
  };

  // The largest denominator a fraction stored onto the Q format L may have for the integer shortcut
  // (store_checked): (num − Lo·den)·K stays in int64. 0 when no denominator fits.
  template <insidable L>
  inline constexpr std::uint64_t q_format_max_den = []
  {
    if constexpr (!q_format_fits<L> || !notched<L>)
      return std::uint64_t{0};
    else
    {
      constexpr std::int64_t  K = abs_den(notch64<L>.Den);
      constexpr std::uint64_t two_km = []
      {
        // 2·K·M with saturation (M bounds |value| and the offset span),
        // the ceiling in uint64: a bound's magnitude may reach 2^63 (past int64).
        constexpr std::uint64_t uint64_hi{std::numeric_limits<std::uint64_t>::max()};
        const rational          a{abs(lower64<L>) > abs(upper64<L>) ? abs(lower64<L>) : abs(upper64<L>)};
        const std::uint64_t     c{round_magnitude(a, round_mode::ceil).Magnitude};
        if (c > (uint64_hi - 2) / 2)
          return uint64_hi;
        std::uint64_t k{static_cast<std::uint64_t>(K)};
        std::uint64_t m{c * 2 + 2};
        if (k > uint64_hi / m)
          return uint64_hi;
        std::uint64_t prod{k * m};
        return prod > uint64_hi / 2 ? uint64_hi : prod * 2;
      }();
      return static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) / two_km;
    }
  }();

  // assign(insidable, floating_point | rational)
  template <insidable L, typename R>
    requires fractional<R>
  struct assignment<L, R>
  {
  private:
    // A floating source with |rhs| ≥ 2^64 lies outside every grid and has no rational form
    // (rational(double) would fail): such values are decided by their sign.
    static constexpr bool huge(const R& rhs) noexcept
    {
      if constexpr (std::floating_point<R>)
        return past_64_bits(rhs);
      else
        return false;
    }

    // (Out of line, by value, for a double:
    // its exact comparison has a cold tail whose call would otherwise cost the inlined store registers.)
    [[gnu::noinline]] static constexpr bool below_lower(R rhs)
      requires std::floating_point<R>
    {
      return huge(rhs) ? rhs < 0 : rhs < lower64<L>;
    }
    static constexpr bool below_lower(const R& rhs)
      requires(!std::floating_point<R>)
    {
      return rhs < lower64<L>;
    }

    template <typename P, typename A>
    static constexpr void apply_clamp(L& lhs, R rhs, P&&, A&& action)
    {
      const bool low{below_lower(rhs)};
      // The clamp target is an interval endpoint: a grid point, no rounding.
      lhs = L::from_raw(endpoint_raw<L>(low), unsafe);

      if constexpr (clamp_action<plain_t<A>>) // the source minus the limit, exact
      {
        if constexpr (std::floating_point<R>)
          if (!is_finite(rhs))
          {
            action.Fn(lhs, infinite_clamp_overshoot<L, R>(low));
            return;
          }
        action.Fn(lhs, make_clamp_overshoot<L, R>(exact_of_source(rhs) + -exact_of(lhs)));
      }
    }

  public:
    // Public, so the insidable-rhs wrap path can reuse the rational specialization's modular wrap on fractional or
    // notched grids, and the wrap path can reuse store_checked after computing the wrapped value.
    // apply_wrap for a fractional R: rounding onto the lattice, then modular reduction into [Lower, Lower + range),
    // range = Upper - Lower + Notch.
    template <typename P, typename A>
    static constexpr void apply_wrap(L& lhs, R rhs, P&& policy, A&& action)
    {
      // Round onto the lattice first (by the policy, like every other store), then fold:
      // an on-lattice value folds onto a grid point, so rounding can never carry it past Upper.
      // A floating source of 2^63 or more has no rational form:
      // it folds exactly, like every value whose rational steps overflow below.
      if constexpr (std::floating_point<R>)
        if (!(rhs < 0x1p63 && rhs > -0x1p63)) [[unlikely]]
          return wrap_exact(lhs, exact_of_double(static_cast<double>(rhs)), policy, action);
      rational rhs_r{rhs};
      if constexpr (!notched<L> && grid_of<L>.MaxDenominator != 0 && uses_flag<L, P, snap>)
      {
        // A bounded continuous grid rounds onto its bound first, too:
        // a fold keeps the denominator, so the folded value stays on it.
        // (L's bound fits int64, and a Farey neighbour p/q of N/d with q ≤ bound < d has p ≤ N:
        // a 64-bit rational.)
        rhs_r = rational_of(round_to_bound<L, plain_t<P>>(exact_of(rhs_r)));
      }
      else if constexpr (uses_flag<L, P, snap>)
      {
        const auto r{try_round_to_lattice<L, P>(rhs_r)};
        if (!r) [[unlikely]]
          return wrap_exact(lhs, exact_of(rhs_r), policy, action);
        rhs_r = *r;
      }
      // The store itself re-enters the rational-rhs specialization, so the notch and rounding logic runs once;
      // then the carry.
      auto finish = [&](const rational& folded, const auto& q)
      {
        assignment<L, rational>::store_checked(lhs, folded, policy, action);
        if constexpr (wrap_action<plain_t<A>>)
          action.Fn(lhs, make_wrap_carry<L, R>(q));
      };
      if (abs_den(rhs_r.Den) == 1 && notch64<L> == 1 && abs_den(lower64<L>.Den) == 1)
      {
        // An integer value on a unit lattice: fold exactly (the grid may span 2^64 values, past the rational range).
        using fold = unit_fold<L, source_lo<rational>, source_hi<rational>>;
        using W    = typename fold::W;
        const auto [q, w] = fold::fold(static_cast<W>(grid_numerator(rhs_r)));
        const W v{fold::lower + w};
        return finish(
          v < W{0} ? -rational{static_cast<std::uint64_t>(-v)} : rational{static_cast<std::uint64_t>(v)}, q);
      }
      // q = floor((rhs - lower) / range), wrapped = rhs - q * range.
      // A step past the 64-bit rational range, or a fold count past int64, folds exactly instead.
      const auto span {try_sub(upper64<L>, lower64<L>)};
      const auto range{span ? try_add(*span, notch64<L>) : span};
      const auto off  {try_sub(rhs_r, lower64<L>)};
      const auto quot {(range && off) ? try_div(*off, *range) : off};
      if (!range || !quot || *quot < rational{std::numeric_limits<std::int64_t>::min()}
          || *quot > rational{std::numeric_limits<std::int64_t>::max()}) [[unlikely]]
        return wrap_exact(lhs, exact_of(rhs_r), policy, action);
      const std::int64_t q{floor(*quot)};
      const auto         qr{try_mul(rational{q}, *range)};
      const auto         w{qr ? try_sub(rhs_r, *qr) : qr};
      if (!w) [[unlikely]]
        return wrap_exact(lhs, exact_of(rhs_r), policy, action);
      finish(*w, q);
    }

    // The fold done exactly when its rational steps overflow (a double past 2^63, a far value on a fine grid):
    // the exact store's wrap, which rounds by the policy and folds the wide lattice index (and its exact carry).
    // A continuous L has no lattice to fold in wide integers: still overflow.
    // (A point cannot wrap at all: dispatch_out_of_range rejects it.)
    template <typename P, typename A, std::size_t K>
    [[gnu::cold]] [[gnu::noinline]] static constexpr void
    wrap_exact(L& lhs, const wide_rational<K>& v, P&& policy, A&& action)
    {
      if constexpr (integer_storage<L>)
        assign_exact<R>(lhs, v, policy, action);
      else
        report_failure(lhs, policy, action, error::overflow);
    }

    // Cold and out of line: the exact wide slot for a quotient past the 64-bit rational, kept off the hot store path.
    // It returns 16 bytes, not the 72-byte exact_index_result:
    // GCC charges the copy of a large return value against inlining the whole store (+17% instructions measured).
    struct wide_slot_result
    {
      std::uint64_t Slot;
      bool          Exact;
    };
    template <typename P>
    [[gnu::cold]] [[gnu::noinline]] static constexpr wide_slot_result wide_slot(rational v)
    {
      const exact_index_result r{exact_index<L, rounding_for<L, P>>(exact_of(v))};
      return {static_cast<std::uint64_t>(r.Index), r.Exact}; // in range: fits 64 bits
    }

    // The source as a rational, converted once.
    // A double finer than 2^-62 has no exact 64-bit rational (rational{d} truncates it toward zero):
    // exact comes back false, and the store takes its exact value through store_fine_double,
    // so it rounds, or reports rounding_error, by its true value.
    static constexpr rational source_of(const R& rhs, bool& exact)
    {
      if constexpr (std::floating_point<R>)
        return rational_of_double(static_cast<double>(rhs), exact);
      else
        return rational{rhs};
    }
    // fine_double_value's result, sized to return in registers
    // (a larger one goes through memory, and the stack protector into the hot path):
    // a rational raw marks a failure by a denominator no value has (0), with the error as numerator;
    // an integer raw carries the error beside it.
    struct fine_result
    {
      raw_t<L> Raw;
      error    Code;
      constexpr error code() const { return Code; }
    };
    struct fine_rational_result
    {
      rational Raw;
      constexpr error code() const { return Raw.Den == 0 ? static_cast<error>(Raw.Num) : error{}; }
    };
    using fine_result_t = std::conditional_t<rational_storage<L>, fine_rational_result, fine_result>;

    // P's flags, recording its first failure into an error instead.
    template <typename P>
    using recording_policy = call_policy<P::Flags, error_ref>;

    // Out of line, and by value (an empty action too), with P's flags only:
    // a reference to a caller's local (an error&) would make it escape the hot path's frame.
    // The store runs on a copy of the lower limit, and a failure is returned, not stored:
    // a failed store leaves lhs intact, and the caller's policy (or its on_error action) hears of it.
    template <typename P, typename A>
    [[gnu::cold]] [[gnu::noinline]] static constexpr fine_result_t fine_double_value(double rhs, A action)
    {
      L r = []
      {
        if constexpr (rational_storage<L>)
          return L::from_raw(lower64<L>, unsafe);
        else if constexpr (point_storage<L>)
          return L::from_raw(raw_t<L>{}, unsafe);
        else
          return L::from_raw(static_cast<raw_t<L>>(raw_lo_exact<L>), unsafe);
      }();
      error code{};
      // A recording on_error stands in for one (dispatch_out_of_range ranks it below clamp and wrap):
      // the order holds.
      if constexpr (error_action<plain_t<A>>)
        assign_exact<R>(r,
                        exact_of_double(rhs),
                        recording_policy<P>{code},
                        on_error([&code](L&, error c, auto&&) { code = c; }));
      else
        assign_exact<R>(r, exact_of_double(rhs), recording_policy<P>{code}, std::forward<A>(action));
      if constexpr (rational_storage<L>)
      {
        if (code == error{})
          return {r.raw()};
        rational failed{};
        failed.Num = static_cast<std::uint64_t>(code);
        failed.Den = 0;
        return {failed};
      }
      else
        return {r.raw(), code};
    }
    template <typename P, typename A>
    static constexpr void store_fine_double(L& lhs, double rhs, P&& policy, A&& action)
    {
      using Act = std::conditional_t<std::is_empty_v<plain_t<A>>, plain_t<A>, A&&>;
      const auto r{fine_double_value<call_policy<plain_t<P>::Flags>, Act>(rhs, std::forward<A>(action))};
      if (r.code() == error{}) [[likely]]
        lhs = L::from_raw(r.Raw, unsafe);
      else if constexpr (error_action<plain_t<A>>)
        action.Fn(lhs, r.code(), error_message(r.code()));
      else
        policy.report(r.code()); // records into an error&, or raises
    }

    template <typename P, typename A = no_action>
    static constexpr void store_checked(L& lhs, R rhs, P&& policy, A&& action = {})
    {
      if constexpr (rational_storage<L>)
      {
        bool           exact{true};
        const rational rv{source_of(rhs, exact)};
        if constexpr (std::floating_point<R>)
          if (!exact) [[unlikely]]
            return store_fine_double(lhs, static_cast<double>(rhs), policy, action);
        // Continuous: store verbatim. The grid's values have denominators ≤ its bound:
        // one past it is off the grid, rounded onto it under a rounding policy, else reported (but unsafe).
        if (abs_den(rv.Den) > to_rational(grid_of<L>.MaxDenominator).Num) [[unlikely]]
        {
          if constexpr (uses_flag<L, P, snap>)
          {
            lhs = L::from_raw(rational_of(onto_bound<L, plain_t<P>>(exact_of(rv))), unsafe);
            return;
          }
          else if (std::is_constant_evaluated() || is_checked(policy_of<L> | plain_t<P>::Flags))
            return report_failure(lhs, policy, action, error::rounding_error);
        }
        lhs = L::from_raw(rv, unsafe);
      }
      else if constexpr (point_grid<L>)
        lhs = L::from_raw({}, unsafe); // a point: its value is the type
      else
      {
        // Store the k-th notch slot: raw_from_offset<L> covers index and value raws.
        auto store_slot = [&](auto k) { lhs = L::from_raw(raw_from_offset<L>(k), unsafe); };

        constexpr bool has_round_flag{uses_flag<L, P, snap>}; // every round_* flag contains snap
        // The slot onum/den when it is exact or P rounds; false otherwise
        // (a strict policy, off the notch: the rational path below reports it).
        auto stores_slot = [&](std::uint64_t onum, std::uint64_t den)
        {
          const std::uint64_t q{onum / den}, r{onum % den}; // one division
          if (r == 0)
          {
            store_slot(q);
            return true;
          }
          if constexpr (has_round_flag)
          {
            store_slot(round_split<L, P>(q, r, den));
            return true;
          }
          return false;
        };

        // A floating source on another anchored lattice within 2^51 (a decimal or coarse notch):
        // the slot from the exact v·K, one rounding (double_slot).
        if constexpr (std::floating_point<R> && !wide_float<R> && double_lattice<L>.Ok
                      && !(anchored<L> && double_exact<grid_of<L>> && std::has_single_bit(notch64<L>.Num)))
        {
          const auto s{double_slot<L, rounding_for<L, P>>(static_cast<double>(rhs))};
          if constexpr (!has_round_flag)
            if (!s.Exact && policy.checks_rounding()) [[unlikely]]
              return report_failure(lhs, policy, action, error::rounding_error);
          store_slot(static_cast<std::uint64_t>(s.Slot));
          return;
        }
        // A floating source on an anchored grid with a power-of-two notch whose values double holds:
        // v/Notch is exact, so round it in double (the integer storage's rule) and read the slot off the value index.
        // No rational round trip.
        if constexpr (std::floating_point<R> && anchored<L> && double_exact<grid_of<L>>
                      && std::has_single_bit(notch64<L>.Num))
        {
          constexpr std::int64_t lo{signed_numerator(lower_index64<L>)};
          const double           v{static_cast<double>(rhs)};
          const std::int64_t     j{snap_double_index<grid_of<L>, rounding_for<L, P>>(v)};
          if constexpr (!has_round_flag)
            if (static_cast<double>(j) * static_cast<double>(notch64<L>) != v && policy.checks_rounding())
              [[unlikely]]
              return report_failure(lhs, policy, action, error::rounding_error);
          store_slot(static_cast<std::uint64_t>(j - lo));
          return;
        }
        bool           exact{true};
        const rational rv{source_of(rhs, exact)};
        if constexpr (std::floating_point<R>)
          if (!exact) [[unlikely]]
            return store_fine_double(lhs, static_cast<double>(rhs), policy, action);

        // Q-format integer shortcut: with an integer Lower and notch 1/K the offset is (num − Lo·aden)·K / aden,
        // integer ops instead of two rational ops.
        // round_quotient is invariant under reduction, so the slot is bit-identical to the rational path.
        // Oversized denominators fall through (the max_den guard keeps every product inside int64).
        if constexpr (q_format_fits<L> && notched<L>)
        {
          constexpr std::int64_t  K = abs_den(notch64<L>.Den);
          constexpr std::int64_t  Lo{lower_int64<L>};
          constexpr std::uint64_t max_den{q_format_max_den<L>};

          const std::uint64_t aden{abs_den(rv.Den)};
          if (max_den != 0 && aden <= max_den)
          {
            // The offset (num − Lo·aden)·K / aden, unreduced:
            // round_quotient rounds it as the reduced fraction would, and max_den keeps the product in int64.
            const std::int64_t  num{signed_numerator(rv)};
            const std::uint64_t onum{ // ≥ 0: rhs ≥ Lower (in range)
              static_cast<std::uint64_t>((num - Lo * static_cast<std::int64_t>(aden)) * K)};
            if (stores_slot(onum, aden))
              return;
          }
        }

        // The same shortcut for every other lattice: with K the common denominator of Notch and Lower,
        // Notch = sN/K and Lower = m/K, the offset is (num·K − m·aden)/(aden·sN).
        if constexpr (lattice_codec<L>.Ok)
        {
          constexpr auto      c{lattice_codec<L>};
          const std::uint64_t aden{abs_den(rv.Den)};
          if (aden <= c.MaxDen)
          {
            // Unreduced: MaxDen bounds these products already,
            // and the unanchored rounding and the exactness test do not change under reduction.
            const std::int64_t  num{signed_numerator(rv)};
            const std::uint64_t onum{ // ≥ 0: rhs ≥ Lower (in range)
              static_cast<std::uint64_t>(num * c.K - c.M * static_cast<std::int64_t>(aden))};
            const std::uint64_t den2{aden * static_cast<std::uint64_t>(c.S)};
            if (stores_slot(onum, den2))
              return;
          }
        }

        // The general slot: out of line and cold behind a shortcut above (it is then rare),
        // inline where it is the store's only path.
        constexpr bool         shortcut{(q_format_fits<L> && notched<L>) || lattice_codec<L>.Ok};
        const wide_slot_result slot{shortcut ? rational_slot_cold<P>(rv) : rational_slot<P>(rv)};
        if constexpr (!has_round_flag)
          if (!slot.Exact && policy.checks_rounding()) [[unlikely]]
            return report_failure(lhs, policy, action, error::rounding_error);
        store_slot(slot.Slot);
      }
    }

  private:
    // The slot of v by 64-bit rationals, (v − Lower)/Notch rounded.
    // The exact quotient can overflow that range (a huge source denominator × a fine notch):
    // the slot then comes from the exact wide index (v is in range, so it fits L's raw).
    // Rounding is the value-space rule, as everywhere else.
    // The 16-byte result returns in registers; the expected rationals stay inside.
    template <typename P>
    static constexpr wide_slot_result rational_slot(rational v)
    {
      const auto rel     {try_sub(v, lower64<L>)};
      const auto quotient{rel ? try_div(*rel, notch64<L>) : rel};
      if (!quotient.has_value()) [[unlikely]]
        return wide_slot<P>(v);
      if (quotient->Den == 1)
        return {quotient->Num, true};
      return {round_quotient<L, P>(quotient->Num, static_cast<std::uint64_t>(quotient->Den)), false};
    }
    template <typename P>
    [[gnu::cold]] [[gnu::noinline]] static constexpr wide_slot_result rational_slot_cold(rational v)
    {
      return rational_slot<P>(v);
    }

    // Range test for the source value.
    // A floating source compares in double when both endpoints are exact doubles (then the comparison is exact),
    // instead of converting the value to a rational first.
    // (A limit near 2^64 rounds to a double of 2^64, which no rational holds: it is no exact double.)
    static constexpr bool exact_double(rational r)
    {
      const double d{static_cast<double>(r)};
      return !past_64_bits(d) && rational{d} == r;
    }
    static constexpr bool double_bounds_exact =
      std::floating_point<R> && exact_double(lower64<L>) && exact_double(upper64<L>);

    // A rational source on integer endpoints compares by multiplying the endpoint by the denominator
    // (n/d ≤ m ⇔ n ≤ m·d; an overflowing m·d exceeds any n): exact, and no division.
    static constexpr bool integer_bounds = std::same_as<R, rational> && abs_den(lower64<L>.Den) == 1
                                        && abs_den(upper64<L>.Den) == 1 && fits_int64(interval_of<L>);

    static constexpr bool out_of_interval(const R& rhs)
    {
      if (huge(rhs))
        return true;
      if constexpr (double_bounds_exact)
      {
        constexpr double lo{static_cast<double>(lower64<L>)};
        constexpr double hi{static_cast<double>(upper64<L>)};
        return rhs < lo || rhs > hi;
      }
      else if constexpr (integer_bounds)
      {
        constexpr std::int64_t lo{signed_numerator(lower64<L>)};
        constexpr std::int64_t hi{signed_numerator(upper64<L>)};
        const std::uint64_t    n{rhs.Num}, d{abs_den(rhs.Den)};
        auto le = [&](std::uint64_t m)
        {
          std::uint64_t p;
          return mul_overflow(m, d, &p) || n <= p;
        };
        auto ge = [&](std::uint64_t m)
        {
          std::uint64_t p;
          return !mul_overflow(m, d, &p) && n >= p;
        };
        if (rhs.Den < 0 && n != 0) // value −n/d < 0
        {
          bool in_lo, in_hi;
          if constexpr (lo >= 0)
            in_lo = false;
          else
            in_lo = le(safe_abs(lo));
          if constexpr (hi >= 0)
            in_hi = true;
          else
            in_hi = ge(safe_abs(hi));
          return !(in_lo && in_hi);
        }
        bool in_lo, in_hi; // value n/d ≥ 0
        if constexpr (lo <= 0)
          in_lo = true;
        else
          in_lo = ge(static_cast<std::uint64_t>(lo));
        if constexpr (hi < 0)
          in_hi = false;
        else
          in_hi = le(static_cast<std::uint64_t>(hi));
        return !(in_lo && in_hi);
      }
      else if constexpr (std::floating_point<R>)
      {
        // A limit no double holds (INT64_MAX): compare exactly, without grid numbers, whose run-time ops stop at
        // 64 bits. Below 2^64 a rational holds the value unless it is finer than 2^-62.
        const double d{static_cast<double>(rhs)};
        if (abs_fraction_exact(d)) [[likely]]
        {
          const rational r{d};
          return r < lower64<L> || upper64<L> < r;
        }
        const auto v{exact_of_double(d)};
        return v < exact_of(lower64<L>) || exact_of(upper64<L>) < v;
      }
      else
        return not includes(interval_of<L>, rhs);
    }

  public:
    template <typename P, typename A = no_action>
    static constexpr L& assign(L& lhs, const R& rhs, P&& policy, A&& action = {})
    {
      // A long double that is a double stores as that double;
      // one with bits past it stores exactly (the double paths would drop them).
      if constexpr (wide_float<R>)
      {
        const double d{static_cast<double>(rhs)};
        if (is_finite(rhs) && static_cast<R>(d) != rhs) [[unlikely]]
        {
          // Past the double range: a stand-in past every limit, which clamps or overflows,
          // but a fold needs the exact value.
          if constexpr (uses_flag<L, P, wrap>)
            if (!is_finite(d))
            {
              report_failure(lhs, policy, action, error::overflow);
              return lhs;
            }
          return assign_exact<R>(lhs, exact_of_long_double(rhs), policy, std::forward<A>(action));
        }
        return assignment<L, double>::assign(lhs, d, policy, std::forward<A>(action));
      }
      else if constexpr (wide_valued<L>)
      {
        // NaN and ±inf first, as below; a finite |rhs| ≥ 2^64 is an integer,
        // taken exactly (or by its side, past the grid: exact_of_large).
        if constexpr (std::floating_point<R>)
          if (!is_finite(rhs) || huge(rhs)) [[unlikely]]
          {
            if (rhs != rhs)
            {
              report_failure(lhs, policy, action, error::not_finite);
              return lhs;
            }
            if (!is_finite(rhs) && !uses_flag<L, P, clamp>)
            {
              report_failure(lhs, policy, action, error::not_finite);
              return lhs;
            }
            // ±∞ clamps by its stand-in and reports the overshoot grid's end.
            if constexpr (clamp_action<plain_t<A>>)
              if (!is_finite(rhs))
              {
                assign_exact<R>(lhs, exact_of_large<L>(static_cast<double>(rhs)), policy, no_action{});
                action.Fn(lhs, infinite_clamp_overshoot<L, R>(rhs < 0));
                return lhs;
              }
            // A fold and an overshoot need the exact value, not the stand-in.
            if constexpr (uses_flag<L, P, wrap> || clamp_action<plain_t<A>> || wrap_action<plain_t<A>>)
              if (is_finite(rhs))
                return assign_exact<R>(
                  lhs, exact_of_double(static_cast<double>(rhs)), policy, std::forward<A>(action));
            return assign_exact<R>(lhs, exact_of_large<L>(static_cast<double>(rhs)), policy, std::forward<A>(action));
          }
        if constexpr (std::floating_point<R>)
          if (!abs_fraction_exact(static_cast<double>(rhs))) [[unlikely]] // finer than 2^-62
            return assign_exact<R>(lhs, exact_of_double(static_cast<double>(rhs)), policy, std::forward<A>(action));
        return assign_exact<R>(lhs, exact_of(rational{rhs}), policy, std::forward<A>(action));
      }
      else
        return assign_builtin(lhs, rhs, policy, std::forward<A>(action));
    }

  private:
    template <typename P, typename A>
    static constexpr L& assign_builtin(L& lhs, const R& rhs, P&& policy, A&& action)
    {
      // Limits that are doubles: one test passes a value in range,
      // and fails NaN, ±inf and every |v| ≥ 2^64 too (the limits are below), so only the rest takes the checks below.
      if constexpr (double_bounds_exact)
      {
        constexpr double lo{static_cast<double>(lower64<L>)}, hi{static_cast<double>(upper64<L>)};
        if (static_cast<double>(rhs) >= lo && static_cast<double>(rhs) <= hi) [[likely]]
        {
          store_checked(lhs, rhs, policy, action);
          return lhs;
        }
      }
      // NaN and ±inf: no rational value to round or range-check.
      // clamp saturates an infinity; everything else reports not_finite through the policy.
      if constexpr (std::floating_point<R>)
        if (!is_finite(rhs)) [[unlikely]]
        {
          if constexpr (uses_flag<L, P, clamp>)
            if (rhs == rhs)
            {
              // Saturated like any value past a limit: an on_clamp hears of it, with an overshoot of ±inf.
              apply_clamp(lhs, rhs, policy, action);
              return lhs;
            }
          report_failure(lhs, policy, action, error::not_finite);
          return lhs;
        }

      if (out_of_interval(rhs)) [[unlikely]]
      {
        if constexpr (std::floating_point<R>)
          if (!abs_fraction_exact(static_cast<double>(rhs)))
          {
            store_fine_double(lhs, static_cast<double>(rhs), policy, action);
            return lhs;
          }
        // Round first: a value just outside may round onto an endpoint.
        if constexpr (rounds_before_range_check<L, plain_t<P>>)
          if (!huge(rhs))
            if (const auto rr = raw_if_rounds_inside<L, plain_t<P>>(rational{rhs}); rr.Ok)
            {
              lhs = L::from_raw(rr.Raw, unsafe);
              return lhs;
            }
        // The fractional path has no wrap action branch (Wrappable = false).
        if (dispatch_out_of_range<false>(
              lhs,
              policy,
              action,
              [&] { apply_clamp(lhs, rhs, policy, action); },
              [&] { apply_wrap(lhs, rhs, policy, action); }))
          return lhs;
      }

      store_checked(lhs, rhs, policy, action);
      return lhs;
    }
  };

  // assign(insidable, insidable)
  template <insidable L, insidable R>
  struct assignment<L, R>
  {
  private:
    // Offset and Factor map rhs.Raw → lhs.Raw by lhs.Raw = Factor·rhs.Raw + Offset between two integer raws.
    // A continuous side stores through the rational path instead (both 0, unused),
    // and a point L holds one value (both 0).
    static constexpr bool integer_raws = integer_storage<L> && integer_storage<R>;

    // Exactly; a point R's raw is always 0: the mapping is Offset alone.
    static constexpr grid_number offset_exact =
      integer_raws && notched<L> ? (lower_of<R> - lower_of<L>) / notch_of<L> : grid_number{0};
    static constexpr grid_number factor_exact =
      integer_raws && notched<L> && !point_storage<R> ? notch_of<R> / notch_of<L> : grid_number{0};

    // A mapping past the 64-bit rational (a full-int64 source into a fine grid) stores through the rational path,
    // as a continuous side does.
    static constexpr bool maps_raws = integer_raws && fits_rational(offset_exact) && fits_rational(factor_exact);

  public:
    static constexpr rational Offset = maps_raws ? to_rational(offset_exact) : rational{0};
    static constexpr rational Factor = maps_raws ? to_rational(factor_exact) : rational{0};

    // Raw-space integer-only mapping: it requires integer raw storage on both sides (not rational),
    // and every raw and every mapped raw in int64:
    // map_raw's L-raw range is [Offset, Offset + Factor·max_index<R>] (+ Lower for value storage).
    static constexpr bool is_integer_mapping = []
    {
      if constexpr (!maps_raws || abs_den(Factor.Den) != 1 || abs_den(Offset.Den) != 1 || !values_fit_int64<L>
                    || !values_fit_int64<R>)
        return false;
      else
      {
        const rational lo{Offset + (index_storage<L> ? rational{0} : lower64<L>)};
        const rational hi{lo + Factor * rational{max_index_of<R>}};
        return lo >= rational{std::numeric_limits<std::int64_t>::min()}
            && hi <= rational{std::numeric_limits<std::int64_t>::max()};
      }
    }();

    // A non-integer mapping folded to one integer multiply-add:
    //   Offset + Factor·raw = (o_s·f_d + raw·f_n·o_d) / (o_d·f_d)
    // with every coefficient a compile-time constant.
    // round_quotient is invariant under fraction reduction,
    // so rounding the unreduced pair is bit-identical to reducing through the two rational ops first.
    // Ok gates on every product (the worst-case run-time numerator over R's raw range too) provably fitting int64;
    // Mul, Add and Den are zeroed when not Ok.
    struct affine_map_t
    {
      std::int64_t Mul;
      std::int64_t Add;
      std::int64_t Den;
      bool         Ok;
    };
    static constexpr affine_map_t affine_map = []
    {
      constexpr affine_map_t no{0, 0, 0, false};
      if constexpr (!maps_raws || !notched<L> || is_integer_mapping || !values_fit_int64<L> || !values_fit_int64<R>)
        return no;
      else
      {
        if (!std::in_range<std::int64_t>(Factor.Num) || !std::in_range<std::int64_t>(Offset.Num))
          return no;
        const std::int64_t f_n{static_cast<std::int64_t>(Factor.Num)}; // Factor > 0
        const std::int64_t f_d = abs_den(Factor.Den);
        const std::int64_t o_s{signed_numerator(Offset)};
        const std::int64_t o_d = abs_den(Offset.Den);
        affine_map_t       m{0, 0, 0, true};
        if (mul_overflow(f_n, o_d, &m.Mul) || mul_overflow(o_s, f_d, &m.Add) || mul_overflow(o_d, f_d, &m.Den))
          return no;
        // The numerator Add + r·Mul over R's offsets r in [0, max_index] runs from Add to Add + max_index·Mul
        // (Mul > 0): both ends in int64, and Add not −2^63 (its magnitude is taken). num: the worst |numerator|.
        if (!std::in_range<std::int64_t>(max_index_of<R>) || m.Add == std::numeric_limits<std::int64_t>::min())
          return no;
        const std::int64_t rmax{static_cast<std::int64_t>(max_index_of<R>)};
        std::int64_t       term, hi;
        if (mul_overflow(rmax, m.Mul, &term) || add_overflow(term, m.Add, &hi))
          return no;
        const std::int64_t num{std::max(m.Add < 0 ? -m.Add : m.Add, hi < 0 ? -hi : hi)};
        // round_quotient equivalence: rounding is reduction-invariant,
        // but its choice between the value-index and the offset branch keys on m·di + num fitting int64.
        // Mirror those checks for the unreduced den, so both forms take the same branch
        // (ties on negatives differ across branches).
        if constexpr (grid_denominator(lower_index_of<L>) == grid_int{1})
        {
          if (!lower_index_fits_int64<L>)
            return no;
          const std::int64_t mbias{signed_numerator(to_rational(lower_index_of<L>))};
          std::int64_t       mdi, total;
          if (mul_overflow(mbias, m.Den, &mdi) || add_overflow(mdi, num, &total))
            return no;
        }
        return m;
      }
    }();

    // L's raw for R's offset 0: Offset + raw_lo<L>, which is_integer_mapping proves fits int64
    // (Offset alone may not, e.g. 2^63 into a full int64).
    static constexpr std::int64_t map_bias = signed_numerator(Offset + (index_storage<L> ? rational{0} : lower64<L>));

    // Map rhs.Raw into L's raw space (requires is_integer_mapping):
    // Factor scales R's 0-based offset, map_bias adds L's raw for offset 0.
    // Every mapped raw fits int64, so the wrapping uint64 multiply-add is exact
    // even where an intermediate (the offset, or a signed partial sum) would not be.
    static constexpr std::int64_t map_raw(auto rhs_raw)
    {
      const std::uint64_t rel{static_cast<std::uint64_t>(static_cast<std::int64_t>(rhs_raw))
                              - static_cast<std::uint64_t>(raw_lo<R>)};
      return static_cast<std::int64_t>(
        static_cast<std::uint64_t>(map_bias) + static_cast<std::uint64_t>(Factor.Num) * rel);
    }

  private:
    template <typename A>
    static constexpr void apply_clamp(L& lhs, const R& rhs, A&& action)
    {
      const bool low{below_lower(rhs)};
      lhs = L::from_raw(endpoint_raw<L>(low), unsafe);
      // The overshoot rhs − clamped, exact on its grid (assign_exact.hpp):
      // R's grid minus the two-point grid of L's limits,
      // which is what rhs minus that limit as an inside computes in integers.
      if constexpr (clamp_action<plain_t<A>>)
      {
        using limits = inside<lower_of<L> == upper_of<L>
                                ? grid{lower_of<L>}
                                : grid{interval{lower_of<L>, upper_of<L>}, upper_of<L> - lower_of<L>}>;
        if constexpr (!wide_valued<R> && !wide_valued<L>
                      && requires(const R& r, const limits& l)
                         {
                           { r - l } -> std::same_as<clamp_overshoot_t<L, R>>;
                         })
        {
          const limits limit{limits::from_raw(raw_of_slot<limits>(low || point_grid<L> ? 0 : 1), unsafe)};
          action.Fn(lhs, rhs - limit);
        }
        else
          action.Fn(lhs, make_clamp_overshoot<L, R>(exact_of(rhs) + -exact_of(lhs)));
      }
    }
    // R's value below L's Lower: the integer compare of out_of_interval.
    static constexpr bool below_lower(const R& rhs)
    {
      if constexpr (units_cmp_fits<L, R>)
      {
        constexpr grid_number  U{gcd(unit_of<L>, unit_of<R>)};
        constexpr std::int64_t lo{static_cast<std::int64_t>(units_lo<L, U>)};
        return value_in_units<std::int64_t, U>(rhs) < lo;
      }
      else
        return as_rational(rhs) < lower64<L>;
    }

    template <typename P, typename A>
    static constexpr void apply_wrap(L& lhs, const R& rhs, P&& policy, A&& action)
    {
      // The integer modular wrap (range = Upper - Lower + 1, integer values) is only correct on a unit-integer grid
      // (notch 1 with integer bounds, so consecutive integers are adjacent grid points)
      // and for a source whose values are integers (no rounding to do).
      // Anything else goes through the rational modular wrap, which rounds by the policy first.
      if constexpr (unit_lattice(grid_of<L>) && integer_lattice<R>)
      {
        // Unit-integer fast path: modular wrap on the integer value, exact
        // (either grid may reach past int64; the span can be 2^64−1).
        using fold = unit_fold<L, grid_numerator(lower_of<R>), grid_numerator(upper_of<R>)>;
        using W    = typename fold::W;
        const auto [excess, w] = fold::fold(static_cast<W>(grid_numerator(as_rational(rhs))));
        lhs = L::from_raw(raw_of_slot<L>(w), unsafe);
        if constexpr (wrap_action<plain_t<A>>)
          action.Fn(lhs, make_wrap_carry<L, R>(excess)); // carry as an inside
      }
      else if constexpr (wrap_action<plain_t<A>>)
      {
        // A fractional destination with a wrap action:
        // reuse the rational modular wrap for the store and the rounding,
        // but re-type its carry q (an inside on the rational source's carry grid) for R
        // before handing it to the user action.
        assignment<L, rational>::apply_wrap(
          lhs, as_rational(rhs), policy,
          ins::detail::on_wrap([&](auto& self, const auto& q) { action.Fn(self, make_wrap_carry<L, R>(q)); }));
      }
      else
      {
        // A fractional destination, no wrap action: delegate unchanged.
        assignment<L, rational>::apply_wrap(lhs, as_rational(rhs), policy, action);
      }
    }

    template <typename P, typename A>
    static constexpr bool clamp_or_fail(L& lhs, const R& rhs, P&& policy, A&& action)
    {
      return dispatch_out_of_range<true>(
        lhs,
        policy,
        action,
        [&] { apply_clamp(lhs, rhs, action); },
        [&] { apply_wrap(lhs, rhs, policy, action); });
    }

    // R's value outside L's interval:
    // for integer raws an integer compare of counts in a common unit against L's endpoints, else the exact value.
    static constexpr bool out_of_interval(const R& rhs)
    {
      if constexpr (units_cmp_fits<L, R>)
      {
        constexpr grid_number  U{gcd(unit_of<L>, unit_of<R>)};
        constexpr std::int64_t lo{static_cast<std::int64_t>(units_lo<L, U>)},
                               hi{static_cast<std::int64_t>(units_hi<L, U>)};
        const std::int64_t     c{value_in_units<std::int64_t, U>(rhs)};
        return c < lo || c > hi;
      }
      else
        return not includes(interval_of<L>, as_rational(rhs));
    }

    // (The value is in range here: of the actions only on_error can fire.)
    template <typename P, typename A>
    static constexpr void store(L& lhs, const R& rhs, P&& policy, A&& action)
    {
      if constexpr (!maps_raws)
        // A continuous side or no raw mapping: store the source's value through the rational store,
        // which rounds it by the policy or reports rounding_error.
        assignment<L, rational>::store_checked(lhs, as_rational(rhs), policy, action);
      else if constexpr (is_integer_mapping)
      {
        // exact: Factor and Offset have integer denominators, no rounding ambiguity
        if constexpr (Offset == 0 && Factor == 1 && same_storage<L, R>)
          lhs = L::from_raw(raw_cast<L>(rhs.raw()), unsafe);
        else
          lhs = L::from_raw(raw_cast<L>(map_raw(rhs.raw())), unsafe);
      }
      else if constexpr (affine_map.Ok)
      {
        // The folded non-integer mapping: one multiply-add, then the same round_quotient
        // (invariant under reduction, bit-identical to the rational chain below).
        // Offset and Factor map R's 0-based offset: a value raw counts from Lower.
        const std::int64_t  r_offset{static_cast<std::int64_t>(rhs.raw()) - raw_lo<R>}; // 0 for an index raw
        const std::int64_t  num{affine_map.Add + r_offset * affine_map.Mul};
        const std::uint64_t q{round_quotient<L, P>(
          static_cast<std::uint64_t>(num < 0 ? -num : num), static_cast<std::uint64_t>(affine_map.Den))};
        lhs = L::from_raw(num < 0 ? raw_from_offset<L>(-static_cast<std::int64_t>(q)) : raw_from_offset<L>(q), unsafe);
      }
      else
      {
        // Offset and Factor map R's 0-based offset (a value raw counts from Lower).
        // A step past the 64-bit rational takes the exact wide store.
        const auto r_offset = [&]
        {
          if constexpr (point_storage<R>) // a point's raw holds nothing: the mapping is Offset alone
            return std::expected<rational, error>{rational{0}};
          else if constexpr (raw_lo<R> == 0)
            return std::expected<rational, error>{rational{rhs.raw()}};
          else
            return try_sub(rational{rhs.raw()}, lower64<R>);
        }();
        const auto scaled{r_offset ? try_mul(Factor, *r_offset) : r_offset};
        const auto mapped{scaled ? try_add(Offset, *scaled) : scaled};
        if (!mapped) [[unlikely]]
        {
          assign_exact<R>(lhs, exact_of(rhs), policy, action);
          return;
        }
        const rational rat{*mapped};
        std::uint64_t  ad{abs_den(rat.Den)};
        // Round the L offset to a notch index in value space through round_quotient (as the scalar path does),
        // honouring every rounding mode.
        std::uint64_t q{round_quotient<L, P>(rat.Num, ad)};
        // rat is the L offset; raw_from_offset<L> adds lower64<L> back for value storage.
        lhs = L::from_raw(
          (rat.Den < 0) ? raw_from_offset<L>(-static_cast<std::int64_t>(q)) : raw_from_offset<L>(q), unsafe);
      }
    }

  public:
    template <typename P, typename A = no_action>
    static constexpr L& assign(L& lhs, const R& rhs, P&& policy, A&& action = {})
    {
      // A wide raw on either side: the exact wide path.
      if constexpr (wide_valued<L> || wide_valued<R>)
      {
        static_checks<P>();
        if constexpr (same_notch_raws<L, R>)
        {
          // Equal notches: the raw maps by a constant shift.
          // Out of range takes the exact path's policy cascade below.
          using W = work_int_t<same_notch_bits<L, R>>;
          const W raw{static_cast<W>(rhs.raw()) + static_cast<W>(same_notch_shift<L, R>)};
          if (raw >= static_cast<W>(raw_lo_exact<L>) && raw <= static_cast<W>(raw_hi_exact<L>)) [[likely]]
          {
            lhs = L::from_raw(static_cast<raw_t<L>>(raw), unsafe);
            return lhs;
          }
          // Past a limit (a product of two 64-bit values, say):
          // unsafe keeps the low bits, as the integer stores do; a clamp without a hook stores the endpoint.
          if constexpr (!needs_runtime_range_check<L, plain_t<P>, plain_t<A>> && !wide_valued<L>)
          {
            lhs = L::from_raw(static_cast<raw_t<L>>(raw), unsafe);
            return lhs;
          }
          else if constexpr (uses_flag<L, P, clamp> && !clamp_action<plain_t<A>> && !wrap_action<plain_t<A>>
                             && !wide_valued<L>)
          {
            lhs = L::from_raw(endpoint_raw<L>(raw < static_cast<W>(raw_lo_exact<L>)), unsafe);
            return lhs;
          }
          else if constexpr (uses_flag<L, P, wrap> && !clamp_action<plain_t<A>> && !wrap_action<plain_t<A>>
                             && !wide_valued<L>
                             && max_index_of<L> < std::uint64_t{std::numeric_limits<std::int64_t>::max()})
          {
            // A wrap folds the slot raw − raw_lo (the period fits int64):
            // in int64 when the slot fits (a step a little past a limit), else in W.
            const W o{raw - static_cast<W>(raw_lo_exact<L>)};
            lhs = L::from_raw(
              raw_from_offset<L>(o >= W{std::numeric_limits<std::int64_t>::min()}
                                         && o <= W{std::numeric_limits<std::int64_t>::max()}
                                     ? fold_slot<L>(static_cast<std::int64_t>(o))
                                     : fold_wide_slot<L>(o)),
              unsafe);
            return lhs;
          }
        }
        return assign_exact<R>(lhs, exact_of(rhs), policy, std::forward<A>(action));
      }
      else
        return assign_builtin(lhs, rhs, policy, std::forward<A>(action));
    }

  private:
    // Every mapped raw minus L's raw_lo fits int64 (a wrap folds that slot).
    static constexpr bool mapped_slots_fit = []
    {
      if constexpr (!is_integer_mapping)
        return false;
      else
      {
        const rational lo{Offset + (index_storage<L> ? rational{0} : lower64<L>)};
        const rational hi{lo + Factor * rational{max_index_of<R>}};
        const rational b{raw_lo<L>};
        const auto     slo{try_sub(lo, b)}, shi{try_sub(hi, b)};
        return slo && shi && *slo >= rational{std::numeric_limits<std::int64_t>::min()}
            && *shi <= rational{std::numeric_limits<std::int64_t>::max()};
      }
    }();
    // The out-of-range store in integers: a clamp or wrap policy with no hook
    // (a hook needs the exact overshoot or carry), on a lattice within int64 (anchored or not)
    // whose slots fold in int64, from an integer or affine raw mapping.
    template <typename P, typename A>
    static constexpr bool integer_out_of_range = []
    {
      if constexpr (clamp_action<plain_t<A>> || wrap_action<plain_t<A>> || point_storage<L> || !notched<L>
                    || !values_fit_int64<L> || !values_fit_int64<R>)
        return false;
      else if constexpr (!(max_index_of<L> < (std::uint64_t{1} << 62)))
        return false;
      else if constexpr (is_integer_mapping)
        return mapped_slots_fit;
      else
        return affine_map.Ok && signed_slots_fit<L>;
    }();
    // The slot of rhs on L, rounded by P's mode in value space, possibly outside 0 … max_index
    // (affine mapping; integer_out_of_range).
    template <typename P>
    static constexpr std::int64_t rounded_slot(const R& rhs)
    {
      if constexpr (is_integer_mapping)
        return map_raw(rhs.raw()) - raw_lo<L>; // (integer_out_of_range: mapped_slots_fit)
      else
        return round_signed_slot<L, P>(
          affine_map.Add + (static_cast<std::int64_t>(rhs.raw()) - raw_lo<R>) * affine_map.Mul, affine_map.Den);
    }

    // wrap and clamp bring any value into range, so a disjoint rhs interval is fine for them
    // (as on the integral-rhs path); only strict policies reject it.
    template <typename P>
    static constexpr void static_checks()
    {
      static_assert(uses_flag<L, P, wrap> || uses_flag<L, P, clamp> || not excludes(interval_of<L>, interval_of<R>),
                    "rhs interval lies entirely outside lhs interval and the policy cannot bring it into range");
      static_assert(notches_compatible<L, R> || uses_flag<L, P, snap>,
                    "incompatible notches: use x.with<round_toward_zero>() or a rounding policy to allow rounding");
    }

    template <typename P, typename A>
    static constexpr L& assign_builtin(L& lhs, const R& rhs, P&& policy, A&& action)
    {
      static_checks<P>();

      if constexpr (not includes(interval_of<L>, interval_of<R>))
      {
        if constexpr (needs_runtime_range_check<L, plain_t<P>, plain_t<A>>)
        {
          // (A point L maps every raw onto its one raw: only the value tells whether it is in range.)
          if constexpr (is_integer_mapping && !point_storage<L>)
          {
            if (std::int64_t mapped{map_raw(rhs.raw())}; mapped < raw_lo<L> || mapped > raw_hi<L>)
            {
              if constexpr (integer_out_of_range<P, A> && uses_flag<L, P, clamp>)
              {
                lhs = L::from_raw(endpoint_raw<L>(mapped < raw_lo<L>), unsafe);
                return lhs;
              }
              else if constexpr (integer_out_of_range<P, A> && uses_flag<L, P, wrap> && mapped_slots_fit)
              {
                lhs = L::from_raw(raw_from_offset<L>(fold_slot<L>(mapped - raw_lo<L>)), unsafe);
                return lhs;
              }
              if (clamp_or_fail(lhs, rhs, policy, action))
                return lhs;
            }
          }
          else if (out_of_interval(rhs))
          {
            // A clamp or a wrap without a hook on an anchored lattice:
            // the slot rounded in integers, then the endpoint or the fold.
            if constexpr (integer_out_of_range<P, A>)
            {
              const std::int64_t slot{rounded_slot<P>(rhs)};
              if (slot >= 0 && std::cmp_less_equal(slot, max_index_of<L>)) // rounded onto the grid
              {
                if constexpr (rounds_before_range_check<L, plain_t<P>>)
                {
                  lhs = L::from_raw(raw_from_offset<L>(static_cast<std::uint64_t>(slot)), unsafe);
                  return lhs;
                }
              }
              if constexpr (uses_flag<L, P, clamp>)
                lhs = L::from_raw(endpoint_raw<L>(slot < 0), unsafe);
              else if constexpr (uses_flag<L, P, wrap>)
                lhs = L::from_raw(raw_from_offset<L>(fold_slot<L>(slot)), unsafe);
              else if (clamp_or_fail(lhs, rhs, policy, action)) // a report, or on_error
                return lhs;
              return lhs;
            }
            // Round first: a value just outside may round onto an endpoint.
            // (The integer mapping above lands on the lattice: nothing to round.)
            if constexpr (rounds_before_range_check<L, plain_t<P>>)
              if (const auto rr = raw_if_rounds_inside<L, plain_t<P>>(as_rational(rhs)); rr.Ok)
              {
                lhs = L::from_raw(rr.Raw, unsafe);
                return lhs;
              }
            if (clamp_or_fail(lhs, rhs, policy, action))
              return lhs;
          }
        }
      }

      store(lhs, rhs, policy, action);
      return lhs;
    }
  };

  // assign_grid_number: store a grid number at run time (a constant, N - 1).
  // Forming one is compile-time only (its operators are consteval), but reading one is not:
  // its limbs live in static storage. The exact value then stores like any exact value.
  // A number too wide for L's exact type lies far outside L when it is an integer,
  // so a stand-in just past the near limit takes the out-of-range path; a fraction that wide is overflow.
  template <insidable L, typename P>
  constexpr L& assign_grid_number(L& lhs, const grid_number& g, P&& policy)
  {
    constexpr std::size_t K{exact_limbs<L>};
    const big_rational    v{g.big()};
    auto limbs = [](const big_int& x) { return x.Size != 0 ? x.Size : std::size_t{1}; };
    if (limbs(v.Num) < K && limbs(v.Den) < K) [[likely]]
      return assign_exact<rational>(lhs, exact_of_grid<K>(g), policy, no_action{});
    if (!v.is_integer())
    {
      no_action none;
      report_failure(lhs, policy, none, error::overflow);
      return lhs;
    }
    const wide_rational<K> one{wide_sint<K>{1}, wide_sint<K>{1}};
    const wide_rational<K> past{v.Num.negative() ? exact_of_grid<K>(lower_of<L>) + -one
                                                 : exact_of_grid<K>(upper_of<L>) + one};
    return assign_exact<rational>(lhs, past, policy, no_action{});
  }
} // namespace ins::detail

#endif // INSIDE_INSIDE_DETAIL_ASSIGNMENT_HPP
