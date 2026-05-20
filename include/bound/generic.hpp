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
// `IsRawRational`, `IsDirectStorage`, `IsNotchStorage`) and the raw/value
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
  inline constexpr bool IsRawRational = std::is_same_v<raw_t<B>, rational>;

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
    if constexpr (IsRawRational<B>)
      return value;
    else
      return value.to<raw_t<B>>().value_or(0);
  }

  // Raw == value: no offset arithmetic needed.
  //   - IsRawRational           — value stored verbatim as rational.
  //   - Notch == 1 && Lower == 0  — offset is zero, value = Raw.
  //   - Notch == 1 && signed raw  — signed types hold the value directly
  //                                 across the whole range (offset would
  //                                 be redundant; matches native int perf).
  template <boundable B>
  inline constexpr bool IsDirectStorage =
      IsRawRational<B>
      || (Notch<B> == 1_r && (Lower<B> == 0_r || std::signed_integral<raw_t<B>>));

  // Raw is a notch index from Lower: value = Raw * Notch + Lower
  template <boundable B>
  inline constexpr bool IsNotchStorage = !IsDirectStorage<B>;

  template <boundable B>
  constexpr imax to_value(B b)
  {
    if constexpr (IsDirectStorage<B>)
      return static_cast<imax>(b.Raw);
    else // IsNotchStorage
      return static_cast<imax>(static_cast<rational>(b));
  }

  template <boundable B>
  constexpr void from_value(B& b, imax val)
  {
    if constexpr (IsDirectStorage<B>)
      b.Raw = raw_cast<B>(val);
    // Integer fast path mirroring assignment.hpp:store — for grids with
    // integer Lower (den == 1) and unit-numerator Notch (e.g. 1/256, 1/16384)
    // the offset reduces to (val - lower) * notch_denominator.
    else if constexpr (abs_den(Lower<B>.Denominator) == 1
                       && Notch<B>.Numerator == 1)
    {
      constexpr imax lower_int = (Lower<B>.Denominator < 0)
        ? -static_cast<imax>(Lower<B>.Numerator)
        :  static_cast<imax>(Lower<B>.Numerator);
      constexpr imax nd = abs_den(Notch<B>.Denominator);   // Notch.Numerator == 1
      b.Raw = raw_cast<B>((val - lower_int) * nd);
    }
    else // IsNotchStorage, generic rational path
    {
      auto offset = (rational{val} - Lower<B>) / Notch<B>;
      b.Raw = raw_cast<B>(offset.value().Numerator);
    }
  }

  template <boundable B>
  inline constexpr umax NotchCount = (Notch<B> == 0) ?
    0 : (Interval<B>/Notch<B>).value().Numerator;

  //---------------------------------------------------------------------------
  // direct_lower_imax / raw_from_offset
  //
  // Bridge between "L-offset" (the natural product of the offset-encoded
  // arithmetic in assignment.hpp) and "L.Raw" (what gets stored). For
  // notch-offset storage they're identical. For direct storage Raw is the
  // *value*, so Raw = offset + Lower<L> — without this adjustment, a
  // signed-direct bound silently stores the offset, e.g. `bound<{-40, 60}>`
  // built from `rational{-40}` would record Raw=0 instead of Raw=-40.
  //---------------------------------------------------------------------------
  template <boundable B>
  inline constexpr imax direct_lower_imax = []{
    if constexpr (IsDirectStorage<B>)
      return (Lower<B>.Denominator < 0)
          ? -static_cast<imax>(Lower<B>.Numerator)
          :  static_cast<imax>(Lower<B>.Numerator);
    else
      return imax{0};
  }();

  template <boundable L>
  constexpr raw_t<L> raw_from_offset(umax offset) noexcept
  {
    if constexpr (IsDirectStorage<L>)
      return raw_cast<L>(static_cast<imax>(offset) + direct_lower_imax<L>);
    else
      return raw_cast<L>(offset);
  }

  template <boundable L>
  constexpr raw_t<L> raw_from_offset(imax offset) noexcept
  {
    if constexpr (IsDirectStorage<L>)
      return raw_cast<L>(offset + direct_lower_imax<L>);
    else
      return raw_cast<L>(static_cast<umax>(offset));
  }

  template <boundable B>
  inline constexpr umax LowerIndex = (Lower<B>/Notch<B>).value_or(0_r).Numerator;

  template <boundable B>
  inline constexpr umax UpperIndex = (Upper<B>/Notch<B>).value_or(0_r).Numerator;

  // Raw-space bounds: maps interval endpoints to raw representation
  template <boundable B>
  inline constexpr imax RawLo = IsDirectStorage<B> ? static_cast<imax>(Lower<B>) : 0;

  template <boundable B>
  inline constexpr imax RawHi = IsDirectStorage<B> ? static_cast<imax>(Upper<B>) : static_cast<imax>(NotchCount<B>);

  // Interval bounds are integers (denominator == ±1)
  template <boundable B>
  inline constexpr bool IsIntegerInterval =
      abs_den(Lower<B>.Denominator) == 1 && abs_den(Upper<B>.Denominator) == 1;

  // Notch and Lower both have integer denominators — enables native integer arithmetic
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
         not IsRawRational<B>
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
