// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_GENERIC_HPP
#define BEMAN_INSIDE_GENERIC_HPP

#include <beman/inside/detail/debug.hpp>
#include <beman/inside/lift.hpp>
#include <beman/inside/grid.hpp>
#include <beman/inside/policy_flag.hpp>

//---------------------------------------------------------------------------
// generic — type-level traits and predicates used everywhere else. Public
// grid/policy introspection (`grid_of<B>`, `policy_of<B>`, `Lower/Upper/notch_of<B>`,
// `interval_of<B>`) plus the `insidable`/`numeric`/`inside_assignable` concepts; the
// storage-shape predicates and raw/value converters are internal (`beman::inside::detail`).
//---------------------------------------------------------------------------
namespace beman::inside
{
  template <grid G = grid{{0, 0}, 0}, policy_flag P = checked> struct inside;

  template <class>                 inline constexpr bool is_inside_v = false;
  template <grid G, policy_flag P> inline constexpr bool is_inside_v<inside<G, P>> = true;

  template <typename B>
  concept insidable = is_inside_v<std::remove_cvref_t<B>>;

  //---------------------------------------------------------------------------
  // Public grid/policy introspection — extract an inside's template parameters.
  // These mirror std::numeric_limits: they report what the grid is, used
  // opaquely (the rational return type is never named by callers).
  //---------------------------------------------------------------------------
  namespace detail
  {
    template <typename B> struct inside_params;
    template <grid G, policy_flag P> struct inside_params<inside<G, P>>
    {
      static constexpr grid        grid_v   = G;
      static constexpr policy_flag policy_v = P;
    };
  }

  template <insidable B>
  inline constexpr grid grid_of = detail::inside_params<std::remove_cvref_t<B>>::grid_v;

  template <insidable B>
  inline constexpr policy_flag policy_of = detail::inside_params<std::remove_cvref_t<B>>::policy_v;

  template <typename T>
  inline constexpr interval interval_of = {0,0};

  template <insidable B>
  inline constexpr interval interval_of<B> = grid_of<B>.Interval;

  template <std::integral I>
  inline constexpr interval interval_of<I> =
      {std::numeric_limits<I>::lowest(), std::numeric_limits<I>::max()};

  template <insidable B> inline constexpr detail::rational lower_of = grid_of<B>.Interval.Lower;
  template <insidable B> inline constexpr detail::rational upper_of = grid_of<B>.Interval.Upper;
  template <insidable B> inline constexpr detail::rational notch_of = grid_of<B>.Notch;

  template <typename N>
  concept numeric = insidable<N> or detail::arithmetic<N>;

  //---------------------------------------------------------------------------
  // Internal plumbing — storage shape, raw/value conversion, dispatch.
  //---------------------------------------------------------------------------
  namespace detail
  {
    template<typename T>
    using plain_t = std::remove_cvref_t<T>;

    // Always-false but template-dependent: lets a `static_assert` inside a
    // template body fire only when that template is actually instantiated
    // (e.g. the guidance overloads that make `inside + 1` ill-formed).
    template<typename...>
    inline constexpr bool dependent_false = false;

    //-------------------------------------------------------------------------
    // Conversion-helper legend — the value/raw plumbing reused across the
    // engine. "value space" = the number an inside denotes; "raw space" = how it
    // is stored (see §2 "Storage encoding" in docs/internals.md). Use this to
    // tell the similarly-named helpers apart:
    //
    //   as_rational(x)         value → rational    exact view of a scalar or inside
    //   as_double(b)           raw   → double       kind-aware decode; lossy off dyadic grids
    //   to_value(b)            raw   → imax         the inside's integer value (decodes an index)
    //   from_value(b, v)       imax  → raw          store integer value v into b (inverse of to_value)
    //   raw_cast<B>(x)         x     → raw_t<B>     TYPE cast only — no value arithmetic
    //   raw_imax(b)            raw   → imax         widen the raw bits (NOT the value for index storage)
    //   raw_from_offset<B>(o)  index → raw_t<B>     adds raw_lo for direct storage; identity for index
    //-------------------------------------------------------------------------

    // Uniform rational view of a scalar or inside (rational{v} / operator rational()).
    template <numeric N>
    [[nodiscard]] constexpr rational as_rational(N v)
    {
      if constexpr (arithmetic<N>) return rational{v};
      else                         return v;
    }

    // Canonical-zero test for a divisor. rational stores zero as {0, 1}, so
    // Numerator == 0 catches it regardless of representation; other types compare
    // against their own zero.
    template <typename T>
    [[nodiscard]] constexpr bool is_canonical_zero(T const& v)
    {
      if constexpr (std::same_as<T, rational>) return v.Numerator == 0;
      else                                      return v == T{0};
    }

    template <insidable B>
    using raw_t = typename B::raw_type;

    // How an inside's value lives in its raw storage — four disjoint encodings
    // (selected by policy flags or deduced; see grid.hpp storage_pick):
    //   rational_raw — raw IS the value, as a rational.
    //   f64_raw      — raw IS the value, as an IEEE-754 double (dyadic grids only).
    //   f32_raw      — raw IS the value, as an IEEE-754 float  (dyadic grids only).
    //   value_raw    — raw IS the value, as a plain integer.
    //   index_raw    — raw is a 0-based notch index; value = Lower + raw*Notch.
    template <insidable B>
    inline constexpr bool f64_raw = std::is_same_v<raw_t<B>, double>;

    template <insidable B>
    inline constexpr bool f32_raw = std::is_same_v<raw_t<B>, float>;

    // fp_raw — value held directly in a floating-point raw (f64 or f32). These
    // share every value-path branch: read/store/compare/arithmetic compute in
    // double, narrowing to the raw type on store (lossless on an fp-exact grid).
    template <insidable B>
    inline constexpr bool fp_raw = f64_raw<B> || f32_raw<B>;

    template <insidable B>
    inline constexpr bool rational_raw = std::is_same_v<raw_t<B>, rational>;

    template <insidable B>
    inline constexpr bool value_raw =
         !fp_raw<B> && !rational_raw<B>
      && ((policy_of<B> & direct) == direct
          // A pinned width flag without `indexed` is value storage (raw == value)
          // regardless of Lower's sign — storage_pick checked the range fits.
          || (has_width_flag(policy_of<B>)
              && (policy_of<B> & indexed) != indexed)
          || ((policy_of<B> & indexed) != indexed
              && notch_of<B> == 1
              && (lower_of<B> == 0 || std::signed_integral<raw_t<B>>)));

    template <insidable B>
    inline constexpr bool index_raw =
         !fp_raw<B> && !rational_raw<B> && !value_raw<B>;

    // Ungated double view of any inside, for the `f64` arithmetic arms (the
    // public operator double() is gated on a rounding flag; this is always
    // available). Everything but index storage holds the value verbatim; an
    // index decodes through the grid.
    template <insidable B>
    [[nodiscard]] constexpr double as_double(B const& b) noexcept
    {
      if constexpr (!index_raw<B>)
        return static_cast<double>(b.raw());
      else
        return static_cast<double>((*(b.raw() * notch_of<B>) + lower_of<B>).value());
    }

    template <insidable B>
    using negative_t = inside<-grid_of<B>, policy_of<B>>;

    // True when R's interval cannot contain zero — so `a / b` can return a plain
    // `inside` instead of `expected<inside, errc>` (see detail/division.hpp). A point
    // grid at 0 is *not* excluded.
    template <insidable R>
    inline constexpr bool divisor_excludes_zero = (lower_of<R> > 0) || (upper_of<R> < 0);

    // Storage-agnostic int truncation of interval endpoints — intent-revealing
    // `static_cast<imax>(lower_of<B>)`. Used by from_value, raw_lo, the fast paths.
    template <insidable B>
    inline constexpr imax lower_imax = trunc(lower_of<B>);

    template <insidable B>
    inline constexpr imax upper_imax = trunc(upper_of<B>);

    // Slot count via grid::max_index (overflow-safe: 0 when it doesn't fit umax,
    // for grids that store as rational and never use the index).
    template <insidable B>
    inline constexpr umax max_index_v = grid_of<B>.max_index();

    //-------------------------------------------------------------------------
    // grid_value_bounds / rational_mul_is_safe / rational_add_is_safe
    //
    // Conservative compile-time inside on the (numerator, denominator) of any
    // canonical value on a grid, and derived "can the rational op of two grid
    // values overflow imax" predicates — letting checked exact arithmetic drop
    // the expected wrapper when the grids prove no overflow is reachable.
    //
    // For a notched grid every value v = lo + k·notch over the common denominator
    // dC = |lo.den|·|hi.den|·|notch.den| is linear in k, so the max scaled
    // numerator is at an endpoint. A continuous grid (Notch == 0, non-point) has
    // unbounded denominators — nothing provable, so the helpers return false.
    //-------------------------------------------------------------------------
    constexpr bool grid_value_bounds(grid g, umax& max_num, umax& max_den) noexcept
    {
      if (g.Notch.Numerator == 0 && !(g.Interval.Lower == g.Interval.Upper))
        return false;                          // continuous: dens unbounded

      umax d_lo = abs_den(g.Interval.Lower.Denominator);
      umax d_hi = abs_den(g.Interval.Upper.Denominator);
      umax d_no = (g.Notch.Numerator == 0) ? umax{1} : abs_den(g.Notch.Denominator);

      umax d_common;
      if (mul_overflow(d_lo, d_hi, &d_common)) return false;
      if (mul_overflow(d_common, d_no, &d_common)) return false;

      umax lo_scaled, hi_scaled;
      if (mul_overflow(g.Interval.Lower.Numerator, d_common / d_lo, &lo_scaled)) return false;
      if (mul_overflow(g.Interval.Upper.Numerator, d_common / d_hi, &hi_scaled)) return false;

      max_num = lo_scaled > hi_scaled ? lo_scaled : hi_scaled;
      max_den = d_common;
      return true;
    }

    constexpr bool rational_mul_is_safe(grid g_l, grid g_r) noexcept
    {
      umax n_l, d_l, n_r, d_r;
      if (!grid_value_bounds(g_l, n_l, d_l)) return false;
      if (!grid_value_bounds(g_r, n_r, d_r)) return false;

      umax num_prod, den_prod;
      if (mul_overflow(n_l, n_r, &num_prod)) return false;
      if (mul_overflow(d_l, d_r, &den_prod)) return false;
      if (den_prod > static_cast<umax>(std::numeric_limits<imax>::max())) return false;
      return true;
    }

    // add_impl's worst case over the conservative common denominator
    // D = d_l*d_r: scaled numerators A <= n_l*d_r and B <= n_r*d_l, sum
    // A + B. (The same-denominator and lcm-reduced paths only shrink these;
    // mixed signs subtract magnitudes.)
    constexpr bool rational_add_is_safe(grid g_l, grid g_r) noexcept
    {
      umax n_l, d_l, n_r, d_r;
      if (!grid_value_bounds(g_l, n_l, d_l)) return false;
      if (!grid_value_bounds(g_r, n_r, d_r)) return false;

      umax den, a, b, sum;
      if (mul_overflow(d_l, d_r, &den)) return false;
      if (den > static_cast<umax>(std::numeric_limits<imax>::max())) return false;
      if (mul_overflow(n_l, d_r, &a)) return false;
      if (mul_overflow(n_r, d_l, &b)) return false;
      if (add_overflow(a, b, &sum)) return false;
      return true;
    }

    // Notch is a non-zero integer (denominator 1) — the grid is notch-aligned,
    // so values map 1:1 to integers. Gates the implicit imax/size_t conversions.
    template <grid G>
    inline constexpr bool notch_is_unit_integer =
      abs_den(G.Notch.Denominator) == 1 && G.Notch.Numerator != 0;

    // ONLY type conversion, NO value representation conversion calculation
    template <insidable B>
    [[nodiscard]] constexpr raw_t<B> raw_cast(auto value) noexcept
    {
      return static_cast<raw_t<B>>(value);
    }

    template <insidable B>
    [[nodiscard]] constexpr raw_t<B> raw_cast(rational value) noexcept
    {
      if constexpr (rational_raw<B>)
        return value;
      else
        return value.to<raw_t<B>>().value_or(0);
    }

    // Widen raw storage to imax. Distinct from `to_value(b)` for notch-stored
    // grids where raw is an index rather than a value — naming separates the
    // two intents that today both spell `static_cast<imax>`.
    template <insidable B>
    constexpr imax raw_imax(B b) noexcept { return static_cast<imax>(b.raw()); }

    //-------------------------------------------------------------------------
    // Q-format integer fast path: for grids with integer Lower, unit-numerator
    // Notch, and raw fitting imax, value↔raw is pure integer arithmetic. Shared
    // by operator rational(), from_value, and assignment::store.
    //-------------------------------------------------------------------------
    template <insidable B>
    inline constexpr bool has_qformat_fast_path =
        abs_den(lower_of<B>.Denominator) == 1
        && notch_of<B>.Numerator == 1
        && !rational_raw<B>
        && (std::signed_integral<raw_t<B>>
            || max_index_v<B> <= static_cast<umax>(std::numeric_limits<imax>::max()));

    // value → raw, integer math only. Pre: has_qformat_fast_path<B>.
    template <insidable B>
    constexpr raw_t<B> q_format_encode(imax value) noexcept
    {
      constexpr imax nd = abs_den(notch_of<B>.Denominator);
      return raw_cast<B>((value - lower_imax<B>) * nd);
    }

    // raw → rational, integer math only. Pre: has_qformat_fast_path<B>.
    template <insidable B>
    constexpr rational q_format_decode(B b) noexcept
    {
      constexpr imax nd = abs_den(notch_of<B>.Denominator);
      return rational{raw_imax(b) + lower_imax<B> * nd, nd};
    }

    // Library-internal extraction helper. Always succeeds (returns `imax`
    // unconditionally) but does not check the value fits in any narrower
    // target. User code should prefer `b.to<T>()`, which carries a typed
    // overflow error.
    template <insidable B>
    [[nodiscard]] constexpr imax to_value(B b) noexcept
    {
      if constexpr (!index_raw<B>)
        return raw_imax(b);
      else if constexpr (abs_den(notch_of<B>.Denominator) == 1 && abs_den(lower_of<B>.Denominator) == 1)
        return lower_imax<B> + raw_imax(b) * static_cast<imax>(notch_of<B>.Numerator);
      else if constexpr (has_qformat_fast_path<B>)
      {
        constexpr imax nd = abs_den(notch_of<B>.Denominator);
        return (raw_imax(b) + lower_imax<B> * nd) / nd;   // q_format_decode, truncated
      }
      else // index storage, generic rational path
        return trunc(as_rational(b));
    }

    template <insidable B>
    constexpr void from_value(B& b, imax val)
    {
      if constexpr (!index_raw<B>)
        b = B::from_raw(raw_cast<B>(val));
      else if constexpr (abs_den(notch_of<B>.Denominator) == 1 && abs_den(lower_of<B>.Denominator) == 1)
        b = B::from_raw(raw_cast<B>((val - lower_imax<B>) / static_cast<imax>(notch_of<B>.Numerator)));
      else if constexpr (has_qformat_fast_path<B>)
        b = B::from_raw(q_format_encode<B>(val));
      else // index storage, generic rational path
      {
        auto offset = (rational{val} - lower_of<B>) / notch_of<B>;
        b = B::from_raw(raw_cast<B>(offset.value().Numerator));
      }
    }

    // x mod m into [0, m) for m > 0 — one division (vs `((x % m) + m) % m`).
    [[nodiscard]] constexpr imax euclid_mod(imax x, imax m) noexcept
    { const imax r = x % m; return r < 0 ? r + m : r; }

    //-------------------------------------------------------------------------
    // raw_lo / raw_hi / raw_from_offset — map interval endpoints to raw space. For
    // notch-offset storage the raw is a 0-based index (raw_lo == 0); for direct
    // storage the raw IS the value (raw_lo == lower_imax<B>), so an offset needs
    // raw_lo<L> added back before storing.
    //-------------------------------------------------------------------------
    template <insidable B>
    inline constexpr imax raw_lo = !index_raw<B> ? lower_imax<B> : 0;

    template <insidable B>
    inline constexpr imax raw_hi = !index_raw<B> ? upper_imax<B> : static_cast<imax>(max_index_v<B>);

    template <insidable L>
    constexpr raw_t<L> raw_from_offset(umax offset) noexcept
    {
      if constexpr (!index_raw<L>)
        return raw_cast<L>(static_cast<imax>(offset) + raw_lo<L>);
      else
        return raw_cast<L>(offset);
    }

    template <insidable L>
    constexpr raw_t<L> raw_from_offset(imax offset) noexcept
    {
      if constexpr (!index_raw<L>)
        return raw_cast<L>(offset + raw_lo<L>);
      else
        return raw_cast<L>(static_cast<umax>(offset));
    }

    //-------------------------------------------------------------------------
    // is_integer_interval vs is_integer_aligned — easy to confuse, both needed.
    //   is_integer_interval<B>: Lower and Upper integer (Notch may be fractional,
    //     e.g. inside<{0,100}, 1/10>). Lets Lower/Upper be used as imax constants.
    //   is_integer_aligned<B>: Notch and Lower integer ⇒ is_integer_interval (not the
    //     converse). Precondition for native integer raw arithmetic (Raw == value).
    //-------------------------------------------------------------------------
    template <insidable B>
    inline constexpr bool is_integer_interval =
        abs_den(lower_of<B>.Denominator) == 1 && abs_den(upper_of<B>.Denominator) == 1;

    template <insidable B>
    inline constexpr bool is_integer_aligned =
        abs_den(notch_of<B>.Denominator) == 1 && abs_den(lower_of<B>.Denominator) == 1;

    // Q-format: the canonical fixed-point shape (Q8.8, Q16.16, ...). Notch has
    // unit numerator with integer denominator > 1, Lower is an integer at 0.
    // Value = Raw / Notch.Denominator. Used to gate the integer fast path for
    // fixed-point division, which would otherwise fall into the slow rational
    // route because Notch.Denominator > 1 disqualifies is_integer_aligned.
    template <insidable B>
    inline constexpr bool is_qformat =
           !rational_raw<B>
        && notch_of<B>.Numerator == 1
        && abs_den(notch_of<B>.Denominator) > 1
        && abs_den(lower_of<B>.Denominator) == 1
        && lower_of<B> == 0;

    // Policy test: checks both type-level and per-operation policy.
    // Composite flags (e.g. round_nearest = bit5 | snap) require all
    // their bits set — having a subset like just `snap` does NOT match.
    template <insidable B, typename P, policy_flag F>
    inline constexpr bool has_policy = has_flag(policy_of<B>, F) || plain_t<P>::test(F);

    // rounding_of (policy_flag.hpp) over L's type policy and the call's policy P.
    template <insidable L, typename P>
    inline constexpr round_mode rounding_for =
        has_policy<L, P, round_floor>     ? round_mode::floor
      : has_policy<L, P, round_ceil>      ? round_mode::ceil
      : has_policy<L, P, round_half_even> ? round_mode::half_even
      : has_policy<L, P, round_nearest>   ? round_mode::nearest
      :                                    round_mode::trunc;

    // v rounded onto L's lattice {k·Notch} by rounding_for<L, P> (value index,
    // ties half away from zero) — not limited to [Lower, Upper], so wrap can
    // round first and fold an on-lattice value after. Lower/Notch is an integer
    // on every valid grid, so the lattice points are exactly the grid's.
    template <insidable L, typename P>
    [[nodiscard]] constexpr rational round_to_lattice(rational v)
    {
      if constexpr (notch_of<L> == 0)
        return v;
      else
      {
        const rational qv = (v / notch_of<L>).value();
        constexpr round_mode m = rounding_for<L, P>;
        imax k;
        if constexpr (m == round_mode::nearest)    k = round(qv);
        else if constexpr (m == round_mode::floor) k = floor(qv);
        else if constexpr (m == round_mode::ceil)  k = ceil(qv);
        else if constexpr (m == round_mode::half_even)
        {
          const imax f = floor(qv);
          const rational frac = (qv - rational{f}).value();
          const rational half{1, 2};
          k = frac > half ? f + 1 : frac < half ? f : ((f & 1) ? f + 1 : f);
        }
        else                                       k = trunc(qv);
        return (rational{k} * notch_of<L>).value();
      }
    }

    // Round, then range-check. Lower and Upper are lattice points, so rounding
    // an in-range value keeps it in range; only an out-of-range value can change
    // outcome. When the policy may round (snap), rounds_into_range rounds such a
    // value and reports whether it lands inside the interval (`out` = the
    // rounded value). Only values within one notch of the interval can, which
    // also keeps round_to_lattice's division bounded for huge sources.
    template <insidable L, typename P>
    inline constexpr bool rounds_before_range_check =
        notch_of<L> != 0 && has_policy<L, P, snap>;

    template <insidable L, typename P>
    [[nodiscard]] constexpr bool rounds_into_range(rational v, rational& out)
    {
      if constexpr (!rounds_before_range_check<L, P>)
        return false;
      else
      {
        constexpr rational lo = (lower_of<L> - notch_of<L>).value_or(lower_of<L>);
        constexpr rational hi = (upper_of<L> + notch_of<L>).value_or(upper_of<L>);
        if (v <= lo || v >= hi)
          return false;
        out = round_to_lattice<L, P>(v);
        return includes(interval_of<L>, out);
      }
    }

    // Store-side form for the assignment paths: when v rounds inside, the raw of
    // the rounded lattice point (an exact in-range point: an index, rational or
    // double raw, no further rounding). Cold and out of line, and it returns the
    // raw in registers instead of writing through the caller's inside:
    //   - a second call site of the large store functions stops GCC inlining
    //     them into the hot path (~40 instructions per in-range store);
    //   - an escaping `lhs` address turns on the stack protector there (~3).
    template <insidable L> struct rounded_raw { raw_t<L> Raw; bool Ok; };

    template <insidable L, typename P>
    [[gnu::cold, gnu::noinline]] constexpr rounded_raw<L> raw_if_rounds_inside(rational v)
    {
      rational r;
      if (!rounds_into_range<L, P>(v, r))
        return {raw_t<L>{}, false};
      if constexpr (fp_raw<L>)
        return {static_cast<raw_t<L>>(static_cast<double>(r)), true};   // exact: fp-exact grid
      else if constexpr (rational_raw<L>)
        return {r, true};
      else
        return {raw_from_offset<L>(((r - lower_of<L>).value() / notch_of<L>).value().Numerator), true};
    }

    // Rounds the split offset quotient q + r/den (r < den ≤ imax_max) per L's
    // rounding policy — q/r form so no expression can overflow umax
    // (num + den/2 could, for num near umax). Shared by round_quotient's
    // offset rule and the 128-bit wide store (assignment.hpp).
    template <insidable L, typename P>
    [[nodiscard]] constexpr umax round_offset(umax q, umax r, umax den) noexcept
    {
      constexpr round_mode m = rounding_for<L, P>;
      if constexpr (m == round_mode::nearest)        return (r * 2 >= den) ? q + 1 : q;
      else if constexpr (m == round_mode::floor)     return q;
      else if constexpr (m == round_mode::ceil)      return (r != 0) ? q + 1 : q;
      else if constexpr (m == round_mode::half_even)
      {
        if (r * 2 < den) return q;
        if (r * 2 > den) return q + 1;
        return (q & 1) ? q + 1 : q;
      }
      else                                                 return q;
    }

    // Round the non-negative offset quotient num/den (den >= 1) to an integer
    // notch index per L's rounding policy.
    //
    // Tie/sign rules are in VALUE space, not offset space, so assigning a value
    // rounds it the same way dividing down to it does (detail::div_rounded is the
    // reference). The offset num/den is >= 0 (sign lost by subtracting Lower), so
    // we rebuild the signed value-index NUM = m·den + num (m = Lower/Notch), round
    // it like div_rounded, and return the offset J - m. m is integral on every
    // dyadic/integer-aligned/Q-format grid; otherwise fall back to offset rounding.
    template <insidable L, typename P>
    [[nodiscard]] constexpr umax round_quotient(umax num, umax den) noexcept
    {
      constexpr rational zl =
          (notch_of<L> == rational{0})
            ? rational{0}
            : (lower_of<L> / notch_of<L>).value_or(rational{0});
      constexpr bool vidx = (zl.Denominator == 1 || zl.Denominator == -1);
      constexpr imax m = vidx
          ? signed_numerator(zl)
          : imax{0};

      if constexpr (!vidx)
        return round_offset<L, P>(num / den, num % den, den);
      else
      {
        // Round the signed value-index NUM/di exactly like detail::div_rounded.
        // A numerator or m·di beyond imax (fp-derived sources on grids with
        // large |Lower·count|) cannot rebuild the signed index — fall back to
        // the offset rule, which differs only at exact ties on negative values.
        const imax di = static_cast<imax>(den);
        imax mdi, NUM;
        if (num > static_cast<umax>(std::numeric_limits<imax>::max())
            || mul_overflow(m, di, &mdi)
            || add_overflow(mdi, static_cast<imax>(num), &NUM)) [[unlikely]]
          return round_offset<L, P>(num / den, num % den, den);
        const imax t   = NUM / di;                 // C++ truncation toward zero
        const imax rr  = NUM % di;                 // sign of NUM, |rr| < di
        imax J;
        if (rr == 0)
          J = t;
        else
        {
          const bool neg = NUM < 0;
          const umax ar  = (rr < 0) ? ~static_cast<umax>(rr) + 1u
                                    :  static_cast<umax>(rr);
          const umax ab  = static_cast<umax>(di);  // ab - ar safe: 0 < ar < ab
          constexpr round_mode mode = rounding_for<L, P>;
          if constexpr (mode == round_mode::nearest)        // half away from zero
            J = (ar >= ab - ar) ? (neg ? t - 1 : t + 1) : t;
          else if constexpr (mode == round_mode::floor)     // toward -inf
            J = neg ? t - 1 : t;
          else if constexpr (mode == round_mode::ceil)      // toward +inf
            J = neg ? t : t + 1;
          else if constexpr (mode == round_mode::half_even) // tie -> even value
          {
            if      (ar < ab - ar) J = t;
            else if (ar > ab - ar) J = neg ? t - 1 : t + 1;
            else                   J = (t & 1) == 0 ? t : (neg ? t - 1 : t + 1);
          }
          else                                                 // snap: toward zero
            J = t;
        }
        return static_cast<umax>(J - m);           // offset index k = J - m (>= 0)
      }
    }

    // Forward decl — defined in assignment.hpp
    template <typename L, typename R> struct assignment;

    // A single-point source (Lower == Upper) carries one value, so the only
    // question is whether it lands on L's grid — admitting e.g. `3_ins` into
    // `{{0,9},3}` while rejecting `1_ins` and out-of-range points.
    template <typename L, typename R>
    inline constexpr bool point_exactly_assignable =
      (lower_of<R> == upper_of<R>) && grid_of<L>.representable(lower_of<R>);

    // Tail of the policy cascade: checked reports.
    // Returns true if a policy handled the failure (caller should return).
    // Cheap default — reports through the static category message (no string).
    template <insidable B, typename P>
    constexpr bool domain_fail([[maybe_unused]] B& b, P&& policy)
    {
      if (policy.domain_check())
      {
        policy.report(errc::domain_error);
        return true;
      }
      return false;
    }

    // The two non-trivial clauses of `inside_assignable`, named so the concept and
    // its `inside_assignable_why` diagnostic share one definition. Concepts (not
    // bools) so `||` short-circuits *instantiation* (e.g. assignment<L,R>::Factor
    // is never formed when R isn't insidable).
    template <typename L, typename R, policy_flag P = checked>
    concept assign_intervals_ok =
      (!insidable<R> && !std::integral<R>)
      // wrap/clamp bring any value into range, so a disjoint rhs interval is fine
      // for them (the integral-rhs path already allows it — int's interval is unbounded).
      || ((policy_of<L> | P) & (wrap | clamp)) != 0
      || not excludes(interval_of<L>, interval_of<R>);

    template <typename L, typename R, policy_flag P>
    concept assign_notch_ok =
      !insidable<R> || abs_den(assignment<L, R>::Factor.Denominator) == 1
      || ((policy_of<L> | P) & snap) != 0
      || point_exactly_assignable<L, R>;
  } // namespace detail

  // Compile-time prerequisites for L = R, gating three failure modes at the call
  // site: (1) R is numeric; (2) intervals overlap (typed-interval R only —
  // skipped for float/rational, which have no static interval); (3) integer
  // notch ratio or snap set (else R's notch doesn't divide L's; opt into
  // rounding). Named `inside_assignable` to avoid shadowing std::assignable_from.
  template <typename L, typename R, policy_flag P = checked>
  concept inside_assignable =
    numeric<R>
    && detail::assign_intervals_ok<L, R, P>
    && detail::assign_notch_ok<L, R, P>;

  // Diagnostic helper: instantiating `inside_assignable_why<L,R,P>` fires a named
  // static_assert per failed clause, so a developer can see which tripped. Backs
  // both the default-build diagnostic fallbacks in `inside` (core.hpp, gated by
  // `BEMAN_INSIDE_STRICT_SFINAE`) and the public `why_assignable` probe below.
  template <typename L, typename R, policy_flag P = checked>
  struct inside_assignable_why
  {
    // Collapse each clause to a plain bool *before* the static_assert. Asserting on
    // a concept-id makes GCC dump the whole satisfaction tree ("constraints not
    // satisfied / no operand of the disjunction…") on top of the message; a bool
    // condition prints just the message. Each clause is self-guarding (the inner
    // disjunctions gate `assignment<L,R>::Factor` on `insidable<R>`), so evaluating
    // all three unconditionally is safe even when R is not numeric.
    static constexpr bool is_numeric   = numeric<R>;
    static constexpr bool intervals_ok = detail::assign_intervals_ok<L, R, P>;
    static constexpr bool notch_ok     = detail::assign_notch_ok<L, R, P>;
    static_assert(is_numeric,
      "inside_assignable: rhs is not numeric (must be an inside or arithmetic type)");
    static_assert(intervals_ok,
      "inside_assignable: rhs interval lies entirely outside lhs interval and the policy "
      "(not wrap/clamp) cannot bring it into range — assignment can never succeed");
    static_assert(notch_ok,
      "inside_assignable: incompatible notches — use `with_snap()` or `policy<snap>()` to allow rounding");
    static constexpr bool value = inside_assignable<L, R, P>;
  };

  // Public manual probe: `static_assert(beman::inside::why_assignable<DstInside, decltype(src)>);`
  // emits the named per-clause reasons in any build — including a strict
  // (`BEMAN_INSIDE_STRICT_SFINAE`) build where the automatic in-`inside` fallbacks are absent.
  template <typename Dst, typename Src, policy_flag P = policy_of<Dst>>
  inline constexpr bool why_assignable =
    inside_assignable_why<Dst, std::remove_cvref_t<Src>, P>::value;
} // namespace beman::inside

#endif // BEMAN_INSIDE_GENERIC_HPP
