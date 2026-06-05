//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDgridHPP
#define BNDgridHPP

#include "bound/lift.hpp"
#include "bound/rational.hpp"
#include "bound/interval.hpp"

#include "slim/expected.hpp"     // slim::expected, slim::unexpected

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
  // `grid` is an NTTP type whose operator+/-/*// is the engine of compile-time
  // result-grid inference for `bound`: every bound arithmetic operator computes
  // its result grid here, so the result's interval is guaranteed by
  // construction to contain every reachable value.
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

    // Runtime sibling of `validate<G>()`: mirrors the same invariants but
    // returns a typed error instead of failing a static_assert. Intended for
    // grids constructed from runtime config (parsed input, network, GUI).
    // The result is a value, so it cannot be used as a `bound<G, P>` template
    // argument — callers that need that path select a pre-declared grid
    // instantiation by other means.
    [[nodiscard]] static constexpr slim::expected<grid, errc>
    try_make(interval iv, rational notch)
    {
      if (iv.Lower > iv.Upper)
        return slim::unexpected{errc::domain_error};
      if (!iv.divides_evenly(notch))
        return slim::unexpected{errc::rounding_error};
      if (notch != 0)
      {
        auto q = iv.Lower / notch;
        if (!q.has_value())
          return slim::unexpected{errc::overflow};
        if (abs_den(q->Denominator) != 1)
          return slim::unexpected{errc::rounding_error};
      }
      return grid{iv, notch};
    }

    constexpr umax max_notch() const
    { return (Notch == 0) ? 0 : (Interval/Notch).value().Numerator; }

    // operator== be default for structural type
    constexpr bool operator==(const grid& rhs) const = default;
    constexpr grid operator-() const { return {-Interval, Notch}; }

    constexpr double raw_to_double(std::unsigned_integral auto raw) const
    { return static_cast<double>((*(raw*Notch) + Interval.Lower).value()); }

    constexpr double raw_to_double(std::signed_integral auto raw) const
    { return static_cast<double>(raw); }

    constexpr double raw_to_double(std::same_as<rational> auto raw) const
    {
      // storage_min only selects rational raw when Notch == 0, so raw is
      // already the value — no offset/scale to apply.
      return static_cast<double>(raw);
    }

    static constexpr grid make_sentinel() noexcept
    { return grid{interval{0_r, 0_r}, rational::make_sentinel()}; }
  };

  // Pick the smallest raw type that can hold every reachable index in G.
  // Order matters: notch-zero grids have no integer index space (rational
  // raw is the only option); signed-direct fits Lower < 0 with notch 1;
  // unsigned-offset (max_notch slots) is the fallback for everything else.
  template <grid G>
  using storage_min =
    std::conditional_t<(G.Notch == 0), rational,
    std::conditional_t<(G.Interval.Lower < 0 && G.Notch == 1),
      smallest_int_for<G.Interval.Lower.trunc(), G.Interval.Upper.trunc()>,
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
    // `gcd(rational, rational)` now returns optional — pass it through lift
    // so a notch-denominator overflow produces nullopt rather than a silently
    // wrapped result grid.
    return lift(
      [](interval i, rational n){ return grid{i, n}; },
      lhs.Interval + rhs.Interval, gcd(lhs.Notch, rhs.Notch));
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
    if (rhs.Interval.Lower == 0 && rhs.Interval.Upper == 0)
      return slim::nullopt;

    // `step` is the smallest non-zero magnitude the divisor can take. We use
    // it to split the divisor's interval into a positive side [step, Upper]
    // and a negative side [Lower, -step], skipping the zero gap. When both
    // sides are present the result is the *union* of the two sub-divisions.
    rational step = (rhs.Notch != 0) ? abs(rhs.Notch) : 1_r;
    bool has_pos = 0 < rhs.Interval.Upper;
    bool has_neg = 0 > rhs.Interval.Lower;

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

//---------------------------------------------------------------------------
// Structured bindings: `auto [iv, notch] = some_grid;`
//---------------------------------------------------------------------------
template <> struct std::tuple_size<bnd::grid> : std::integral_constant<std::size_t, 2> {};
template <> struct std::tuple_element<0, bnd::grid> { using type = bnd::interval; };
template <> struct std::tuple_element<1, bnd::grid> { using type = bnd::rational; };

namespace bnd
{
  template <std::size_t I, class G>
    requires std::same_as<std::remove_cvref_t<G>, bnd::grid>
  constexpr auto&& get(G&& g) noexcept
  {
    if constexpr (I == 0) return std::forward<G>(g).Interval;
    else                  return std::forward<G>(g).Notch;
  }
}

#endif // BNDgridHPP
