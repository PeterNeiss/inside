//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDcommonHPP
#define BNDcommonHPP

#include "bound/debug.hpp"
#include "bound/utility/grid.hpp"

namespace bnd
{
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

  template <grid G>
  inline constexpr interval get_interval(bound<G>) { return G.Interval; }

  template <grid G>
  inline constexpr rational get_lower(bound<G>) { return G.Interval.Lower; }

  template <grid G>
  inline constexpr rational get_upper(bound<G>) { return G.Interval.Upper; }

  template <grid G>
  inline constexpr rational get_notch(bound<G>) { return G.Notch; }

  template <typename N>
  concept numeric = boundable<N> or arithmetic<N>;

  // ONLY type conversion, NO value representation conversion calculation
  template <boundable B>
  constexpr raw_t<B> raw_cast(auto value) { return static_cast<raw_t<B>>(value); }

  template <boundable B>
  inline constexpr raw_t<B> max_notch(B b)
  {
    return (get_notch(b) == 0) ?
    raw_cast<B>(0) : raw_cast<B>((get_interval(b)/get_notch(b)).Numerator);
  }

  template <boundable B>
  inline constexpr umax offset_lower(B b)
  { return (get_notch(b) == 0) ? 0ull : (get_lower(b)/get_notch(b)).Numerator; }

  template <boundable B>
  inline constexpr umax offset_upper(B b)
  { return (get_notch(b) == 0) ? 0ull : (get_upper(b)/get_notch(b)).Numerator; }
} // namespace bnd

#endif // BNDcommonHPP
