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

  template <grid G>
  inline constexpr grid get_grid(bound<G> = {}) { return G; }

  template <grid G>
  inline constexpr interval get_interval(bound<G>) { return G.Interval; }

  template <grid G>
  inline constexpr rational get_lower(bound<G>) { return G.Interval.Lower; }

  template <grid G>
  inline constexpr rational get_upper(bound<G>) { return G.Interval.Upper; }

  template <grid G>
  inline constexpr rational get_notch(bound<G>) { return G.Notch; }

  template <typename B>
  concept boundable = requires(B b)
  {
    { get_grid(b) } -> std::same_as<grid>;
    typename B::raw_type;
    b.Raw;
  };

  template <boundable B>
  using negative = bound<-get_grid(B{})>;

  template <boundable B>
  using raw_t = typename B::raw_type;

  template <typename N>
  concept numeric = boundable<N> or arithmetic<N>;
} // namespace bnd

#endif // BNDcommonHPP
