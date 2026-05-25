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
  static constexpr bool is_signed      = (G.Interval.Lower < bnd::rational{0u});
  static constexpr bool is_integer     = (G.Notch == bnd::rational{1u});
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
  static constexpr int digits   = std::numeric_limits<bnd::raw_t<B>>::digits;
  static constexpr int digits10 = std::numeric_limits<bnd::raw_t<B>>::digits10;

  static constexpr B min()    noexcept { return B{G.Interval.Lower}; }
  static constexpr B max()    noexcept { return B{G.Interval.Upper}; }
  static constexpr B lowest() noexcept { return B{G.Interval.Lower}; }
  // epsilon / round_error are zero for an exact type — no rounding noise.
  static constexpr B epsilon()     noexcept { return B{G.Interval.Lower}; }
  static constexpr B round_error() noexcept { return B{G.Interval.Lower}; }
};

template <bnd::grid G, bnd::policy_flag P>
struct std::hash<bnd::bound<G, P>>
{
  using B = bnd::bound<G, P>;

  constexpr std::size_t operator()(B const& b) const noexcept
  {
    if constexpr (bnd::IsRawRational<B>)
    {
      // Boost-style hash combine over (Numerator, Denominator).
      auto h1 = std::hash<bnd::umax>{}(b.Raw.Numerator);
      auto h2 = std::hash<bnd::imax>{}(b.Raw.Denominator);
      return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
    else
      return std::hash<bnd::raw_t<B>>{}(b.Raw);
  }
};

#endif // BNDnumericlimitsHPP
