//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
//TODO rename generic.hpp
#ifndef BNDcommonHPP
#define BNDcommonHPP

#define SLIM_OPTIONAL_LEAN_AND_MEAN
#include "slim/optional.hpp"

#include "bound/debug.hpp"
#include "bound/grid.hpp"

namespace bnd
{
  template<typename T>
  using plain = std::remove_cvref_t<T>;

  template <grid G = {{0, 0}, 0}> struct bound;

  template <typename B>
  concept boundable = requires { []<grid G>(bound<G>){}(B{}); };

  template <boundable B>
  using raw_t = typename B::raw_type;

  template <typename R>
  inline constexpr bool is_rational = std::is_same_v<R, rational>;

  template <boundable B>
  inline constexpr bool is_raw_rational = is_rational<raw_t<B>>;

  template <boundable B>
  inline constexpr grid Grid = []<grid G>(bound<G>){ return G; } (B{});

  template <boundable B>
  using negative = bound<-Grid<B>>;

  template <typename T>
  inline constexpr interval Interval = {0,0};

  template <boundable B>
  inline constexpr interval Interval<B> = []<grid G>(bound<G>){ return G.Interval; } (B{});

  template <std::integral I>
  inline constexpr interval Interval<I> =
      {std::numeric_limits<I>::lowest(), std::numeric_limits<I>::max()};

  template <boundable B>
  inline constexpr rational Lower = []<grid G>(bound<G>){ return G.Interval.Lower; } (B{});

  template <boundable B>
  inline constexpr rational Upper = []<grid G>(bound<G>){ return G.Interval.Upper; } (B{});

  template <boundable B>
  inline constexpr rational Notch = []<grid G>(bound<G>){ return G.Notch; } (B{});

  template <typename N>
  concept numeric = boundable<N> or arithmetic<N>;

  // ONLY type conversion, NO value representation conversion calculation
  template <boundable B>
  constexpr raw_t<B> raw_cast(auto value)
  {
    return static_cast<raw_t<B>>(value);
  }

  template <boundable B>
  constexpr raw_t<B> raw_cast(rational value) { return value.to<raw_t<B>>().value_or(0); }

  template <boundable B>
  inline constexpr raw_t<B> MaxNotch = (Notch<B> == 0) ?
    raw_cast<B>(0) : raw_cast<B>((Interval<B>/Notch<B>).value().Numerator);

  template <boundable B>
  inline constexpr umax OffsetLower = (Lower<B>/Notch<B>).value_or(0_r).Numerator;

  template <boundable B>
  inline constexpr umax OffsetUpper = (Upper<B>/Notch<B>).value_or(0_r).Numerator;
} // namespace bnd

#endif // BNDcommonHPP
