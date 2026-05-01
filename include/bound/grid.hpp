//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDgridHPP
#define BNDgridHPP

#include "bound/lift.hpp"
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
  constexpr slim::optional<grid> operator/(const grid&, const grid&);

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<grid> operator+(const grid& lhs, const grid& rhs)
  {
    return lift(
      [&](interval i){ return grid{i, gcd(lhs.Notch, rhs.Notch)}; },
      lhs.Interval + rhs.Interval);
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
    return lift(
      [](interval i, rational n){ return grid{i, n}; },
      lhs.Interval * rhs.Interval, lhs.Notch * rhs.Notch);
  }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<grid> operator/(const grid& lhs, const grid& rhs)
  {
    auto d = lhs.Interval / rhs.Interval;
    if (d.has_value())
      return grid{*d, 0_r};

    // Divisor interval includes zero — exclude zero for result interval.
    if (rhs.Interval.Lower == 0_r && rhs.Interval.Upper == 0_r)
      return slim::nullopt;

    rational step = (rhs.Notch != 0_r) ? abs(rhs.Notch) : 1_r;
    bool has_pos = 0_r < rhs.Interval.Upper;
    bool has_neg = 0_r > rhs.Interval.Lower;

    if (has_pos && has_neg)
    {
      return lift(
        [](interval pos, interval neg){
          return grid{interval{std::min(neg.Lower, pos.Lower),
                               std::max(neg.Upper, pos.Upper)}, 0_r};
        },
        lhs.Interval / interval{step, rhs.Interval.Upper},
        lhs.Interval / interval{rhs.Interval.Lower, -step});
    }
    else if (has_pos)
    {
      return lift([](interval i){ return grid{i, 0_r}; },
                  lhs.Interval / interval{step, rhs.Interval.Upper});
    }
    else
    {
      return lift([](interval i){ return grid{i, 0_r}; },
                  lhs.Interval / interval{rhs.Interval.Lower, -step});
    }
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
