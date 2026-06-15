//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDintervalHPP
#define BNDintervalHPP

#include "bound/lift.hpp"
#include "bound/detail/rational.hpp"

#include <algorithm>
#include <initializer_list>
#include <tuple>

namespace bnd { struct interval; }

namespace slim
{
  template<>
  struct sentinel_traits<bnd::interval>
  {
    protected:
      static constexpr bnd::interval sentinel() noexcept;
      static constexpr bool is_sentinel(const bnd::interval& v) noexcept;
  };
} // namespace slim

namespace bnd
{
  //---------------------------------------------------------------------------
  // interval — structural NTTP type (public members only) with inclusive Lower
  // and Upper bounds. Like `grid`, its operator+/-/*// computes result intervals
  // at compile time; division returns nullopt when the divisor straddles zero
  // (grid::operator/ re-runs on the two zero-free halves and unions them).
  //---------------------------------------------------------------------------
  struct interval
  {
    detail::rational Lower;
    detail::rational Upper;

    interval() = default;

    constexpr interval(detail::rational lower, detail::rational upper)
     :Lower{lower}, Upper{upper} { }
    constexpr interval(arithmetic auto lower, arithmetic auto upper)
     :Lower{lower}, Upper{upper} { }

    template <auto I>
    static constexpr bool validate()
    {
      static_assert(I.Lower <= I.Upper);
      return true;
    }

    constexpr bool operator==(const interval& rhs) const = default;
    constexpr interval operator-() const { return interval{-Upper, -Lower}; }

    constexpr bool divides_evenly(const detail::rational& notch) const
    { return bnd::detail::divides_evenly((Upper - Lower).value(), notch); }

    constexpr slim::optional<detail::rational> operator/(const detail::rational& notch) const
    { return (Upper - Lower) / notch; }
  };

  // Containment / disjointness — free functions over the public endpoints
  // (siblings of the binary interval operators below).
  [[nodiscard]] constexpr bool includes(interval const& iv, interval const& rhs)
  { return iv.Lower <= rhs.Lower && rhs.Upper <= iv.Upper; }

  [[nodiscard]] constexpr bool includes(interval const& iv, detail::rational const& r)
  { return iv.Lower <= r && r <= iv.Upper; }

  [[nodiscard]] constexpr bool includes(interval const& iv, arithmetic auto a)
  { return includes(iv, detail::rational{a}); }

  // `excludes` means *strictly disjoint* — the intervals share no value.
  // `!includes()` is weaker: it only rules out total containment, so two
  // overlapping intervals are `!includes` AND `!excludes`.
  [[nodiscard]] constexpr bool excludes(interval const& iv, interval const& rhs)
  { return rhs.Upper < iv.Lower || iv.Upper < rhs.Lower; }

  // The `includes(rhs, iv)` clause catches rhs wholly containing iv (where
  // neither rhs endpoint lands in iv, so the other checks would miss it).
  [[nodiscard]] constexpr bool overlaps(interval const& iv, interval const& rhs)
  { return includes(rhs, iv) || includes(iv, rhs.Lower) || includes(iv, rhs.Upper); }

  // The min/max hull of four endpoint combinations — the result interval of an
  // interval product or quotient (interval arithmetic's four-corner rule).
  namespace detail
  {
    constexpr interval corner_hull(rational a, rational b, rational c, rational d) noexcept
    {
      auto [lo, hi] = std::minmax({a, b, c, d});
      return interval{lo, hi};
    }
  }

  constexpr slim::optional<interval> operator+  (const interval&, const interval&);
  constexpr slim::optional<interval> operator-  (const interval&, const interval&);
  constexpr slim::optional<interval> operator*  (const interval&, const interval&);
  constexpr slim::optional<interval> operator/  (const interval&, const interval&);
  constexpr auto                     operator<=>(const interval&, const interval&) -> std::partial_ordering;

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<interval> operator+(const interval& lhs, const interval& rhs)
  {
    return lift(
      [](detail::rational l, detail::rational u){ return interval{l, u}; },
      lhs.Lower + rhs.Lower, lhs.Upper + rhs.Upper);
  }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<interval> operator-(const interval& lhs, const interval& rhs)
  {
    return operator+(lhs, -rhs);
  }

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<interval> operator*(const interval& lhs, const interval& rhs)
  {
    return lift(detail::corner_hull,
      lhs.Lower * rhs.Lower, lhs.Lower * rhs.Upper,
      lhs.Upper * rhs.Lower, lhs.Upper * rhs.Upper);
  }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<interval> operator/(const interval& lhs, const interval& rhs)
  {
    if (includes(rhs, 0))
      return slim::nullopt;

    return lift(detail::corner_hull,
      lhs.Lower / rhs.Lower, lhs.Lower / rhs.Upper,
      lhs.Upper / rhs.Lower, lhs.Upper / rhs.Upper);
  }

  //---------------------------------------------------------------------------
  // operator<=>
  //---------------------------------------------------------------------------
  inline constexpr auto operator<=>(const interval& lhs, const interval& rhs) -> std::partial_ordering
  {
    if (lhs.Upper < rhs.Lower)
      return std::partial_ordering::less;

    if (lhs.Lower > rhs.Upper)
      return std::partial_ordering::greater;

    if (lhs.Lower == rhs.Lower && lhs.Upper == rhs.Upper)
      return std::partial_ordering::equivalent;

    return std::partial_ordering::unordered;
  }

} // namespace bnd

namespace slim
{
  constexpr bnd::interval sentinel_traits<bnd::interval>::sentinel() noexcept
  { return bnd::interval{bnd::detail::rational::make_sentinel(), bnd::detail::rational::make_sentinel()}; }

  constexpr bool sentinel_traits<bnd::interval>::is_sentinel(const bnd::interval& v) noexcept
  { return v.Lower.Denominator == 0; }
} // namespace slim

//---------------------------------------------------------------------------
// Structured bindings: `auto [lo, hi] = interval{...};`
//---------------------------------------------------------------------------
template <> struct std::tuple_size<bnd::interval> : std::integral_constant<std::size_t, 2> {};
template <std::size_t I> struct std::tuple_element<I, bnd::interval> { using type = bnd::detail::rational; };

namespace bnd
{
  template <std::size_t I, class Iv>
    requires std::same_as<std::remove_cvref_t<Iv>, bnd::interval>
  constexpr auto&& get(Iv&& iv) noexcept
  {
    if constexpr (I == 0) return std::forward<Iv>(iv).Lower;
    else                  return std::forward<Iv>(iv).Upper;
  }
}

#endif // BNDintervalHPP
