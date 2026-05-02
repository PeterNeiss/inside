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

  // Raw == value: no offset arithmetic needed
  template <boundable B>
  inline constexpr bool is_direct_storage =
      is_raw_rational<B>
      || (Notch<B> == 1_r && (Lower<B> == 0_r || std::signed_integral<raw_t<B>>));

  // Raw is a notch index from Lower: value = Raw * Notch + Lower
  template <boundable B>
  inline constexpr bool is_notch_storage = !is_direct_storage<B>;

  template <boundable B>
  constexpr imax to_value(B b)
  {
    if constexpr (is_direct_storage<B>)
      return static_cast<imax>(b.Raw);
    else // is_notch_storage
      return static_cast<imax>(static_cast<rational>(b));
  }

  template <boundable B>
  constexpr void from_value(B& b, imax val)
  {
    if constexpr (is_direct_storage<B>)
      b.Raw = raw_cast<B>(val);
    else // is_notch_storage
    {
      auto offset = (rational{val} - Lower<B>) / Notch<B>;
      b.Raw = raw_cast<B>(offset.value().Numerator);
    }
  }

  template <boundable B>
  inline constexpr umax NotchCount = (Notch<B> == 0) ?
    0 : (Interval<B>/Notch<B>).value().Numerator;

  template <boundable B>
  inline constexpr umax LowerIndex = (Lower<B>/Notch<B>).value_or(0_r).Numerator;

  template <boundable B>
  inline constexpr umax UpperIndex = (Upper<B>/Notch<B>).value_or(0_r).Numerator;

  // Raw-space bounds: maps interval endpoints to raw representation
  template <boundable B>
  inline constexpr imax RawLo = is_direct_storage<B> ? static_cast<imax>(Lower<B>) : 0;

  template <boundable B>
  inline constexpr imax RawHi = is_direct_storage<B> ? static_cast<imax>(Upper<B>) : static_cast<imax>(NotchCount<B>);

  // Interval bounds are integers (denominator == ±1)
  template <boundable B>
  inline constexpr bool is_integer_interval =
      abs_den(Lower<B>.Denominator) == 1 && abs_den(Upper<B>.Denominator) == 1;

  // Notch and Lower both have integer denominators — enables native integer arithmetic
  template <boundable B>
  inline constexpr bool is_integer_aligned =
      abs_den(Notch<B>.Denominator) == 1 && abs_den(Lower<B>.Denominator) == 1;

  // Policy test: checks both type-level and per-operation policy
  template <boundable B, typename P, policy_flag F>
  inline constexpr bool has_policy = (BoundPolicy<B> & F) || plain<P>::test(F);

  // Forward decl — defined in assignment.hpp
  template <typename L, typename R> struct assignment;

  // Compile-time prerequisites for L = R: intervals overlap (for typed-interval R),
  // and (for boundable R) the notch ratio is integral or rounding is accepted.
  // Skipped for floating_point/rational R since they have no static interval.
  template <typename L, typename R, policy_flag P = checked>
  concept assignable_from =
      numeric<R>
      && (!boundable<R> && !std::integral<R>
          || not Interval<L>.excludes(Interval<R>))
      && (!boundable<R>
          || abs_den(assignment<L, R>::Factor.Denominator) == 1
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
    if constexpr (has_policy<B, P, sentinel>)
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
