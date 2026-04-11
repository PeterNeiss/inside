//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDintervalHPP
#define BNDintervalHPP

#include "bound/rational.hpp"

#include <algorithm>
#include <initializer_list>

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
  // interval
  //---------------------------------------------------------------------------
  // Must be a structural type for template NTTP (only public members)
  //---------------------------------------------------------------------------
  // The interval has inclusive upper and lower bounds.
  // At least one rational is in the interval
  // The rationals are embedded in the interval with rational == Lower == Upper
  //---------------------------------------------------------------------------
  struct interval
  {
    rational Lower;
    rational Upper;

    interval() = default;

    constexpr interval(rational lower, rational upper)
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

    constexpr bool includes(interval const& rhs) const
    { return Lower <= rhs.Lower && rhs.Upper <= Upper; }

    constexpr bool includes(rational const& r) const
    { return Lower <= r && r <= Upper; }

    constexpr bool includes(arithmetic auto a) const
    { return includes(rational{a}); }

    // NOT equivalent to !includes()
    constexpr bool excludes(interval const& rhs) const
    { return rhs.Upper < Lower || Upper < rhs.Lower; }

    constexpr bool overlaps(interval const& rhs) const
    { return rhs.includes(*this) || includes(rhs.Lower) || includes(rhs.Upper); }

    constexpr bool divides_evenly(const rational& notch) const
    { return bnd::divides_evenly((Upper - Lower).value(), notch); }

    constexpr slim::optional<rational> operator/(const rational& notch) const
    { return (Upper - Lower) / notch; }
  };

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
    auto lower = lhs.Lower + rhs.Lower;
    auto upper = lhs.Upper + rhs.Upper;
    if (!lower || !upper)
      return slim::nullopt;
    return interval{*lower, *upper};
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
    auto a = lhs.Lower * rhs.Lower;
    auto b = lhs.Lower * rhs.Upper;
    auto c = lhs.Upper * rhs.Lower;
    auto d = lhs.Upper * rhs.Upper;
    if (!a || !b || !c || !d)
      return slim::nullopt;

    auto [lower, upper] = std::minmax({*a, *b, *c, *d});
    return interval{lower, upper};
  }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<interval> operator/(const interval& lhs, const interval& rhs)
  {
    if (rhs.includes(0))
      return slim::nullopt;

    auto a = lhs.Lower / rhs.Lower;
    auto b = lhs.Lower / rhs.Upper;
    auto c = lhs.Upper / rhs.Lower;
    auto d = lhs.Upper / rhs.Upper;
    if (!a || !b || !c || !d)
      return slim::nullopt;

    auto [lower, upper] = std::minmax({*a, *b, *c, *d});
    return interval{lower, upper};
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
  { return bnd::interval{bnd::rational::make_sentinel(), bnd::rational::make_sentinel()}; }

  constexpr bool sentinel_traits<bnd::interval>::is_sentinel(const bnd::interval& v) noexcept
  { return v.Lower.Denominator == 0; }
} // namespace slim

#endif // BNDintervalHPP
