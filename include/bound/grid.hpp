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
  // A grid discretizes its interval into notch-sized steps; the interval must
  // divide evenly by the notch. If Notch is zero, every rational in the
  // interval is allowed and raw is not offset.
  //---------------------------------------------------------------------------
  // `grid` is an NTTP type whose operator+/-/*// is the engine of compile-time
  // result-grid inference for `bound`: every bound arithmetic operator computes
  // its result grid here, so the result's interval is guaranteed by
  // construction to contain every reachable value.
  //---------------------------------------------------------------------------
  struct grid
  {
    interval Interval;
    bnd::detail::rational Notch;

    grid() = default;
    constexpr grid(arithmetic auto lower, arithmetic auto upper, arithmetic auto notch)
      :grid{interval{lower, upper}, bnd::detail::rational{notch}} { }
    constexpr grid(arithmetic auto lower, arithmetic auto upper)
      :grid{interval{lower, upper}, bnd::detail::rational{1}} { }
    constexpr grid(arithmetic auto lower)
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

    // Runtime sibling of `validate<G>()`: mirrors the same invariants but
    // returns a typed error instead of failing a static_assert. Intended for
    // grids constructed from runtime config (parsed input, network, GUI).
    // The result is a value, so it cannot be used as a `bound<G, P>` template
    // argument — callers that need that path select a pre-declared grid
    // instantiation by other means.
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

    // True when `v` is an *exact* slot of this grid: in the interval AND on a
    // notch. Notch-zero grids store the value verbatim (rational storage), so
    // any in-range value qualifies. Mirrors the on-notch test in `validate` /
    // `try_make`; used to admit a single representable value (e.g. `0_b`)
    // regardless of whether the whole-range notch mapping is exact.
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

    constexpr double raw_to_double(std::unsigned_integral auto raw) const
    { return static_cast<double>((*(raw*Notch) + Interval.Lower).value()); }

    constexpr double raw_to_double(std::signed_integral auto raw) const
    { return static_cast<double>(raw); }

    constexpr double raw_to_double(std::same_as<bnd::detail::rational> auto raw) const
    {
      // storage_min only selects rational raw when Notch == 0, so raw is
      // already the value — no offset/scale to apply.
      return static_cast<double>(raw);
    }

    // Double-backed (`real`) storage: the raw IS the value.
    constexpr double raw_to_double(std::same_as<double> auto raw) const
    { return raw; }

    static constexpr grid make_sentinel() noexcept
    { return grid{interval{bnd::detail::rational{0}, bnd::detail::rational{0}}, bnd::detail::rational::make_sentinel()}; }
  };

  // Pick the smallest raw type that can hold every reachable index in G.
  // Order matters: notch-zero grids have no integer index space (rational
  // raw is the only option); signed-direct fits Lower < 0 with notch 1;
  // unsigned-offset (max_notch slots) is the fallback for everything else.
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

  // Storage for a bound<G, P>: math operands (`real` policy) are double-backed
  // under the default (double) engine on a dyadic grid; otherwise (and always
  // under BND_MATH_FIXED) the integer selection above.
  template <grid G, policy_flag P>
  using storage_for =
#ifdef BND_MATH_FIXED
    storage_min<G>;
#else
    std::conditional_t<((P & bnd::real) == bnd::real) && dyadic_grid<G>, double, storage_min<G>>;
#endif
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
    // `gcd(rational, rational)` now returns optional — pass it through lift
    // so a notch-denominator overflow produces nullopt rather than a silently
    // wrapped result grid.
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

    // `step` is the smallest non-zero magnitude the divisor can take. We use
    // it to split the divisor's interval into a positive side [step, Upper]
    // and a negative side [Lower, -step], skipping the zero gap. When both
    // sides are present the result is the *union* of the two sub-divisions.
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
