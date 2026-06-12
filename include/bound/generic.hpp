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
// generic — type-level traits and predicates used everywhere else.
//
// Public grid/policy introspection (`Grid<B>`, `BoundPolicy<B>`, `Lower<B>`,
// `Upper<B>`, `Notch<B>`, `Interval<B>`) plus the `boundable`, `numeric`, and
// `bound_assignable` concepts. The storage-shape predicates and raw/value
// converters that expose how values are stored (`raw_t`, the `*_raw` predicates,
// `to_value`, `from_value`, the Q-format engine, …) are internal and live in
// `bnd::detail`.
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

    // Uniform rational view of a scalar or bound. For arithmetic N it
    // constructs rational{v}; for a bound it uses the implicit
    // operator rational(). Returns the internal rational, so it lives here;
    // core-header callers spell it `detail::as_rational`.
    template <numeric N>
    [[nodiscard]] constexpr rational as_rational(N v)
    {
      if constexpr (arithmetic<N>) return rational{v};
      else                         return v;
    }

    template <boundable B>
    using raw_t = typename B::raw_type;

    // How a bound's value lives in its raw storage — four disjoint encodings,
    // selected by the policy's representation flags (exact/real/direct/indexed,
    // widest-wins) or deduced from the grid when none is set (see
    // grid.hpp storage_pick). Read back here as predicates:
    //   rational_raw — raw IS the value, as a rational (`exact` policy, or
    //                  deduced for notch-zero grids).
    //   real_raw     — raw IS the value, as an IEEE-754 `double` (the `real`
    //                  policy under the default math engine; dyadic grids only).
    //   value_raw    — raw IS the value, as a plain integer (`direct` policy,
    //                  or deduced: notch 1 and signed, or unsigned with Lower 0).
    //   index_raw    — raw is a 0-based notch index; value = Lower + raw*Notch.
    // The first two read off the raw type alone; the integer pair additionally
    // consults the policy, mirroring storage_pick's choice exactly.
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

    // Ungated double view of any bound, for the `real` arithmetic arms. Unlike
    // the public `operator double()` (gated on a rounding policy flag), this is
    // always available — a `real` result may take operands of any policy,
    // including strict `just<>` constants. Kind-aware (NOT raw-signedness-based:
    // a `direct` bound with Lower >= 0 has an unsigned VALUE raw): everything
    // but index storage holds the value verbatim; an index decodes through the
    // grid.
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

    // True when R's interval cannot contain zero — i.e. division by an R can
    // never be division by zero. Decided purely from the grid, so `a / b` can
    // return a plain `bound` instead of `slim::optional<bound>` (see
    // detail/division.hpp). A point grid at 0 (Lower==Upper==0) is *not*
    // excluded — it straddles (== is) zero.
    template <boundable R>
    inline constexpr bool DivisorExcludesZero = (Lower<R> > 0) || (Upper<R> < 0);

    // Storage-agnostic int truncation of interval endpoints. Equivalent to
    // `static_cast<imax>(Lower<B>)` but expresses intent without a cast and
    // works for both storage kinds. Used by `from_value`, `RawLo`, and the
    // integer fast paths.
    template <boundable B>
    inline constexpr imax LowerImax = Lower<B>.trunc();

    template <boundable B>
    inline constexpr imax UpperImax = Upper<B>.trunc();

    template <boundable B>
    inline constexpr umax NotchCount = (Notch<B> == 0) ?
      0 : (Interval<B>/Notch<B>).value().Numerator;

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
    // Q-format integer fast path
    //
    // For grids with integer Lower, unit-numerator Notch (e.g. 1/256, 1/65536),
    // and raw fitting in `imax`, conversion between value and raw reduces to
    // pure integer arithmetic — no rational construction. Three call sites
    // (bound::operator rational(), from_value, assignment::store) share this
    // shape; centralised here as `q_format_encode` / `q_format_decode`.
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
    // RawLo / RawHi / raw_from_offset
    //
    // Raw-space bounds map interval endpoints to the raw representation. For
    // notch-offset storage the raw is a 0-based index, so `RawLo == 0`. For
    // direct storage the raw IS the value, so `RawLo == LowerImax<B>` — and
    // every "value as L-offset" needs `RawLo<L>` added back before storing.
    // Without this, e.g. `bound<{-40, 60}>{-rational{40}}` would record
    // Raw=0 instead of Raw=-40.
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
    //
    // IsIntegerInterval<B>: Lower and Upper are integers (denominator == ±1).
    //   Notch may still be fractional — e.g. bound<{0,100}, 1/10> qualifies.
    //   Used as a precondition for treating Lower/Upper as `imax` constants
    //   (e.g. integer range checks, modular wrap arithmetic over [Lo, Hi]).
    //
    // IsIntegerAligned<B>: Notch and Lower are integers. Under the grid's
    //   `Interval.divides_evenly(Notch)` invariant this implies Upper integer,
    //   so IsIntegerAligned ⇒ IsIntegerInterval. The converse does NOT hold.
    //   Used as a precondition for native integer arithmetic on raws (every
    //   notch step is exactly 1, so Raw == value).
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
    // Composite flags (e.g. round_nearest = bit5 | ignore_round) require all
    // their bits set — having a subset like just `ignore_round` does NOT match.
    template <boundable B, typename P, policy_flag F>
    inline constexpr bool HasPolicy = (BoundPolicy<B> & F) == F || plain<P>::test(F);

    // Round the exact non-negative offset quotient num/den (den >= 1) to an
    // integer notch index per L's effective rounding policy. round_nearest is
    // half-away-from-zero; round_half_even breaks ties to even; round_ceil rounds
    // up; round_floor — and any policy with no rounding flag — truncate (== floor
    // for a non-negative quotient). Shared by assignment's apply_clamp and
    // store_checked; arm order matches the original inline dispatch.
    template <boundable L, typename P>
    [[nodiscard]] constexpr umax round_quotient(umax num, umax den) noexcept
    {
      if constexpr (HasPolicy<L, P, round_nearest>)
        return (num + den / 2) / den;
      else if constexpr (HasPolicy<L, P, round_floor>)
        return num / den;
      else if constexpr (HasPolicy<L, P, round_ceil>)
        return (num + den - 1) / den;
      else if constexpr (HasPolicy<L, P, round_half_even>)
      {
        umax q = num / den, r = num % den;
        if (r * 2 < den) return q;
        if (r * 2 > den) return q + 1;
        return (q & 1) ? q + 1 : q;
      }
      else
        return num / den;
    }

    // Forward decl — defined in assignment.hpp
    template <typename L, typename R> struct assignment;

    // A single-point source bound (Lower == Upper) carries exactly one value,
    // so the whole-range notch-mapping test (`Factor.Denominator == 1`) is the
    // wrong question — the only thing that matters is whether that one value
    // lands on L's grid. This admits e.g. `0_b` / `3_b` into a `{{0,9},3}` grid
    // while still rejecting `1_b` (not on a notch) and out-of-range points.
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

    // The two non-trivial clauses of `bound_assignable`, named so the concept
    // and its `bound_assignable_why` diagnostic share one definition. These are
    // concepts (not constexpr bools) so the `||` short-circuits *instantiation*
    // — e.g. `assignment<L,R>::Factor` / `Lower<R>` are never formed when R is
    // not boundable.
    template <typename L, typename R>
    concept assign_intervals_ok =
      (!boundable<R> && !std::integral<R>) || not Interval<L>.excludes(Interval<R>);

    template <typename L, typename R, policy_flag P>
    concept assign_notch_ok =
      !boundable<R> || abs_den(assignment<L, R>::Factor.Denominator) == 1
      || ((BoundPolicy<L> | P) & ignore_round) != 0
      || point_exactly_assignable<L, R>;
  } // namespace detail

  // Compile-time prerequisites for L = R. Three clauses gate three failure
  // modes that should all be caught at the call site (not deep in a template
  // instantiation):
  //   1. R is numeric                              — anything else is a hard
  //                                                  type error.
  //   2. intervals overlap (typed-interval R only) — if R's full interval
  //                                                  lies wholly outside L's,
  //                                                  no assignment can succeed.
  //                                                  Skipped for floating-point
  //                                                  and rational R since they
  //                                                  have no static interval.
  //   3. integer notch ratio OR ignore_round set   — a non-integer
  //                                                  Factor.Denominator means
  //                                                  R's notch doesn't divide
  //                                                  L's; the user must opt
  //                                                  into rounding.
  // Named `bound_assignable` (not `assignable_from`) to avoid shadowing the
  // unrelated `std::assignable_from` from <concepts>.
  template <typename L, typename R, policy_flag P = checked>
  concept bound_assignable =
    numeric<R>
    && detail::assign_intervals_ok<L, R>
    && detail::assign_notch_ok<L, R, P>;

  // Diagnostic helper: when `bound_assignable` fails, instantiating
  // `bound_assignable_why<L, R, P>` produces individually-named static_asserts
  // for each clause, so the user sees which one tripped. Not used in normal
  // code paths — meant to be called from `static_assert(bound_assignable_why<L,R,P>::value)`
  // when a developer wants better diagnostics during local debugging.
  template <typename L, typename R, policy_flag P = checked>
  struct bound_assignable_why
  {
    static_assert(numeric<R>,
      "bound_assignable: rhs is not numeric (must be a bound or arithmetic type)");
    static_assert(detail::assign_intervals_ok<L, R>,
      "bound_assignable: rhs interval lies entirely outside lhs interval — assignment can never succeed");
    static_assert(detail::assign_notch_ok<L, R, P>,
      "bound_assignable: incompatible notches — use `with_truncate()` or `policy<ignore_round>()` to allow rounding");
    static constexpr bool value = bound_assignable<L, R, P>;
  };
} // namespace bnd

#endif // BNDgenericHPP
