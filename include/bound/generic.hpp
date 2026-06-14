//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDgenericHPP
#define BNDgenericHPP

#define SLIM_OPTIONAL_LEAN_AND_MEAN
#include "slim/optional.hpp"

#include "bound/detail/debug.hpp"
#include "bound/grid.hpp"
#include "bound/policy_flag.hpp"

//---------------------------------------------------------------------------
// generic — type-level traits and predicates used everywhere else. Public
// grid/policy introspection (`Grid<B>`, `BoundPolicy<B>`, `Lower/Upper/Notch<B>`,
// `Interval<B>`) plus the `boundable`/`numeric`/`bound_assignable` concepts; the
// storage-shape predicates and raw/value converters are internal (`bnd::detail`).
//---------------------------------------------------------------------------
namespace bnd
{
  template <grid G = {{0, 0}, 0}, policy_flag P = checked> struct bound;

  template <class>                 inline constexpr bool is_bound_v = false;
  template <grid G, policy_flag P> inline constexpr bool is_bound_v<bound<G, P>> = true;

  template <typename B>
  concept boundable = is_bound_v<std::remove_cvref_t<B>>;

  //---------------------------------------------------------------------------
  // Public grid/policy introspection — extract a bound's template parameters.
  // These mirror std::numeric_limits: they report what the grid is, used
  // opaquely (the rational return type is never named by callers).
  //---------------------------------------------------------------------------
  template <boundable B>
  inline constexpr grid Grid = []<grid G, policy_flag P>(bound<G, P>){ return G; } (B{});

  template <boundable B>
  inline constexpr policy_flag BoundPolicy = []<grid G, policy_flag P>(bound<G, P>){ return P; } (B{});

  template <typename T>
  inline constexpr interval Interval = {0,0};

  template <boundable B>
  inline constexpr interval Interval<B> = []<grid G, policy_flag P>(bound<G, P>){ return G.Interval; } (B{});

  template <std::integral I>
  inline constexpr interval Interval<I> =
      {std::numeric_limits<I>::lowest(), std::numeric_limits<I>::max()};

  template <boundable B>
  inline constexpr bnd::detail::rational Lower = []<grid G, policy_flag P>(bound<G, P>){ return G.Interval.Lower; } (B{});

  template <boundable B>
  inline constexpr bnd::detail::rational Upper = []<grid G, policy_flag P>(bound<G, P>){ return G.Interval.Upper; } (B{});

  template <boundable B>
  inline constexpr bnd::detail::rational Notch = []<grid G, policy_flag P>(bound<G, P>){ return G.Notch; } (B{});

  template <typename N>
  concept numeric = boundable<N> or arithmetic<N>;

  //---------------------------------------------------------------------------
  // Internal plumbing — storage shape, raw/value conversion, dispatch.
  //---------------------------------------------------------------------------
  namespace detail
  {
    template<typename T>
    using plain = std::remove_cvref_t<T>;

    // Always-false but template-dependent: lets a `static_assert` inside a
    // template body fire only when that template is actually instantiated
    // (e.g. the guidance overloads that make `bound + 1` ill-formed).
    template<typename...>
    inline constexpr bool dependent_false = false;

    // Uniform rational view of a scalar or bound (rational{v} / operator rational()).
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

    template <boundable B>
    using raw_t = typename B::raw_type;

    // How a bound's value lives in its raw storage — four disjoint encodings
    // (selected by policy flags or deduced; see grid.hpp storage_pick):
    //   rational_raw — raw IS the value, as a rational.
    //   real_raw     — raw IS the value, as an IEEE-754 double (dyadic grids only).
    //   value_raw    — raw IS the value, as a plain integer.
    //   index_raw    — raw is a 0-based notch index; value = Lower + raw*Notch.
    template <boundable B>
    inline constexpr bool real_raw = std::is_same_v<raw_t<B>, double>;

    template <boundable B>
    inline constexpr bool rational_raw = std::is_same_v<raw_t<B>, rational>;

    template <boundable B>
    inline constexpr bool value_raw =
         !real_raw<B> && !rational_raw<B>
      && ((BoundPolicy<B> & bnd::direct) == bnd::direct
          || ((BoundPolicy<B> & bnd::indexed) != bnd::indexed
              && Notch<B> == 1
              && (Lower<B> == 0 || std::signed_integral<raw_t<B>>)));

    template <boundable B>
    inline constexpr bool index_raw =
         !real_raw<B> && !rational_raw<B> && !value_raw<B>;

    // Ungated double view of any bound, for the `real` arithmetic arms (the
    // public operator double() is gated on a rounding flag; this is always
    // available). Everything but index storage holds the value verbatim; an
    // index decodes through the grid.
    template <boundable B>
    [[nodiscard]] constexpr double as_double(B const& b) noexcept
    {
      if constexpr (!index_raw<B>)
        return static_cast<double>(b.raw());
      else
        return static_cast<double>((*(b.raw() * Notch<B>) + Lower<B>).value());
    }

    template <boundable B>
    using negative = bound<-Grid<B>, BoundPolicy<B>>;

    // True when R's interval cannot contain zero — so `a / b` can return a plain
    // `bound` instead of `optional<bound>` (see detail/division.hpp). A point
    // grid at 0 is *not* excluded.
    template <boundable R>
    inline constexpr bool DivisorExcludesZero = (Lower<R> > 0) || (Upper<R> < 0);

    // Storage-agnostic int truncation of interval endpoints — intent-revealing
    // `static_cast<imax>(Lower<B>)`. Used by from_value, RawLo, the fast paths.
    template <boundable B>
    inline constexpr imax LowerImax = Lower<B>.trunc();

    template <boundable B>
    inline constexpr imax UpperImax = Upper<B>.trunc();

    template <boundable B>
    inline constexpr umax NotchCount = (Notch<B> == 0) ?
      0 : (Interval<B>/Notch<B>).value().Numerator;

    //-------------------------------------------------------------------------
    // grid_value_bounds / rational_mul_is_safe / rational_add_is_safe
    //
    // Conservative compile-time bound on the (numerator, denominator) of any
    // canonical value on a grid, and derived "can the rational op of two grid
    // values overflow imax" predicates — letting checked exact arithmetic drop
    // the optional wrapper when the grids prove no overflow is reachable.
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
    template <boundable B>
    constexpr raw_t<B> raw_cast(auto value)
    {
      return static_cast<raw_t<B>>(value);
    }

    template <boundable B>
    constexpr raw_t<B> raw_cast(rational value)
    {
      if constexpr (rational_raw<B>)
        return value;
      else
        return value.to<raw_t<B>>().value_or(0);
    }

    // Widen raw storage to imax. Distinct from `to_value(b)` for notch-stored
    // grids where raw is an index rather than a value — naming separates the
    // two intents that today both spell `static_cast<imax>`.
    template <boundable B>
    constexpr imax raw_imax(B b) noexcept { return static_cast<imax>(b.raw()); }

    //-------------------------------------------------------------------------
    // Q-format integer fast path: for grids with integer Lower, unit-numerator
    // Notch, and raw fitting imax, value↔raw is pure integer arithmetic. Shared
    // by operator rational(), from_value, and assignment::store.
    //-------------------------------------------------------------------------
    template <boundable B>
    inline constexpr bool HasQFormatFastPath =
        abs_den(Lower<B>.Denominator) == 1
        && Notch<B>.Numerator == 1
        && !rational_raw<B>
        && (std::signed_integral<raw_t<B>>
            || NotchCount<B> <= static_cast<umax>(std::numeric_limits<imax>::max()));

    // value → raw, integer math only. Pre: HasQFormatFastPath<B>.
    template <boundable B>
    constexpr raw_t<B> q_format_encode(imax value) noexcept
    {
      constexpr imax nd = abs_den(Notch<B>.Denominator);
      return raw_cast<B>((value - LowerImax<B>) * nd);
    }

    // raw → rational, integer math only. Pre: HasQFormatFastPath<B>.
    template <boundable B>
    constexpr rational q_format_decode(B b) noexcept
    {
      constexpr imax nd = abs_den(Notch<B>.Denominator);
      return rational{raw_imax(b) + LowerImax<B> * nd, nd};
    }

    // Library-internal extraction helper. Always succeeds (returns `imax`
    // unconditionally) but does not check the value fits in any narrower
    // target. User code should prefer `b.to<T>()`, which carries a typed
    // overflow error.
    template <boundable B>
    constexpr imax to_value(B b)
    {
      if constexpr (!index_raw<B>)
        return raw_imax(b);
      else // index storage
        return as_rational(b).trunc();
    }

    template <boundable B>
    constexpr void from_value(B& b, imax val)
    {
      if constexpr (!index_raw<B>)
        b = B::from_raw(raw_cast<B>(val));
      else if constexpr (HasQFormatFastPath<B>)
        b = B::from_raw(q_format_encode<B>(val));
      else // index storage, generic rational path
      {
        auto offset = (rational{val} - Lower<B>) / Notch<B>;
        b = B::from_raw(raw_cast<B>(offset.value().Numerator));
      }
    }

    //-------------------------------------------------------------------------
    // RawLo / RawHi / raw_from_offset — map interval endpoints to raw space. For
    // notch-offset storage the raw is a 0-based index (RawLo == 0); for direct
    // storage the raw IS the value (RawLo == LowerImax<B>), so an offset needs
    // RawLo<L> added back before storing.
    //-------------------------------------------------------------------------
    template <boundable B>
    inline constexpr imax RawLo = !index_raw<B> ? LowerImax<B> : 0;

    template <boundable B>
    inline constexpr imax RawHi = !index_raw<B> ? UpperImax<B> : static_cast<imax>(NotchCount<B>);

    template <boundable L>
    constexpr raw_t<L> raw_from_offset(umax offset) noexcept
    {
      if constexpr (!index_raw<L>)
        return raw_cast<L>(static_cast<imax>(offset) + RawLo<L>);
      else
        return raw_cast<L>(offset);
    }

    template <boundable L>
    constexpr raw_t<L> raw_from_offset(imax offset) noexcept
    {
      if constexpr (!index_raw<L>)
        return raw_cast<L>(offset + RawLo<L>);
      else
        return raw_cast<L>(static_cast<umax>(offset));
    }

    //-------------------------------------------------------------------------
    // IsIntegerInterval vs IsIntegerAligned — easy to confuse, both needed.
    //   IsIntegerInterval<B>: Lower and Upper integer (Notch may be fractional,
    //     e.g. bound<{0,100}, 1/10>). Lets Lower/Upper be used as imax constants.
    //   IsIntegerAligned<B>: Notch and Lower integer ⇒ IsIntegerInterval (not the
    //     converse). Precondition for native integer raw arithmetic (Raw == value).
    //-------------------------------------------------------------------------
    template <boundable B>
    inline constexpr bool IsIntegerInterval =
        abs_den(Lower<B>.Denominator) == 1 && abs_den(Upper<B>.Denominator) == 1;

    template <boundable B>
    inline constexpr bool IsIntegerAligned =
        abs_den(Notch<B>.Denominator) == 1 && abs_den(Lower<B>.Denominator) == 1;

    // Q-format: the canonical fixed-point shape (Q8.8, Q16.16, ...). Notch has
    // unit numerator with integer denominator > 1, Lower is an integer at 0.
    // Value = Raw / Notch.Denominator. Used to gate the integer fast path for
    // fixed-point division, which would otherwise fall into the slow rational
    // route because Notch.Denominator > 1 disqualifies IsIntegerAligned.
    template <boundable B>
    inline constexpr bool IsQFormat =
           !rational_raw<B>
        && Notch<B>.Numerator == 1
        && abs_den(Notch<B>.Denominator) > 1
        && abs_den(Lower<B>.Denominator) == 1
        && Lower<B> == 0;

    // Policy test: checks both type-level and per-operation policy.
    // Composite flags (e.g. round_nearest = bit5 | snapping) require all
    // their bits set — having a subset like just `snapping` does NOT match.
    template <boundable B, typename P, policy_flag F>
    inline constexpr bool HasPolicy = (BoundPolicy<B> & F) == F || plain<P>::test(F);

    // Round the non-negative offset quotient num/den (den >= 1) to an integer
    // notch index per L's rounding policy.
    //
    // Tie/sign rules are in VALUE space, not offset space, so assigning a value
    // rounds it the same way dividing down to it does (detail::div_rounded is the
    // reference). The offset num/den is >= 0 (sign lost by subtracting Lower), so
    // we rebuild the signed value-index NUM = m·den + num (m = Lower/Notch), round
    // it like div_rounded, and return the offset J - m. m is integral on every
    // dyadic/integer-aligned/Q-format grid; otherwise fall back to offset rounding.
    template <boundable L, typename P>
    [[nodiscard]] constexpr umax round_quotient(umax num, umax den) noexcept
    {
      constexpr rational zl =
          (Notch<L> == rational{0})
            ? rational{0}
            : (Lower<L> / Notch<L>).value_or(rational{0});
      constexpr bool vidx = (zl.Denominator == 1 || zl.Denominator == -1);
      constexpr imax m = vidx
          ? (zl.Denominator < 0 ? -static_cast<imax>(zl.Numerator)
                                :  static_cast<imax>(zl.Numerator))
          : imax{0};

      if constexpr (!vidx)
      {
        // Exotic Lower/Notch (no integral value index) — historical offset rule.
        if constexpr (HasPolicy<L, P, round_nearest>)        return (num + den / 2) / den;
        else if constexpr (HasPolicy<L, P, round_floor>)     return num / den;
        else if constexpr (HasPolicy<L, P, round_ceil>)      return (num + den - 1) / den;
        else if constexpr (HasPolicy<L, P, round_half_even>)
        {
          umax q = num / den, r = num % den;
          if (r * 2 < den) return q;
          if (r * 2 > den) return q + 1;
          return (q & 1) ? q + 1 : q;
        }
        else                                                 return num / den;
      }
      else
      {
        // Round the signed value-index NUM/di exactly like detail::div_rounded.
        const imax di  = static_cast<imax>(den);
        const imax NUM = m * di + static_cast<imax>(num);
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
          if constexpr (HasPolicy<L, P, round_nearest>)        // half away from zero
            J = (ar >= ab - ar) ? (neg ? t - 1 : t + 1) : t;
          else if constexpr (HasPolicy<L, P, round_floor>)     // toward -inf
            J = neg ? t - 1 : t;
          else if constexpr (HasPolicy<L, P, round_ceil>)      // toward +inf
            J = neg ? t : t + 1;
          else if constexpr (HasPolicy<L, P, round_half_even>) // tie -> even value
          {
            if      (ar < ab - ar) J = t;
            else if (ar > ab - ar) J = neg ? t - 1 : t + 1;
            else                   J = (t & 1) == 0 ? t : (neg ? t - 1 : t + 1);
          }
          else                                                 // snapping: toward zero
            J = t;
        }
        return static_cast<umax>(J - m);           // offset index k = J - m (>= 0)
      }
    }

    // Forward decl — defined in assignment.hpp
    template <typename L, typename R> struct assignment;

    // A single-point source (Lower == Upper) carries one value, so the only
    // question is whether it lands on L's grid — admitting e.g. `3_b` into
    // `{{0,9},3}` while rejecting `1_b` and out-of-range points.
    template <typename L, typename R>
    inline constexpr bool point_exactly_assignable =
      (Lower<R> == Upper<R>) && Grid<L>.representable(Lower<R>);

    template <boundable B>
    [[nodiscard]] constexpr raw_t<B> sentinel_raw()
    {
      if constexpr (std::is_same_v<raw_t<B>, rational>)
        return rational::make_sentinel();
      else if constexpr (std::signed_integral<raw_t<B>>)
        return std::numeric_limits<raw_t<B>>::min();
      else
        return std::numeric_limits<raw_t<B>>::max();
    }

    // Tail of the policy cascade: sentinel sets sentinel raw, checked reports.
    // Returns true if a policy handled the failure (caller should return).
    template <boundable B, typename P>
    constexpr bool domain_fail(B& b, P&& policy, std::string msg)
    {
      if constexpr (HasPolicy<B, P, sentinel>)
      {
        b = B::from_raw(sentinel_raw<B>());
        return true;
      }
      else if (policy.domain_check())
      {
        policy.report(errc::domain_error, std::move(msg));
        return true;
      }
      return false;
    }

    // The two non-trivial clauses of `bound_assignable`, named so the concept and
    // its `bound_assignable_why` diagnostic share one definition. Concepts (not
    // bools) so `||` short-circuits *instantiation* (e.g. assignment<L,R>::Factor
    // is never formed when R isn't boundable).
    template <typename L, typename R, policy_flag P = checked>
    concept assign_intervals_ok =
      (!boundable<R> && !std::integral<R>)
      // wrap/clamp bring any value into range, so a disjoint rhs interval is fine
      // for them (the integral-rhs path already allows it — int's interval is unbounded).
      || ((BoundPolicy<L> | P) & (wrap | clamp)) != 0
      || not Interval<L>.excludes(Interval<R>);

    template <typename L, typename R, policy_flag P>
    concept assign_notch_ok =
      !boundable<R> || abs_den(assignment<L, R>::Factor.Denominator) == 1
      || ((BoundPolicy<L> | P) & snapping) != 0
      || point_exactly_assignable<L, R>;
  } // namespace detail

  // Compile-time prerequisites for L = R, gating three failure modes at the call
  // site: (1) R is numeric; (2) intervals overlap (typed-interval R only —
  // skipped for float/rational, which have no static interval); (3) integer
  // notch ratio or snapping set (else R's notch doesn't divide L's; opt into
  // rounding). Named `bound_assignable` to avoid shadowing std::assignable_from.
  template <typename L, typename R, policy_flag P = checked>
  concept bound_assignable =
    numeric<R>
    && detail::assign_intervals_ok<L, R, P>
    && detail::assign_notch_ok<L, R, P>;

  // Diagnostic helper: instantiating `bound_assignable_why<L,R,P>` fires a named
  // static_assert per failed clause, so a developer can see which tripped. Not
  // used in normal code paths.
  template <typename L, typename R, policy_flag P = checked>
  struct bound_assignable_why
  {
    static_assert(numeric<R>,
      "bound_assignable: rhs is not numeric (must be a bound or arithmetic type)");
    static_assert(detail::assign_intervals_ok<L, R, P>,
      "bound_assignable: rhs interval lies entirely outside lhs interval and the policy "
      "(not wrap/clamp) cannot bring it into range — assignment can never succeed");
    static_assert(detail::assign_notch_ok<L, R, P>,
      "bound_assignable: incompatible notches — use `with_truncate()` or `policy<snapping>()` to allow rounding");
    static constexpr bool value = bound_assignable<L, R, P>;
  };
} // namespace bnd

#endif // BNDgenericHPP
