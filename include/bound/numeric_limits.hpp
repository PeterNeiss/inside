//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDnumericlimitsHPP
#define BNDnumericlimitsHPP

#include "bound/bound.hpp"

#include <limits>
#include <functional>

//---------------------------------------------------------------------------
// numeric_limits / hash — `std::` specialisations for `bound<G, P>`.
//
// `std::numeric_limits<bound<G, P>>` reports the *grid* bounds (Lower / Upper)
// rather than the raw type's limits — what generic code asking "what's the
// largest value this type can hold?" actually wants. `std::hash` hashes the
// single `Raw` member; for rational raw, Numerator and Denominator are mixed
// with a boost-style hash combine.
//---------------------------------------------------------------------------

template <bnd::grid G, bnd::policy_flag P>
struct std::numeric_limits<bnd::bound<G, P>>
{
  using B = bnd::bound<G, P>;

  static constexpr bool is_specialized = true;
  static constexpr bool is_signed      = (G.Interval.Lower < bnd::detail::rational{0});
  static constexpr bool is_integer     = (G.Notch == bnd::detail::rational{1});
  static constexpr bool is_exact       = true;     // rational + integer raw are both exact
  static constexpr bool is_bounded     = true;
  static constexpr bool is_modulo      = (P & bnd::wrap) != 0;
  static constexpr bool has_infinity   = false;
  static constexpr bool has_quiet_NaN  = false;
  static constexpr bool has_signaling_NaN = false;
  static constexpr bool traps          = (P & bnd::checked) != 0;
  static constexpr bool is_iec559      = false;
  static constexpr int  radix          = 2;
  static constexpr std::float_round_style round_style =
      ((P & bnd::round_nearest) == bnd::round_nearest) ? std::round_to_nearest
                                                       : std::round_toward_zero;

  // digits / digits10 forward to the raw type so generic algorithms see the
  // storage size, not the rational interval count.
  static constexpr int digits   = std::numeric_limits<bnd::detail::raw_t<B>>::digits;
  static constexpr int digits10 = std::numeric_limits<bnd::detail::raw_t<B>>::digits10;

  static constexpr B min()    noexcept { return B{G.Interval.Lower}; }
  static constexpr B max()    noexcept { return B{G.Interval.Upper}; }
  static constexpr B lowest() noexcept { return B{G.Interval.Lower}; }
  // Exact types have no rounding noise — epsilon and round_error are 0 when
  // 0 is on the grid (it always is when 0 ∈ interval, since the grid is
  // validated such that Lower is an integer multiple of Notch). When 0 is
  // outside the interval, fall back to the grid minimum — the closest
  // representable stand-in for "no error" the type can express.
  static constexpr B epsilon() noexcept
  {
    if constexpr (G.Interval.Lower <= bnd::detail::rational{0}
               && bnd::detail::rational{0} <= G.Interval.Upper)
      return B{bnd::detail::rational{0}};
    else
      return B{G.Interval.Lower};
  }
  static constexpr B round_error() noexcept { return epsilon(); }
};

template <bnd::grid G, bnd::policy_flag P>
struct std::hash<bnd::bound<G, P>>
{
  using B = bnd::bound<G, P>;

  constexpr std::size_t operator()(B const& b) const noexcept
  {
    if constexpr (bnd::detail::storage_of<B> == bnd::detail::storage::rational)
    {
      // Boost-style hash combine over (Numerator, Denominator).
      auto h1 = std::hash<bnd::umax>{}(b.raw().Numerator);
      auto h2 = std::hash<bnd::imax>{}(b.raw().Denominator);
      return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
    else
      return std::hash<bnd::detail::raw_t<B>>{}(b.raw());
  }
};

#endif // BNDnumericlimitsHPP
