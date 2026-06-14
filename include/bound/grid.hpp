//---------------------------------------------------------------------------
// Copyright (C) 2026 Peter Neiss
//---------------------------------------------------------------------------
#ifndef BNDgridHPP
#define BNDgridHPP

#include "bound/lift.hpp"
#include "bound/rational.hpp"
#include "bound/interval.hpp"
#include "bound/policy_flag.hpp"

#include "slim/expected.hpp"     // slim::expected, slim::unexpected

#include <algorithm>
#include <concepts>              // std::convertible_to (grid corner ctors)

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
  // grid — structural NTTP type (public members only). Discretizes its interval
  // into notch-sized steps (interval must divide evenly by notch; Notch == 0
  // allows every rational, raw not offset). Its operator+/-/*// is the engine of
  // compile-time result-grid inference: every bound arithmetic operator computes
  // its result grid here, so the result interval contains every reachable value.
  //---------------------------------------------------------------------------
  struct grid
  {
    interval Interval;
    bnd::detail::rational Notch;

    grid() = default;
    // Corner ctors accept any type convertible to `rational` — int/float/rational and
    // any `bound` / `just<>` (via its implicit `operator rational()`), so a bound can be
    // a grid corner. They stay *templates* (deducing the corner type) on purpose: a
    // braced `{lo, hi}` can't deduce to a template parameter, so the `grid{{lo,hi}, notch}`
    // spelling unambiguously picks `grid(interval, rational)` below. The conversion is
    // resolved at the call site, so grid.hpp needs no dependency on `bound`.
    constexpr grid(std::convertible_to<bnd::detail::rational> auto lower,
                   std::convertible_to<bnd::detail::rational> auto upper,
                   std::convertible_to<bnd::detail::rational> auto notch)
      :grid{interval{lower, upper}, notch} { }
    constexpr grid(std::convertible_to<bnd::detail::rational> auto lower,
                   std::convertible_to<bnd::detail::rational> auto upper)
      :grid{interval{lower, upper}, bnd::detail::rational{1}} { }
    constexpr grid(std::convertible_to<bnd::detail::rational> auto lower)
      :grid{interval{lower, lower}, bnd::detail::rational{0}} { }
    constexpr grid(interval val, bnd::detail::rational notch):Interval{val}, Notch{notch} { }

    template <auto G>
    static constexpr bool validate()
    {
      interval::validate<G.Interval>();
      static_assert(G.Interval.divides_evenly(G.Notch));
      static_assert(G.Notch == 0 || detail::abs_den((G.Interval.Lower/G.Notch).value().Denominator) == 1);

      return true;
    }

    // Runtime sibling of validate<G>(): same invariants, but returns a typed
    // error instead of failing a static_assert — for grids built from runtime
    // config. A value, so it can't be a bound<G,P> template argument.
    [[nodiscard]] static constexpr slim::expected<grid, errc>
    try_make(interval iv, bnd::detail::rational notch)
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
        if (detail::abs_den(q->Denominator) != 1)
          return slim::unexpected{errc::rounding_error};
      }
      return grid{iv, notch};
    }

    constexpr umax max_notch() const
    { return (Notch == 0) ? 0 : (Interval/Notch).value().Numerator; }

    // True when `v` is an *exact* slot: in the interval AND on a notch (notch-0
    // grids store verbatim, so any in-range value qualifies). Used to admit a
    // single representable value (e.g. `0_b`) regardless of whole-range mapping.
    constexpr bool representable(bnd::detail::rational v) const
    {
      if (!Interval.includes(v)) return false;
      if (Notch == 0) return true;
      auto diff = v - Interval.Lower;            // optional<rational>
      if (!diff) return false;
      auto off = diff.value() / Notch;           // optional<rational>
      return off.has_value() && detail::abs_den(off->Denominator) == 1;
    }

    // operator== be default for structural type
    constexpr bool operator==(const grid& rhs) const = default;
    constexpr grid operator-() const { return {-Interval, Notch}; }

    // (Raw → double decoding lives in `detail::as_double` (generic.hpp): the
    // decode depends on the storage KIND, not the raw type's signedness — a
    // `direct`-policy bound has an unsigned raw that IS the value.)

    // Snap a double to the nearest grid point `Lower + k·Notch`. `real` storage
    // is only selected for dyadic grids, so the snap is lossless. A continuous
    // grid (Notch == 0) passes `v` through. The round stays constexpr/<cmath>-free.
    constexpr double snap_double(double v) const
    {
      if (Notch == bnd::detail::rational{0}) return v;
      const double lo = static_cast<double>(Interval.Lower);
      const double nd = static_cast<double>(Notch);
      const double q  = (v - lo) / nd;
      const imax   k  = static_cast<imax>(q >= 0 ? q + 0.5 : q - 0.5);
      return lo + static_cast<double>(k) * nd;
    }

    static constexpr grid make_sentinel() noexcept
    { return grid{interval{bnd::detail::rational{0}, bnd::detail::rational{0}}, bnd::detail::rational::make_sentinel()}; }
  };

  // Smallest raw type holding every reachable index in G. Order: notch-zero →
  // rational (no integer index space); signed-direct fits Lower < 0 with notch 1;
  // unsigned-offset (max_notch slots) otherwise.
  namespace detail
  {
  template <grid G>
  using storage_min =
    std::conditional_t<(G.Notch == 0), bnd::detail::rational,
    std::conditional_t<(G.Interval.Lower < 0 && G.Notch == 1),
      smallest_int_for<G.Interval.Lower.trunc(), G.Interval.Upper.trunc()>,
      smallest_uint_for<G.max_notch()>>>;

  // Dyadic grid: power-of-2 notch denominator and Lower denominator, so every
  // on-grid value is exactly representable in IEEE-754 `double`. Precondition
  // for double-backed (`real`) storage.
  constexpr bool is_pow2(umax n) { return n != 0 && (n & (n - 1)) == 0; }

  template <grid G>
  inline constexpr bool dyadic_grid =
       G.Notch.Numerator != 0
    && is_pow2(bnd::detail::abs_den(G.Notch.Denominator))
    && is_pow2(bnd::detail::abs_den(G.Interval.Lower.Denominator));

  // Storage for a bound<G, P>: representation flags pick the raw type, widest-wins
  // (exact > real > direct > indexed > deduced).
  //   exact   → rational raw on any grid.
  //   real    → double-backed under the default engine, on a dyadic or notch-0
  //             grid; elided under BND_MATH_FIXED (falls through to deduced).
  //   direct  → raw == value, plain integer (Notch == 1).
  //   indexed → raw == 0-based notch index (Notch != 0).
  //   none    → storage_min deduction.
  template <grid G, policy_flag P>
  constexpr auto storage_pick()
  {
    if constexpr ((P & bnd::exact) == bnd::exact)
      return bnd::detail::rational{};
#ifndef BND_MATH_FIXED
    else if constexpr (((P & bnd::real) == bnd::real)
                    && (dyadic_grid<G> || G.Notch == 0))
      return double{};
#endif
    else if constexpr ((P & bnd::direct) == bnd::direct && G.Notch == 1)
      return std::conditional_t<(G.Interval.Lower < 0),
          smallest_int_for<G.Interval.Lower.trunc(), G.Interval.Upper.trunc()>,
          smallest_uint_for<static_cast<umax>(G.Interval.Upper.trunc())>>{};
    else if constexpr ((P & bnd::indexed) == bnd::indexed && G.Notch != 0)
      return smallest_uint_for<G.max_notch()>{};
    else
      return storage_min<G>{};
  }

  template <grid G, policy_flag P>
  using storage_for = decltype(storage_pick<G, P>());
  }

  constexpr slim::optional<grid> operator+(const grid&, const grid&);
  constexpr slim::optional<grid> operator-(const grid&, const grid&);
  constexpr slim::optional<grid> operator*(const grid&, const grid&);
  constexpr slim::optional<grid> operator/(const grid&, const grid&);

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<grid> operator+(const grid& lhs, const grid& rhs)
  {
    // gcd returns optional — lift it so a notch-denominator overflow produces
    // nullopt rather than a silently wrapped result grid.
    return lift(
      [](interval i, bnd::detail::rational n){ return grid{i, n}; },
      lhs.Interval + rhs.Interval, detail::gcd(lhs.Notch, rhs.Notch));
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
      [](interval i, bnd::detail::rational n){ return grid{i, n}; },
      lhs.Interval * rhs.Interval, lhs.Notch * rhs.Notch);
  }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<grid> operator/(const grid& lhs, const grid& rhs)
  {
    auto d = lhs.Interval / rhs.Interval;
    if (d.has_value())
      return grid{*d, bnd::detail::rational{0}};

    // Divisor interval includes zero — exclude zero for result interval.
    if (rhs.Interval.Lower == 0 && rhs.Interval.Upper == 0)
      return slim::nullopt;

    // `step` = smallest non-zero divisor magnitude; splits the divisor interval
    // into positive [step, Upper] and negative [Lower, -step] (skipping zero).
    // Both sides present → the result is their union.
    bnd::detail::rational step = (rhs.Notch != 0) ? detail::abs(rhs.Notch) : bnd::detail::rational{1};
    bool has_pos = 0 < rhs.Interval.Upper;
    bool has_neg = 0 > rhs.Interval.Lower;

    if (has_pos && has_neg)
    {
      return lift(
        [](interval pos, interval neg){
          return grid{interval{std::min(neg.Lower, pos.Lower),
                               std::max(neg.Upper, pos.Upper)}, bnd::detail::rational{0}};
        },
        lhs.Interval / interval{step, rhs.Interval.Upper},
        lhs.Interval / interval{rhs.Interval.Lower, -step});
    }
    else if (has_pos)
    {
      return lift([](interval i){ return grid{i, bnd::detail::rational{0}}; },
                  lhs.Interval / interval{step, rhs.Interval.Upper});
    }
    else
    {
      return lift([](interval i){ return grid{i, bnd::detail::rational{0}}; },
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
template <> struct std::tuple_element<1, bnd::grid> { using type = bnd::detail::rational; };

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
