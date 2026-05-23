//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDgenericHPP
#define BNDgenericHPP

#define SLIM_OPTIONAL_LEAN_AND_MEAN
#include "slim/optional.hpp"

#include "bound/debug.hpp"
#include "bound/grid.hpp"
#include "bound/policy_flag.hpp"

//---------------------------------------------------------------------------
// generic — type-level traits and predicates used everywhere else.
//
// Forward-declares `bound<G, P>` and pulls out its template parameters as
// reusable variable templates: `Grid<B>`, `BoundPolicy<B>`, `Lower<B>`,
// `Upper<B>`, `Notch<B>`, `Interval<B>`, plus the storage shape (`raw_t<B>`,
// `is_raw_rational`, `is_direct_storage`, `is_notch_storage`) and the raw/value
// converters (`raw_cast`, `to_value`, `from_value`). Also defines the
// `boundable`, `numeric`, and `bound_assignable` concepts that arithmetic
// and assignment specialisations use to filter overloads.
//---------------------------------------------------------------------------
namespace bnd
{
  template<typename T>
  using plain = std::remove_cvref_t<T>;

  template <grid G = {{0, 0}, 0}, policy_flag P = checked> struct bound;

  template <typename B>
  concept boundable = !arithmetic<B> && requires { []<grid G, policy_flag P>(bound<G, P>){}(B{}); };

  template <boundable B>
  using raw_t = typename B::raw_type;

  template <boundable B>
  inline constexpr bool is_raw_rational = std::is_same_v<raw_t<B>, rational>;

  template <boundable B>
  inline constexpr grid Grid = []<grid G, policy_flag P>(bound<G, P>){ return G; } (B{});

  template <boundable B>
  inline constexpr policy_flag BoundPolicy = []<grid G, policy_flag P>(bound<G, P>){ return P; } (B{});

  template <boundable B>
  using negative = bound<-Grid<B>, BoundPolicy<B>>;

  template <typename T>
  inline constexpr interval Interval = {0,0};

  template <boundable B>
  inline constexpr interval Interval<B> = []<grid G, policy_flag P>(bound<G, P>){ return G.Interval; } (B{});

  template <std::integral I>
  inline constexpr interval Interval<I> =
      {std::numeric_limits<I>::lowest(), std::numeric_limits<I>::max()};

  template <boundable B>
  inline constexpr rational Lower = []<grid G, policy_flag P>(bound<G, P>){ return G.Interval.Lower; } (B{});

  template <boundable B>
  inline constexpr rational Upper = []<grid G, policy_flag P>(bound<G, P>){ return G.Interval.Upper; } (B{});

  template <boundable B>
  inline constexpr rational Notch = []<grid G, policy_flag P>(bound<G, P>){ return G.Notch; } (B{});

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

  // True when `Raw` (and intermediate offset products) fit in `imax`.
  // Q-format integer fast paths widen Raw to imax; for unsigned 64-bit raw
  // (e.g. the result type of Q16.16 × Q16.16) that widening can wrap, so the
  // fast path must fall back to the rational route when this is false.
  template <boundable B>
  inline constexpr bool raw_fits_in_imax =
      not is_raw_rational<B>
      && (std::signed_integral<raw_t<B>>
          || NotchCount<B> <= static_cast<umax>(std::numeric_limits<imax>::max()));

  template <typename N>
  concept numeric = boundable<N> or arithmetic<N>;

  // ONLY type conversion, NO value representation conversion calculation
  template <boundable B>
  constexpr raw_t<B> raw_cast(auto value)
  {
    return static_cast<raw_t<B>>(value);
  }

  template <boundable B>
  constexpr raw_t<B> raw_cast(rational value)
  {
    if constexpr (is_raw_rational<B>)
      return value;
    else
      return value.to<raw_t<B>>().value_or(0);
  }

  // Raw == value: no offset arithmetic needed.
  //   - is_raw_rational           — value stored verbatim as rational.
  //   - Notch == 1 && Lower == 0  — offset is zero, value = Raw.
  //   - Notch == 1 && signed raw  — signed types hold the value directly
  //                                 across the whole range (offset would
  //                                 be redundant; matches native int perf).
  template <boundable B>
  inline constexpr bool is_direct_storage =
      is_raw_rational<B>
      || (Notch<B> == 1_r && (Lower<B> == 0_r || std::signed_integral<raw_t<B>>));

  // Raw is a notch index from Lower: value = Raw * Notch + Lower
  template <boundable B>
  inline constexpr bool is_notch_storage = !is_direct_storage<B>;

  // Widen raw storage to imax. Distinct from `to_value(b)` for notch-stored
  // grids where raw is an index rather than a value — naming separates the
  // two intents that today both spell `static_cast<imax>`.
  template <boundable B>
  constexpr imax signed_raw(B b) noexcept { return static_cast<imax>(b.Raw); }

  //---------------------------------------------------------------------------
  // Q-format integer fast path
  //
  // For grids with integer Lower, unit-numerator Notch (e.g. 1/256, 1/65536),
  // and raw fitting in `imax`, conversion between value and raw reduces to
  // pure integer arithmetic — no rational construction. Three call sites
  // (bound::operator rational(), from_value, assignment::store) share this
  // shape; centralised here as `q_format_encode` / `q_format_decode`.
  //---------------------------------------------------------------------------
  template <boundable B>
  inline constexpr bool has_q_format_fast_path =
      abs_den(Lower<B>.Denominator) == 1
      && Notch<B>.Numerator == 1
      && raw_fits_in_imax<B>;

  // value → raw, integer math only. Pre: has_q_format_fast_path<B>.
  template <boundable B>
  constexpr raw_t<B> q_format_encode(imax value) noexcept
  {
    constexpr imax nd = abs_den(Notch<B>.Denominator);
    return raw_cast<B>((value - LowerImax<B>) * nd);
  }

  // raw → rational, integer math only. Pre: has_q_format_fast_path<B>.
  template <boundable B>
  constexpr rational q_format_decode(B b) noexcept
  {
    constexpr imax nd = abs_den(Notch<B>.Denominator);
    return rational{signed_raw(b) + LowerImax<B> * nd, nd};
  }

  template <boundable B>
  constexpr imax to_value(B b)
  {
    if constexpr (is_direct_storage<B>)
      return signed_raw(b);
    else // is_notch_storage
      return as_rational(b).trunc();
  }

  template <boundable B>
  constexpr void from_value(B& b, imax val)
  {
    if constexpr (is_direct_storage<B>)
      b.Raw = raw_cast<B>(val);
    else if constexpr (has_q_format_fast_path<B>)
      b.Raw = q_format_encode<B>(val);
    else // is_notch_storage, generic rational path
    {
      auto offset = (rational{val} - Lower<B>) / Notch<B>;
      b.Raw = raw_cast<B>(offset.value().Numerator);
    }
  }

  template <boundable B>
  inline constexpr umax LowerIndex = (Lower<B>/Notch<B>).value_or(0_r).Numerator;

  template <boundable B>
  inline constexpr umax UpperIndex = (Upper<B>/Notch<B>).value_or(0_r).Numerator;

  //---------------------------------------------------------------------------
  // RawLo / RawHi / raw_from_offset
  //
  // Raw-space bounds map interval endpoints to the raw representation. For
  // notch-offset storage the raw is a 0-based index, so `RawLo == 0`. For
  // direct storage the raw IS the value, so `RawLo == LowerImax<B>` — and
  // every "value as L-offset" needs `RawLo<L>` added back before storing.
  // Without this, e.g. `bound<{-40, 60}>{rational{-40}}` would record
  // Raw=0 instead of Raw=-40.
  //---------------------------------------------------------------------------
  template <boundable B>
  inline constexpr imax RawLo = is_direct_storage<B> ? LowerImax<B> : 0;

  template <boundable B>
  inline constexpr imax RawHi = is_direct_storage<B> ? UpperImax<B> : static_cast<imax>(NotchCount<B>);

  template <boundable L>
  constexpr raw_t<L> raw_from_offset(umax offset) noexcept
  {
    if constexpr (is_direct_storage<L>)
      return raw_cast<L>(static_cast<imax>(offset) + RawLo<L>);
    else
      return raw_cast<L>(offset);
  }

  template <boundable L>
  constexpr raw_t<L> raw_from_offset(imax offset) noexcept
  {
    if constexpr (is_direct_storage<L>)
      return raw_cast<L>(offset + RawLo<L>);
    else
      return raw_cast<L>(static_cast<umax>(offset));
  }

  //---------------------------------------------------------------------------
  // is_integer_interval vs is_integer_aligned — easy to confuse, both needed.
  //
  // is_integer_interval<B>: Lower and Upper are integers (denominator == ±1).
  //   Notch may still be fractional — e.g. bound<{0,100}, 1/10> qualifies.
  //   Used as a precondition for treating Lower/Upper as `imax` constants
  //   (e.g. integer range checks, modular wrap arithmetic over [Lo, Hi]).
  //
  // is_integer_aligned<B>: Notch and Lower are integers. Under the grid's
  //   `Interval.divides_evenly(Notch)` invariant this implies Upper integer,
  //   so is_integer_aligned ⇒ is_integer_interval. The converse does NOT hold.
  //   Used as a precondition for native integer arithmetic on raws (every
  //   notch step is exactly 1, so Raw == value).
  //---------------------------------------------------------------------------
  template <boundable B>
  inline constexpr bool is_integer_interval =
      abs_den(Lower<B>.Denominator) == 1 && abs_den(Upper<B>.Denominator) == 1;

  template <boundable B>
  inline constexpr bool is_integer_aligned =
      abs_den(Notch<B>.Denominator) == 1 && abs_den(Lower<B>.Denominator) == 1;

  // Q-format: the canonical fixed-point shape (Q8.8, Q16.16, ...). Notch has
  // unit numerator with integer denominator > 1, Lower is an integer at 0.
  // Value = Raw / Notch.Denominator. Used to gate the integer fast path for
  // fixed-point division, which would otherwise fall into the slow rational
  // route because Notch.Denominator > 1 disqualifies is_integer_aligned.
  template <boundable B>
  inline constexpr bool is_q_format =
         not is_raw_rational<B>
      && Notch<B>.Numerator == 1
      && abs_den(Notch<B>.Denominator) > 1
      && abs_den(Lower<B>.Denominator) == 1
      && Lower<B> == 0_r;

  // Policy test: checks both type-level and per-operation policy.
  // Composite flags (e.g. round_nearest = bit5 | ignore_round) require all
  // their bits set — having a subset like just `ignore_round` does NOT match.
  template <boundable B, typename P, policy_flag F>
  inline constexpr bool HasPolicy = (BoundPolicy<B> & F) == F || plain<P>::test(F);

  // Forward decl — defined in assignment.hpp
  template <typename L, typename R> struct assignment;

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
    && ((!boundable<R> && !std::integral<R>) || not Interval<L>.excludes(Interval<R>))
    && (!boundable<R> || abs_den(assignment<L, R>::Factor.Denominator) == 1
        || ((BoundPolicy<L> | P) & ignore_round) != 0);

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
    static_assert((!boundable<R> && !std::integral<R>) || not Interval<L>.excludes(Interval<R>),
      "bound_assignable: rhs interval lies entirely outside lhs interval — assignment can never succeed");
    static_assert(!boundable<R> || abs_den(assignment<L, R>::Factor.Denominator) == 1
                  || ((BoundPolicy<L> | P) & ignore_round) != 0,
      "bound_assignable: incompatible notches — use `with_round()` or `policy<ignore_round>()` to allow rounding");
    static constexpr bool value = bound_assignable<L, R, P>;
  };

  template <boundable B>
  constexpr raw_t<B> sentinel_raw()
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
      b.Raw = sentinel_raw<B>();
      return true;
    }
    else if (policy.domain_check())
    {
      policy.report(errc::domain_error, std::move(msg));
      return true;
    }
    return false;
  }
} // namespace bnd

#endif // BNDgenericHPP
