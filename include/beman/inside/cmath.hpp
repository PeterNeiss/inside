// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_CMATH_HPP
#define BEMAN_INSIDE_CMATH_HPP

#include <beman/inside/inside.hpp>
#include <beman/inside/cmath_adaptive.hpp>   // the math engine

#include <cstdint>
#include <expected>

//---------------------------------------------------------------------------
// beman::inside::math — the transcendental functions (cmath_adaptive.hpp)
// and the grid operations (abs, sign, copysign, floor, ceil, round, trunc,
// fmod, pown).
//
// Every transcendental returns the correctly rounded point of its output grid,
// under the output's rounding mode, at whatever precision that grid needs —
// so results are the same on every platform, at compile time and at runtime,
// with or without an FPU. Inputs and outputs may be grids past 64 bits.
// `fn_into<Out>(x)` names the output grid; `fn(x)` deduces it from the input
// (its notch and policy, the function's range over the input rounded
// outward).
//
// BEMAN_INSIDE_MATH_NO_FP (auto-enabled when freestanding) leaves out the
// engine's double and dd tiers and <cmath>; results do not change. Builds
// with -ffast-math are rejected (policy_flag.hpp).
//---------------------------------------------------------------------------
namespace beman::inside::math
{
  using beman::inside::detail::rational;

  namespace detail
  {
    using namespace beman::inside::detail;

    // Exact rational source for the irrational constants.
    inline constexpr rational kPiRat{1068966896, 340262731};
    inline constexpr rational kTwoPiRat = 2 * kPiRat;

    // Policy of an auto-deduced output: the input's, minus any fixed-width
    // storage flag (i8 … u64) — the output range differs, as for arithmetic.
    template <insidable In>
    inline constexpr policy_flag out_policy = policy_of<In> & ~raw_width_mask;

    // A 64-bit grid operation's result through Out's assignment.
    template <insidable Out, typename V>
    constexpr Out store_value(V const& v) { return Out{v}; }
  }

  // Public irrational constants as point insides, so they compose directly in
  // inside-space (`angle * math::pi`) with no rational on the surface.
  inline constexpr auto pi     = just<detail::kPiRat>;
  inline constexpr auto two_pi = just<detail::kTwoPiRat>;

  namespace detail
  {
    // Grid-number helpers for the algebraic tier's deduced outputs: exact for
    // grid numbers of any size (C++26), the 64-bit rationals otherwise.
    constexpr grid_rational grid_abs(grid_rational const& r) { return r < 0 ? -r : r; }
    constexpr grid_rational grid_sign(grid_rational const& r) { return grid_rational{(r > 0) - (r < 0)}; }

    // r rounded to an integer by M (half away from zero for nearest).
    template <round_mode M>
    constexpr grid_rational grid_to_int(grid_rational const& r)
    {
      const grid_wide n = wide_numerator(r), d = wide_denominator(r);
      grid_wide q = n / d;
      const grid_wide rem = n - q * d;
      if (!rem.is_zero())
      {
        const bool neg = n.negative();
        const grid_wide arem = neg ? -rem : rem;
        const grid_wide away = neg ? q - grid_wide{1} : q + grid_wide{1};
        if constexpr (M == round_mode::floor)        { if (neg) q = away; }
        else if constexpr (M == round_mode::ceil)    { if (!neg) q = away; }
        else if constexpr (M == round_mode::nearest) { if (!(arem + arem < d)) q = away; }
      }
#if BEMAN_INSIDE_BIG_GRIDS
      return grid_rational{q};
#else
      return rational{static_cast<imax>(q)};
#endif
    }

    // max(|Lower|, |Upper|): sizes the abs and copysign outputs.
    template <insidable In>
    inline constexpr grid_rational abs_auto_upper =
      grid_abs(lower_of<In>) > grid_abs(upper_of<In>) ? grid_abs(lower_of<In>) : grid_abs(upper_of<In>);

    template <insidable In>
    using abs_auto_t = inside<{{grid_rational{0}, abs_auto_upper<In>}, notch_of<In>}, out_policy<In>>;

    // sign(x) ∈ {sign(Lower) … sign(Upper)}, integer notch.
    template <insidable In>
    using sign_auto_t = inside<{grid_sign(lower_of<In>), grid_sign(upper_of<In>)}, out_policy<In>>;

    // copysign(mag, sgn): |mag| with sgn's possible signs. |mag| ranges over
    // [m_lo, m_hi] (m_lo = 0 when mag's interval spans 0); a valid grid's Lower is
    // a multiple of its notch, so ±|mag| stays on mag's lattice.
    template <insidable Mag>
    inline constexpr grid_rational abs_auto_lower =
      (lower_of<Mag> <= 0 && upper_of<Mag> >= 0) ? grid_rational{0}
      : (grid_abs(lower_of<Mag>) < grid_abs(upper_of<Mag>)) ? grid_abs(lower_of<Mag>) : grid_abs(upper_of<Mag>);

    template <insidable Mag, insidable Sgn>
    using copysign_auto_t = inside<{{
        lower_of<Sgn> < 0 ? -abs_auto_upper<Mag> : abs_auto_lower<Mag>,
        upper_of<Sgn> >= 0 ? abs_auto_upper<Mag> : -abs_auto_lower<Mag>},
        notch_of<Mag>}, out_policy<Mag>>;

    template <insidable In, round_mode M>
    using integer_auto_t = inside<{{grid_to_int<M>(lower_of<In>), grid_to_int<M>(upper_of<In>)}, 1}, out_policy<In>>;

    template <insidable In> using floor_auto_t = integer_auto_t<In, round_mode::floor>;
    template <insidable In> using ceil_auto_t  = integer_auto_t<In, round_mode::ceil>;
    template <insidable In> using round_auto_t = integer_auto_t<In, round_mode::nearest>;
    template <insidable In> using trunc_auto_t = integer_auto_t<In, round_mode::trunc>;

    // The exact path: an input or output past the 64-bit rationals (more than
    // 2^64 slots, or grid numbers past 64 bits) computes on exact values.
    template <insidable... Bs>
    inline constexpr bool exact_path = (exact_valued<Bs> || ...);

    template <insidable Out, std::size_t E>
    constexpr Out store_exact(exact_frac<E> const& v)
    { return ax::store_exact<Out>(v, make_policy<policy_of<Out>>()); }

    template <round_mode M, std::size_t E>
    constexpr exact_frac<E> exact_to_int(exact_frac<E> const& v) noexcept
    { return {rounded_div<M>(v.Num, v.Den), wide_sint<E>{1}}; }

    // Double-backed fast path for the algebraic tier. |x| and the integer
    // roundings of a grid value are exact in double (|x| < 2^53 on a
    // double_exact grid, so the imax cast cannot overflow), and the
    // auto-deduced Out holds every result by construction, so the result is
    // stored as the raw without the rational round-trip.
    template <insidable Out, insidable AutoOut, insidable In>
    inline constexpr bool fp_direct =
        std::same_as<Out, AutoOut> && fp_raw<In> && fp_raw<Out>;

    template <insidable Out, insidable In, typename F>
    constexpr Out fp_direct_store(In x, F f) noexcept
    { return Out::from_raw(raw_cast<Out>(f(static_cast<double>(x.raw())))); }

    constexpr double fp_trunc(double v) noexcept { return static_cast<double>(static_cast<imax>(v)); }
    constexpr double fp_floor(double v) noexcept { const double t = fp_trunc(v); return t > v ? t - 1 : t; }
    constexpr double fp_ceil (double v) noexcept { const double t = fp_trunc(v); return t < v ? t + 1 : t; }
    constexpr double fp_round(double v) noexcept   // half away from zero, like rational round()
    {
      const double t = fp_trunc(v), f = v - t;     // exact: v and t share the grid
      return f >= 0.5 ? t + 1 : f <= -0.5 ? t - 1 : t;
    }
  }

  //---------------------------------------------------------------------------
  // Algebraic tier — exact, no polynomial machinery. Each function wraps the
  // corresponding `rational` operation and routes through `Out`'s assignment.
  //---------------------------------------------------------------------------

  // |x|. Output Lower must be ≥ 0 (the result is always non-negative).
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out abs_into(In x)
  {
    static_assert(lower_of<Out> <= 0,
                  "beman::inside::math::abs: Out must include 0");
    if constexpr (detail::exact_path<Out, In>)
      return detail::store_exact<Out>(detail::ax::abs(detail::ax::exact_input(x)));
    else if constexpr (detail::fp_direct<Out, detail::abs_auto_t<In>, In>)
      return detail::fp_direct_store<Out>(x, [](double v) { return v < 0 ? -v : v; });
    else
      return detail::store_value<Out>(beman::inside::detail::abs(rational{x}));
  }

  // sign(x) ∈ {−1, 0, 1}, by exact comparison (no decode).
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out sign_into(In x)
  { return Out{imax{(x > 0) - (x < 0)}}; }

  // copysign(mag, sgn) — |mag| with the sign of sgn; sgn == 0 counts as positive.
  template <insidable Out, insidable Mag, insidable Sgn>
  [[nodiscard]] constexpr Out copysign_into(Mag mag, Sgn sgn)
  {
    if constexpr (detail::exact_path<Out, Mag>)
    {
      const auto a = detail::ax::abs(detail::ax::exact_input(mag));
      return detail::store_exact<Out>(sgn < 0 ? -a : a);
    }
    else
    {
      const rational a = beman::inside::detail::abs(rational{mag});
      return detail::store_value<Out>(sgn < 0 ? -a : a);
    }
  }

  // ⌊x⌋ — largest integer ≤ x.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out floor_into(In x)
  {
    if constexpr (detail::exact_path<Out, In>)
      return detail::store_exact<Out>(detail::exact_to_int<beman::inside::detail::round_mode::floor>(detail::ax::exact_input(x)));
    else if constexpr (detail::fp_direct<Out, detail::floor_auto_t<In>, In>)
      return detail::fp_direct_store<Out>(x, detail::fp_floor);
    else
      return detail::store_value<Out>(floor(rational{x}));
  }

  // ⌈x⌉ — smallest integer ≥ x.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out ceil_into(In x)
  {
    if constexpr (detail::exact_path<Out, In>)
      return detail::store_exact<Out>(detail::exact_to_int<beman::inside::detail::round_mode::ceil>(detail::ax::exact_input(x)));
    else if constexpr (detail::fp_direct<Out, detail::ceil_auto_t<In>, In>)
      return detail::fp_direct_store<Out>(x, detail::fp_ceil);
    else
      return detail::store_value<Out>(ceil(rational{x}));
  }

  // x rounded to nearest integer, half-away-from-zero (matches the existing
  // `rational::round()` convention used throughout the library).
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out round_into(In x)
  {
    if constexpr (detail::exact_path<Out, In>)
      return detail::store_exact<Out>(detail::exact_to_int<beman::inside::detail::round_mode::nearest>(detail::ax::exact_input(x)));
    else if constexpr (detail::fp_direct<Out, detail::round_auto_t<In>, In>)
      return detail::fp_direct_store<Out>(x, detail::fp_round);
    else
      return detail::store_value<Out>(round(rational{x}));
  }

  // x truncated toward zero. Distinct from floor for negative inputs:
  // trunc(-1.7) = -1 vs floor(-1.7) = -2.
  template <insidable Out, insidable In>
  [[nodiscard]] constexpr Out trunc_into(In x)
  {
    if constexpr (detail::exact_path<Out, In>)
      return detail::store_exact<Out>(detail::exact_to_int<beman::inside::detail::round_mode::trunc>(detail::ax::exact_input(x)));
    else if constexpr (detail::fp_direct<Out, detail::trunc_auto_t<In>, In>)
      return detail::fp_direct_store<Out>(x, detail::fp_trunc);
    else
      return detail::store_value<Out>(trunc(rational{x}));
  }

  namespace detail
  {
    using namespace beman::inside::detail;

    // Gate for fmod's integer fast path. When both operands and Out are
    // integer-backed on commensurable notches, fmod collapses to ONE integer
    // remainder in units of g = gcd(::beman::inside::detail::notch64<InX>, ::beman::inside::detail::notch64<InY>): with x = a·g and
    // y = b·g, x − trunc(x/y)·y = (a − (a/b)·b)·g = (a % b)·g exactly (C++ %
    // is truncated division, the same convention). Conditions:
    //   * integer raws only (rational/double raws keep the rational path),
    //   * non-zero notches, g on Out's grid (g / ::beman::inside::detail::notch64<Out> integer),
    //   * divisor grid excludes zero (no runtime zero check needed),
    //   * Out's interval covers ±max|y| (result magnitude is < |y|),
    //   * all unit counts fit comfortably in imax (headroom 4).
    template <insidable Out, insidable InX, insidable InY>
    inline constexpr bool fmod_int_fast = []{
      if (rational_raw<InX> || fp_raw<InX>
       || rational_raw<InY> || fp_raw<InY>
       || rational_raw<Out> || fp_raw<Out>)
        return false;
      if (::beman::inside::detail::notch64<InX> == 0 || ::beman::inside::detail::notch64<InY> == 0 || ::beman::inside::detail::notch64<Out> == 0)
        return false;
      if (!divisor_excludes_zero<InY>)
        return false;
      auto go = gcd(::beman::inside::detail::notch64<InX>, ::beman::inside::detail::notch64<InY>);
      if (!go.has_value()) return false;
      rational g = *go;
      auto qo = g / ::beman::inside::detail::notch64<Out>;
      if (!qo.has_value() || abs_den(qo->Denominator) != 1)
        return false;
      rational maxx =
          abs(::beman::inside::detail::lower64<InX>) > abs(::beman::inside::detail::upper64<InX>)
            ? abs(::beman::inside::detail::lower64<InX>) : abs(::beman::inside::detail::upper64<InX>);
      rational maxy =
          abs(::beman::inside::detail::lower64<InY>) > abs(::beman::inside::detail::upper64<InY>)
            ? abs(::beman::inside::detail::lower64<InY>) : abs(::beman::inside::detail::upper64<InY>);
      if (::beman::inside::detail::lower64<Out> > -maxy || ::beman::inside::detail::upper64<Out> < maxy)
        return false;
      constexpr umax lim = static_cast<umax>(std::numeric_limits<imax>::max() / 4);
      auto ux = maxx / g;  auto uy = maxy / g;  auto uo = maxy / ::beman::inside::detail::notch64<Out>;
      return ux.has_value() && uy.has_value() && uo.has_value()
          && ux->Numerator <= lim && uy->Numerator <= lim && uo->Numerator <= lim;
    }();
  }

  // x mod y = x − ⌊x/y⌋·y (truncated-division convention, matching std::fmod).
  // Result has the sign of x. Pre: y != 0 (fmod_into checks it).
  template <insidable Out, insidable InX, insidable InY>
  [[nodiscard]] constexpr Out fmod_nonzero(InX x, InY y)
  {
    if constexpr (detail::exact_path<Out, InX, InY>)
    {
      // x − trunc(x/y)·y on exact values.
      constexpr std::size_t E = 2 * (detail::ax::input_limbs<InX> > detail::ax::input_limbs<InY>
                                       ? detail::ax::input_limbs<InX> : detail::ax::input_limbs<InY>) + 1;
      using F = beman::inside::detail::exact_frac<E>;
      const F a{detail::ax::exact_input(x)}, b{detail::ax::exact_input(y)};
      const auto q = (a.Num * b.Den) / (a.Den * b.Num);   // truncated
      return detail::store_exact<Out>(a + F{-(q * b.Num), b.Den});
    }
    else if constexpr (detail::fmod_int_fast<Out, InX, InY>)
    {
      // One integer remainder in g-units; bit-identical to the rational path.
      constexpr rational g = *beman::inside::detail::gcd(::beman::inside::detail::notch64<InX>, ::beman::inside::detail::notch64<InY>);
      constexpr imax wx  = trunc((::beman::inside::detail::notch64<InX> / g).value());
      constexpr imax wy  = trunc((::beman::inside::detail::notch64<InY> / g).value());
      constexpr imax wo  = trunc((g / ::beman::inside::detail::notch64<Out>).value());
      constexpr imax lox = trunc((::beman::inside::detail::lower64<InX> / g).value());   // exact: grid invariant
      constexpr imax loy = trunc((::beman::inside::detail::lower64<InY> / g).value());
      constexpr imax loo = trunc((::beman::inside::detail::lower64<Out> / ::beman::inside::detail::notch64<Out>).value());
      const imax a = beman::inside::detail::raw_imax(x) * wx
                   + (beman::inside::detail::index_raw<InX> ? lox : 0);
      const imax b = beman::inside::detail::raw_imax(y) * wy
                   + (beman::inside::detail::index_raw<InY> ? loy : 0);
      const imax r = a % b;                                    // |r| < |b|, in Out's range
      return Out::from_raw(beman::inside::detail::raw_from_offset<Out>(r * wo - loo));
    }
    else
    {
      rational xv = x;
      rational yv = y;
      rational q  = xv / yv;
      imax     qt = trunc(q);
      rational qy = qt * yv;
      rational r  = xv - qy;
      return detail::store_value<Out>(r);
    }
  }

  // Like `/`: a plain Out when y's grid excludes 0, else expected<Out, errc>
  // with division_by_zero for y == 0.
  template <insidable Out, insidable InX, insidable InY>
  [[nodiscard]] constexpr auto fmod_into(InX x, InY y)
  {
    if constexpr (beman::inside::detail::divisor_excludes_zero<InY>)
      return fmod_nonzero<Out>(x, y);
    else
    {
      if (y == 0)
        return std::expected<Out, errc>{std::unexpected(errc::division_by_zero)};
      return std::expected<Out, errc>{fmod_nonzero<Out>(x, y)};
    }
  }

  //---------------------------------------------------------------------------
  // Auto-deducing forms — algebraic tier.
  //
  // Each `fn_into<Out>(x)` has an auto form `fn(x)` that derives `Out` from `In`
  // and delegates to it. Notch policy: abs/fmod inherit `::beman::inside::detail::notch64<In>`; floor/ceil/round/trunc
  // deduce notch 1 since their outputs are integer-valued.
  //---------------------------------------------------------------------------

  //---------------------------------------------------------------------------
  // pown<E> — compile-time integer powers, pure grid arithmetic
  //---------------------------------------------------------------------------
  // Repeated squaring in inside-space: every multiply widens the result grid
  // corner-correctly, so the result is exact for exact inputs and negative
  // bases are fine. No engine, no `f64` requirement — works on any inside
  // (like abs/floor/fmod). Checked rational raws may return
  // std::expected<inside, errc> per the usual arithmetic vocabulary. Negative
  // exponents are deferred (they need the division error story).
  template <imax E, insidable In>
    requires (E >= 0)
  [[nodiscard]] constexpr auto pown(In x) noexcept
  {
    if constexpr (E == 0)      { (void)x; return just<1>; }
    else if constexpr (E == 1) return x;
    else if constexpr (E % 2)  return x * pown<E - 1>(x);
    else                       { auto h = pown<E / 2>(x); return h * h; }
  }

  template <insidable In>
  [[nodiscard]] constexpr auto abs(In x) { return abs_into<detail::abs_auto_t<In>>(x); }

  template <insidable In>
  [[nodiscard]] constexpr auto sign(In x) { return sign_into<detail::sign_auto_t<In>>(x); }

  template <insidable Mag, insidable Sgn>
  [[nodiscard]] constexpr auto copysign(Mag mag, Sgn sgn)
  { return copysign_into<detail::copysign_auto_t<Mag, Sgn>>(mag, sgn); }

  template <insidable In>
  [[nodiscard]] constexpr auto floor(In x) { return floor_into<detail::floor_auto_t<In>>(x); }

  template <insidable In>
  [[nodiscard]] constexpr auto ceil(In x) { return ceil_into<detail::ceil_auto_t<In>>(x); }

  template <insidable In>
  [[nodiscard]] constexpr auto round(In x) { return round_into<detail::round_auto_t<In>>(x); }

  template <insidable In>
  [[nodiscard]] constexpr auto trunc(In x) { return trunc_into<detail::trunc_auto_t<In>>(x); }

  namespace detail
  {
    // fmod's result: |r| < |y| and |r| ≤ |x|, with the sign of x, on the gcd of
    // both notches (x − k·y lies on that lattice, so the result is exact).
    template <insidable InX, insidable InY>
    inline constexpr grid_rational fmod_bound =
        abs_auto_upper<InX> < abs_auto_upper<InY> ? abs_auto_upper<InX> : abs_auto_upper<InY>;

    template <insidable InX, insidable InY>
    using fmod_auto_t = inside<{{(lower_of<InX> < 0 ? -fmod_bound<InX, InY> : grid_rational{0}),
                                 (upper_of<InX> > 0 ?  fmod_bound<InX, InY> : grid_rational{0})},
                                ax::gcd_notch<InX, InY>}, out_policy<InX> | round_nearest>;
  } // namespace detail

  template <insidable InX, insidable InY>
  [[nodiscard]] constexpr auto fmod(InX x, InY y)
  { return fmod_into<detail::fmod_auto_t<InX, InY>>(x, y); }

  //---------------------------------------------------------------------------
  // amp<K> — amplitude grid [-1, 1] at 1/K resolution: a ready-made explicit
  // output for sin / cos (`math::sin_into<math::amp<32768>>(angle)`), decoupling
  // the output precision from the angle's grid. K must be a power of two (f64).
  // All angles are radians, as in <cmath>.
  //---------------------------------------------------------------------------
  template <std::uint64_t K>
  using amp = inside<{{rational{-1}, rational{1}},
                     per<K>}, f64>;


  //---------------------------------------------------------------------------
  // The transcendentals: the adaptive engine.
  //---------------------------------------------------------------------------
  using adaptive::sqrt;
  using adaptive::sqrt_into;
  using adaptive::exp2_into;
  using adaptive::log2_into;
  using adaptive::exp_into;
  using adaptive::log_into;
  using adaptive::pow_base_into;
  using adaptive::sin_into;
  using adaptive::cos_into;
  using adaptive::tan_into;
  using adaptive::atan2_into;
  using adaptive::atan_into;
  using adaptive::asin_into;
  using adaptive::acos_into;
  using adaptive::sinh_into;
  using adaptive::cosh_into;
  using adaptive::tanh_into;
  using adaptive::asinh_into;
  using adaptive::acosh_into;
  using adaptive::atanh_into;
  using adaptive::log10_into;
  using adaptive::cbrt_into;
  using adaptive::hypot_into;
  using adaptive::pow_into;
  using adaptive::exp2;
  using adaptive::log2;
  using adaptive::exp;
  using adaptive::log;
  using adaptive::pow_base;
  using adaptive::sin;
  using adaptive::cos;
  using adaptive::tan;
  using adaptive::atan2;
  using adaptive::atan;
  using adaptive::asin;
  using adaptive::acos;
  using adaptive::sinh;
  using adaptive::cosh;
  using adaptive::tanh;
  using adaptive::asinh;
  using adaptive::acosh;
  using adaptive::atanh;
  using adaptive::log10;
  using adaptive::cbrt;
  using adaptive::hypot;
  using adaptive::pow;
}

#endif // BEMAN_INSIDE_CMATH_HPP
