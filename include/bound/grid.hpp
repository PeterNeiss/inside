//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDgridHPP
#define BNDgridHPP

#include "bound/rational.hpp"
#include "bound/interval.hpp"

#include <algorithm>

namespace bnd { struct grid; }

namespace slim
{
  template<>
  struct sentinel_traits<bnd::grid>
  {
    protected:
      static constexpr bnd::grid sentinel() noexcept;
      static constexpr bool is_sentinel(const bnd::grid& v) noexcept;
  };
} // namespace slim

namespace bnd
{
  //---------------------------------------------------------------------------
  // grid
  //---------------------------------------------------------------------------
  // Must be a structural type for template NTTP (only public members)
  //---------------------------------------------------------------------------
  // The grid has an interval space by notches
  // The interval must divide evenly by the notches.
  // If Notch is zero, all rationals in the interval are allowed, raw is not offset
  //---------------------------------------------------------------------------
  struct grid
  {
    interval Interval;
    rational Notch;

    grid() = default;
    constexpr grid(arithmetic auto lower, arithmetic auto upper, arithmetic auto notch)
      :grid{interval{lower, upper}, rational{notch}} { }
    constexpr grid(arithmetic auto lower, arithmetic auto upper)
      :grid{interval{lower, upper}, 1_r} { }
    constexpr grid(arithmetic auto lower)
      :grid{interval{lower, lower}, 0_r} { }
    constexpr grid(interval val, rational notch):Interval{val}, Notch{notch} { }

    template <auto G>
    static constexpr bool validate()
    {
      interval::validate<G.Interval>();
      static_assert(G.Interval.divides_evenly(G.Notch));
      static_assert(G.Notch == 0 || abs_den((G.Interval.Lower/G.Notch).value().Denominator) == 1);

      return true;
    }

    constexpr umax max_notch() const
    { return (Notch == 0_r) ? 0 : (Interval/Notch).value().Numerator; }

    constexpr rational low_per_notch() const
    { return (Interval.Lower/Notch).value_or(0_r); }

    constexpr rational up_per_notch() const
    { return (Interval.Upper/Notch).value_or(0_r); }


    // operator== be default for structural type
    constexpr bool operator==(const grid& rhs) const = default;
    constexpr grid operator-() const { return {-Interval, Notch}; }

    template <std::unsigned_integral Raw>
    constexpr Raw to_raw(rational value) const
    {
      //TODO: check safty of calculation
      rational raw = ((value - Interval.Lower)/Notch).value();
      return static_cast<Raw>(raw.Numerator / static_cast<umax>(raw.Denominator));
    }

    template <std::signed_integral Raw>
    constexpr Raw to_raw(rational value) const
    { return static_cast<Raw>(value); }

    template <std::same_as<rational> Raw>
    constexpr rational to_raw(rational value) const
    {
      return ((value - Interval.Lower)/Notch).value_or(value);
    }

    constexpr double raw_to_double(std::unsigned_integral auto raw) const
    { return static_cast<double>((*(raw*Notch) + Interval.Lower).value()); }

    constexpr double raw_to_double(std::signed_integral auto raw) const
    { return static_cast<double>(raw); }

    constexpr double raw_to_double(std::same_as<rational> auto raw) const
    {
      return (Notch == 0_r) ? static_cast<double>(raw) :
        static_cast<double>((*(raw*Notch) + Interval.Lower).value());
    }

    static constexpr grid make_sentinel() noexcept
    { return grid{interval{0_r, 0_r}, rational::make_sentinel()}; }
  };

  template <grid G>
  using storage_min =
    std::conditional_t<(G.Notch == 0_r), rational,
    std::conditional_t<(G.Interval.Lower < 0_r && G.Notch == 1_r),
      smallest_int_for<static_cast<imax>(G.Interval.Lower), static_cast<imax>(G.Interval.Upper)>,
      smallest_uint_for<G.max_notch()>>>;

  constexpr slim::optional<grid> operator+(const grid&, const grid&);
  constexpr slim::optional<grid> operator-(const grid&, const grid&);
  constexpr slim::optional<grid> operator*(const grid&, const grid&);

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<grid> operator+(const grid& lhs, const grid& rhs)
  {
    auto interval = lhs.Interval + rhs.Interval;
    if (!interval)
      return slim::nullopt;
    return grid{*interval, gcd(lhs.Notch, rhs.Notch)};
  }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<grid> operator-(const grid& lhs, const grid& rhs)
  {
    return operator+(lhs, -rhs);
  }

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<grid> operator*(const grid& lhs, const grid& rhs)
  {
    auto interval = lhs.Interval * rhs.Interval;
    auto notch = lhs.Notch * rhs.Notch;
    if (!interval || !notch)
      return slim::nullopt;
    return grid{*interval, *notch};
  }
} // namespace bnd

namespace slim
{
  constexpr bnd::grid sentinel_traits<bnd::grid>::sentinel() noexcept
  { return bnd::grid::make_sentinel(); }

  constexpr bool sentinel_traits<bnd::grid>::is_sentinel(const bnd::grid& v) noexcept
  { return v.Notch.Denominator == 0; }
} // namespace slim

#endif // BNDgridHPP
