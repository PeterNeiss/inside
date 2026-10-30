// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_CMATH_HPP
#define BEMAN_INSIDE_CMATH_HPP

#include <beman/inside/detail/config.hpp>

#include <beman/inside/inside.hpp>
#include <beman/inside/detail/math_constants.hpp>
#include <beman/inside/detail/math_entry.hpp> // the math engine

#include <bit>
#include <cstdint>
#include <expected>

//---------------------------------------------------------------------------
// beman::inside::math — the transcendental functions (detail/math_entry.hpp),
// the grid operations (abs, sign, copysign, floor, ceil, round, trunc, fmod,
// pown), the constants (pi, e, … from detail/math_constants.hpp) and
// unit_amplitude<K>, the [-1, 1] output grid at resolution 1/K.
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
namespace beman::inside::math {
namespace detail {
using namespace beman::inside::detail;

// Policy of an auto-deduced output: the input's (a cursor's output is no
// cursor).
template <insidable In>
inline constexpr policy_flag out_policy = flags_of<In> & ~cursor_marker;

// A 64-bit grid operation's result through Out's assignment.
template <insidable Out, typename V>
constexpr Out store_value(const V& v) {
    return Out{v};
}

// The sign of a grid number, for the deduced sign() output.
constexpr grid_rational grid_sign(const grid_rational& r) { return grid_rational{(r > 0) - (r < 0)}; }

// max(|Lower|, |Upper|): sizes the abs and copysign outputs.
template <insidable In>
inline constexpr grid_rational abs_result_upper = abs(lower_of<In>) > abs(upper_of<In>) ? abs(lower_of<In>)
                                                                                        : abs(upper_of<In>);

// The lattice of ±x: x's notch, refined by 2·Lower when x's lattice does
// not pass through 0 (±(0.25 + k/2) lie on 0.25 + ℤ/2; ±(0.5 + k) on ℤ/2).
template <insidable In>
inline constexpr grid_rational sym_notch =
    anchored<In> ? notch_of<In> : gcd(notch_of<In>, lower_of<In> + lower_of<In>);

// The lowest non-negative point of that lattice: 0 when it passes through 0.
template <insidable In>
inline constexpr grid_rational sym_floor = [] {
    if constexpr (anchored<In>)
        return grid_rational{0};
    else {
        const grid_rational n = sym_notch<In>, l = lower_of<In>;
        // l − n·⌊l/n⌋, with ⌊l/n⌋ from the rounding helper below
        return l - (n * grid_to_int<round_mode::floor>(l / n));
    }
}();

// An exact math output's grid: the lattice when it has a notch, else
// continuous with the denominator bound d (an exact ±x or x − k·y keeps the
// inputs' denominators bounded, grid.hpp den_bound).
consteval grid exact_out_grid(const interval& iv, const grid_rational& notch, const grid_int& d) {
    return notch == 0 ? bounded_continuous(iv, d) : grid{iv, notch};
}

template <insidable In>
using abs_result_t =
    inside<exact_out_grid({sym_floor<In>, abs_result_upper<In>}, sym_notch<In>, den_bound(grid_of<In>)),
           out_policy<In>>;

// sign(x) ∈ {sign(Lower) … sign(Upper)}, integer notch.
template <insidable In>
using sign_result_t = inside<{grid_sign(lower_of<In>), grid_sign(upper_of<In>)}, out_policy<In>>;

// copysign(mag, sgn): |mag| with sgn's possible signs. |mag| ranges over
// [m_lo, m_hi] (m_lo = 0 when mag's interval spans 0); ±|mag| lies on
// sym_notch's lattice (mag's own when it passes through 0).
template <insidable Mag>
inline constexpr grid_rational abs_result_lower = (lower_of<Mag> <= 0 && upper_of<Mag> >= 0)  ? sym_floor<Mag>
                                                  : (abs(lower_of<Mag>) < abs(upper_of<Mag>)) ? abs(lower_of<Mag>)
                                                                                              : abs(upper_of<Mag>);

template <insidable Mag, insidable Sgn>
using copysign_result_t = inside<exact_out_grid({lower_of<Sgn> < 0 ? -abs_result_upper<Mag> : abs_result_lower<Mag>,
                                                 upper_of<Sgn> >= 0 ? abs_result_upper<Mag> : -abs_result_lower<Mag>},
                                                sym_notch<Mag>,
                                                den_bound(grid_of<Mag>)),
                                 out_policy<Mag>>;

template <insidable In, round_mode M>
using integer_result_t = inside<{{grid_to_int<M>(lower_of<In>), grid_to_int<M>(upper_of<In>)}, 1}, out_policy<In>>;

template <insidable In>
using floor_result_t = integer_result_t<In, round_mode::floor>;
template <insidable In>
using ceil_result_t = integer_result_t<In, round_mode::ceil>;
template <insidable In>
using round_result_t = integer_result_t<In, round_mode::nearest>;
template <insidable In>
using trunc_result_t = integer_result_t<In, round_mode::trunc>;

// The exact path: an input or output past the 64-bit rationals (more than
// 2^64 slots, or grid numbers past 64 bits) computes on exact values.
template <insidable... Bs>
inline constexpr bool any_wide_valued = (wide_valued<Bs> || ...);

template <insidable Out, std::size_t E>
constexpr Out store_exact(const wide_rational<E>& v) {
    return ax::store_exact<Out>(v, make_policy<flags_of<Out>>());
}

template <round_mode M, std::size_t E>
constexpr wide_rational<E> exact_to_int(const wide_rational<E>& v) noexcept {
    return {rounded_div<M>(v.Num, v.Den), wide_sint<E>{1}};
}

// Integer fast path for the algebraic tier: an anchored integer raw holds
// the value J·p/q (J its value index, Notch p/q), so its integer rounding is
// J·p over q rounded. It applies when the auto-deduced Out (which holds every
// result by construction) is the target and |J·p| fits imax.
template <insidable Out, insidable AutoOut, insidable In>
inline constexpr bool int_direct = [] {
    if constexpr (!std::same_as<Out, AutoOut> || !integer_storage<In> || !integer_storage<Out> || !notched<In> ||
                  !anchored<In> || wide_valued<In> || wide_valued<Out>)
        return false;
    else {
        const umax m =
            index_lo64<In>->Num > index_hi64<In>->Num ? index_lo64<In>->Num : index_hi64<In>->Num; // max |J|
        return m <= static_cast<umax>(std::numeric_limits<imax>::max()) / notch64<In>.Num;
    }
}();

// x rounded to an integer by M: exactly, on the value index, or through
// rational. Round is half away from zero, as rational round() is.
template <round_mode M, insidable Out, insidable In>
constexpr Out integer_into(In x) {
    if constexpr (any_wide_valued<Out, In>)
        return store_exact<Out>(exact_to_int<M>(ax::exact_input(x)));
    else if constexpr (int_direct<Out, integer_result_t<In, M>, In>) {
        constexpr imax p = static_cast<imax>(notch64<In>.Num);
        constexpr imax q = static_cast<imax>(abs_den(notch64<In>.Den));
        const imax     j = value_index<imax>(x) * p;
        if constexpr (p == 1 && std::has_single_bit(static_cast<umax>(q)) && M != round_mode::half_even) {
            // A notch 2^-k: shifts. floor is the arithmetic shift, ceil its
            // mirror; nearest and trunc shift the magnitude (half away from 0).
            constexpr int k = std::countr_zero(static_cast<umax>(q));
            if constexpr (M == round_mode::floor)
                return from_value_index<Out>(j >> k);
            else if constexpr (M == round_mode::ceil)
                return from_value_index<Out>(-((-j) >> k));
            else {
                const imax s = j >> 63, a = (j ^ s) - s; // sign mask, |j|
                const imax r = (a + (M == round_mode::nearest ? q / 2 : 0)) >> k;
                return from_value_index<Out>((r ^ s) - s);
            }
        } else
            return from_value_index<Out>(div_rounded(j, q, M));
    } else // (as a rational: the integer may pass imax, a uint64's)
        return store_value<Out>(round_to_integral(rational{x}, M));
}
} // namespace detail

//---------------------------------------------------------------------------
// Algebraic tier — exact, no polynomial machinery. Each function wraps the
// corresponding `rational` operation and routes through `Out`'s assignment.
//---------------------------------------------------------------------------

namespace detail {
template <class Out, class In>
consteval bool abs_into_ok() {
    return lower_of<Out> <= abs_result_lower<In>;
}
} // namespace detail

// |x|. Out must include the smallest |x|, 0 when x's range spans 0 — a
// constraint, so a probe sees it.
template <insidable Out, insidable In>
    requires(detail::abs_into_ok<Out, In>())
[[nodiscard]] constexpr Out abs_into(In x) {
    if constexpr (detail::any_wide_valued<Out, In>)
        return detail::store_exact<Out>(detail::ax::abs(detail::ax::exact_input(x)));
    else if constexpr (std::same_as<Out, detail::abs_result_t<In>> && detail::notched<In> && detail::anchored<In> &&
                       notch_of<Out> == notch_of<In>) {
        // The same lattice through 0: |x| is the value index's magnitude.
        // In umax: |INT64_MIN| = 2^63 is past imax (Out then reaches it).
        const imax j = detail::value_index<imax>(x);
        return detail::from_value_index<Out>(j < 0 ? umax{0} - static_cast<umax>(j) : static_cast<umax>(j));
    } else
        return detail::store_value<Out>(beman::inside::detail::abs(rational{x}));
}

// sign(x) ∈ {−1, 0, 1}, by exact comparison (no decode).
template <insidable Out, insidable In>
[[nodiscard]] constexpr Out sign_into(In x) {
    return Out{imax{(x > 0) - (x < 0)}};
}

// copysign(mag, sgn) — |mag| with the sign of sgn; sgn == 0 counts as positive.
template <insidable Out, insidable Mag, insidable Sgn>
[[nodiscard]] constexpr Out copysign_into(Mag mag, Sgn sgn) {
    if constexpr (detail::any_wide_valued<Out, Mag>) {
        const auto a = detail::ax::abs(detail::ax::exact_input(mag));
        return detail::store_exact<Out>(sgn < 0 ? -a : a);
    } else if constexpr (std::same_as<Out, detail::copysign_result_t<Mag, Sgn>> && detail::notched<Mag> &&
                         detail::anchored<Mag> && notch_of<Out> == notch_of<Mag>) {
        // The same lattice through 0: ±|value index|. j is never INT64_MIN:
        // then Out, symmetric, would hold 2^64 slots and be wide (above).
        const imax j = detail::value_index<imax>(mag), a = j < 0 ? -j : j;
        return detail::from_value_index<Out>(sgn < 0 ? -a : a);
    } else {
        const rational a = beman::inside::detail::abs(rational{mag});
        return detail::store_value<Out>(sgn < 0 ? -a : a);
    }
}

// ⌊x⌋, ⌈x⌉, x rounded half away from zero, and x truncated toward zero.
template <insidable Out, insidable In>
[[nodiscard]] constexpr Out floor_into(In x) {
    return detail::integer_into<detail::round_mode::floor, Out>(x);
}
template <insidable Out, insidable In>
[[nodiscard]] constexpr Out ceil_into(In x) {
    return detail::integer_into<detail::round_mode::ceil, Out>(x);
}
template <insidable Out, insidable In>
[[nodiscard]] constexpr Out round_into(In x) {
    return detail::integer_into<detail::round_mode::nearest, Out>(x);
}
template <insidable Out, insidable In>
[[nodiscard]] constexpr Out trunc_into(In x) {
    return detail::integer_into<detail::round_mode::trunc, Out>(x);
}

namespace detail {
// Gate for fmod's integer fast path. When both operands and Out are
// integer-backed on commensurable notches, fmod collapses to ONE integer
// remainder in units of g = gcd(notch of InX, notch of InY): with x
// = a·g and y = b·g, x − trunc(x/y)·y = (a − (a/b)·b)·g = (a % b)·g exactly (C++ % is truncated division, the same
// convention). Conditions:
//   * integer raws only (rational raws keep the rational path),
//   * non-zero notches, g on Out's grid (g / notch of Out integer),
//   * Out's interval covers the remainders: ±min(max|x|, max|y|), x's sign,
//   * all unit counts fit comfortably in imax (headroom 4).
template <insidable Out, insidable InX, insidable InY>
inline constexpr bool fmod_int_fast = [] {
    if (!notched<InX> || !notched<InY> || !notched<Out>)
        return false;
    // (y != 0 is fmod_nonzero's precondition.) Lower/g must be an integer:
    // the lattices pass through 0.
    if (!anchored<InX> || !anchored<InY> || !anchored<Out>)
        return false;
    auto go = gcd(notch64<InX>, notch64<InY>);
    if (!go.has_value())
        return false;
    rational g = *go;
    // (try_ ops: a quotient past 64 bits makes the fast path inapplicable,
    // not the build fail.)
    auto qo = try_div(g, notch64<Out>);
    if (!qo.has_value() || abs_den(qo->Den) != 1)
        return false;
    rational maxx = abs(lower64<InX>) > abs(upper64<InX>) ? abs(lower64<InX>) : abs(upper64<InX>);
    rational maxy = abs(lower64<InY>) > abs(upper64<InY>) ? abs(lower64<InY>) : abs(upper64<InY>);
    // Out holds every remainder: |r| ≤ min(max|x|, max|y|), with x's sign.
    const rational bound = maxx < maxy ? maxx : maxy;
    if (lower64<Out> > (lower64<InX> < 0 ? -bound : rational{0}) ||
        upper64<Out> < (upper64<InX> > 0 ? bound : rational{0}))
        return false;
    constexpr umax lim = static_cast<umax>(std::numeric_limits<imax>::max() / 4);
    auto           ux  = try_div(maxx, g);
    auto           uy  = try_div(maxy, g);
    auto           uo  = try_div(maxy, notch64<Out>);
    return ux.has_value() && uy.has_value() && uo.has_value() && ux->Num <= lim && uy->Num <= lim && uo->Num <= lim;
}();
} // namespace detail

// x mod y = x − trunc(x/y)·y (truncated division, matching std::fmod).
// Result has the sign of x. Pre: y != 0 (fmod_into checks it).
template <insidable Out, insidable InX, insidable InY>
[[nodiscard]] constexpr Out fmod_nonzero(InX x, InY y) {
    // x − trunc(x/y)·y on exact values.
    auto exact = [&] {
        constexpr std::size_t E =
            2 * (detail::ax::input_limbs<InX> > detail::ax::input_limbs<InY> ? detail::ax::input_limbs<InX>
                                                                             : detail::ax::input_limbs<InY>)+1;
        using F = detail::wide_rational<E>;
        const F    a{detail::ax::exact_input(x)}, b{detail::ax::exact_input(y)};
        const auto q = (a.Num * b.Den) / (a.Den * b.Num); // truncated
        return detail::store_exact<Out>(a + F{-(q * b.Num), b.Den});
    };
    if constexpr (detail::any_wide_valued<Out, InX, InY>)
        return exact();
    else if constexpr (detail::fmod_int_fast<Out, InX, InY>) {
        // One integer remainder in g-units; bit-identical to the rational path.
        constexpr rational g   = *detail::gcd(detail::notch64<InX>, detail::notch64<InY>);
        constexpr imax     wx  = trunc((detail::notch64<InX> / g).value());
        constexpr imax     wy  = trunc((detail::notch64<InY> / g).value());
        constexpr imax     wo  = trunc((g / detail::notch64<Out>).value());
        constexpr imax     lox = trunc((detail::lower64<InX> / g).value()); // exact: grid invariant
        constexpr imax     loy = trunc((detail::lower64<InY> / g).value());
        constexpr imax     loo = trunc((detail::lower64<Out> / detail::notch64<Out>).value());
        const imax         a   = detail::raw_imax(x) * wx + (detail::index_storage<InX> ? lox : 0);
        const imax         b   = detail::raw_imax(y) * wy + (detail::index_storage<InY> ? loy : 0);
        const imax         r   = a % b; // |r| < |b|, in Out's range
        return Out::from_raw(detail::raw_from_offset<Out>(r * wo - loo));
    } else {
        // In 64-bit rationals while every step fits (the quotient within
        // imax); else exactly — the remainder itself always fits Out.
        const rational xv = x, yv = y;
        if (const auto q = try_div(xv, yv);
            q && *q < rational{std::numeric_limits<imax>::max()} && rational{std::numeric_limits<imax>::min()} < *q)
            if (const auto qy = try_mul(rational{trunc(*q)}, yv))
                if (const auto r = try_sub(xv, *qy)) [[likely]]
                    return detail::store_value<Out>(*r);
        return exact();
    }
}

// Like `/`: a plain Out when y's grid excludes 0, else expected<Out, errc>
// with division_by_zero for y == 0.
template <insidable Out, insidable InX, insidable InY>
[[nodiscard]] constexpr auto fmod_into(InX x, InY y) {
    if constexpr (beman::inside::detail::divisor_excludes_zero<InY>)
        return fmod_nonzero<Out>(x, y);
    else {
        if (y == 0)
            return std::expected<Out, errc>{std::unexpected(errc::division_by_zero)};
        return std::expected<Out, errc>{fmod_nonzero<Out>(x, y)};
    }
}

//---------------------------------------------------------------------------
// pown<E> — compile-time integer powers, pure grid arithmetic
//---------------------------------------------------------------------------
// Repeated squaring in inside-space: every multiply widens the result grid
// corner-correctly, so the result is exact for exact inputs and negative
// bases are fine. No engine — works on any inside
// (like abs/floor/fmod). Checked rational raws may return
// std::expected<inside, errc> per the usual arithmetic vocabulary. Negative
// exponents are deferred (they need the division error story).
namespace detail {
template <imax E, insidable In>
constexpr auto pown_impl(In x) noexcept {
    if constexpr (E == 0) {
        (void)x;
        return just<1>;
    } else if constexpr (E == 1)
        return x;
    else if constexpr (E % 2)
        return x * pown_impl<E - 1>(x);
    else {
        auto h = pown_impl<E / 2>(x);
        return h * h;
    }
}

// pown<E>(x) exists when each squaring and multiply does: a checked
// continuous product (an expected) does not multiply further.
template <imax E, class In>
inline constexpr bool pown_defined = [] {
    if constexpr (E < 0)
        return false;
    else if constexpr (E <= 1)
        return true;
    else if constexpr (E % 2 == 1) {
        if constexpr (!pown_defined<E - 1, In>)
            return false;
        else
            return requires(In x) { x * pown_impl<E - 1>(x); };
    } else {
        if constexpr (!pown_defined<E / 2, In>)
            return false;
        else
            return requires(In x) { pown_impl<E / 2>(x) * pown_impl<E / 2>(x); };
    }
}();
} // namespace detail

template <imax E, insidable In>
    requires detail::pown_defined<E, In>
[[nodiscard]] constexpr auto pown(In x) noexcept {
    return detail::pown_impl<E>(x);
}

//---------------------------------------------------------------------------
// Auto-deducing forms — algebraic tier.
//
// Each `fn_into<Out>(x)` has an auto form `fn(x)` that derives `Out` from `In`
// and delegates to it. abs and copysign take a notch symmetric about 0
// (sym_notch), fmod the gcd of both inputs' notches; floor/ceil/round/trunc
// deduce notch 1 since their outputs are integer-valued.
//---------------------------------------------------------------------------
template <insidable In>
[[nodiscard]] constexpr auto abs(In x) {
    return abs_into<detail::abs_result_t<In>>(x);
}

template <insidable In>
[[nodiscard]] constexpr auto sign(In x) {
    return sign_into<detail::sign_result_t<In>>(x);
}

template <insidable Mag, insidable Sgn>
[[nodiscard]] constexpr auto copysign(Mag mag, Sgn sgn) {
    return copysign_into<detail::copysign_result_t<Mag, Sgn>>(mag, sgn);
}

template <insidable In>
[[nodiscard]] constexpr auto floor(In x) {
    return floor_into<detail::floor_result_t<In>>(x);
}

template <insidable In>
[[nodiscard]] constexpr auto ceil(In x) {
    return ceil_into<detail::ceil_result_t<In>>(x);
}

template <insidable In>
[[nodiscard]] constexpr auto round(In x) {
    return round_into<detail::round_result_t<In>>(x);
}

template <insidable In>
[[nodiscard]] constexpr auto trunc(In x) {
    return trunc_into<detail::trunc_result_t<In>>(x);
}

namespace detail {
// fmod's result: |r| < |y| and |r| ≤ |x|, with the sign of x, on the gcd of
// both notches (x − k·y lies on that lattice, so the result is exact).
template <insidable InX, insidable InY>
inline constexpr grid_rational fmod_bound =
    abs_result_upper<InX> < abs_result_upper<InY> ? abs_result_upper<InX> : abs_result_upper<InY>;

template <insidable InX, insidable InY>
using fmod_result_t = inside<exact_out_grid({(lower_of<InX> < 0 ? -fmod_bound<InX, InY> : grid_rational{0}),
                                             (upper_of<InX> > 0 ? fmod_bound<InX, InY> : grid_rational{0})},
                                            ax::gcd_notch<InX, InY>,
                                            den_bound(grid_of<InX>) * den_bound(grid_of<InY>)),
                             out_policy<InX> | round_nearest>;
} // namespace detail

template <insidable InX, insidable InY>
[[nodiscard]] constexpr auto fmod(InX x, InY y) {
    return fmod_into<detail::fmod_result_t<InX, InY>>(x, y);
}

//---------------------------------------------------------------------------
// unit_amplitude<K> — amplitude grid [-1, 1] at 1/K resolution: a ready-made explicit
// output for sin / cos (`math::sin_into<math::unit_amplitude<32768>>(angle)`), decoupling
// the output precision from the angle's grid; results round to nearest. All
// angles are radians, as in <cmath>.
//---------------------------------------------------------------------------
template <std::uint64_t K>
using unit_amplitude = inside<{{rational{-1}, rational{1}}, per<K>}, round_nearest>;

//---------------------------------------------------------------------------
// The transcendentals: the adaptive engine.
//---------------------------------------------------------------------------
using adaptive::acos;
using adaptive::acos_into;
using adaptive::acosh;
using adaptive::acosh_into;
using adaptive::asin;
using adaptive::asin_into;
using adaptive::asinh;
using adaptive::asinh_into;
using adaptive::atan;
using adaptive::atan2;
using adaptive::atan2_into;
using adaptive::atan_into;
using adaptive::atanh;
using adaptive::atanh_into;
using adaptive::cbrt;
using adaptive::cbrt_into;
using adaptive::cos;
using adaptive::cos_into;
using adaptive::cosh;
using adaptive::cosh_into;
using adaptive::exp;
using adaptive::exp2;
using adaptive::exp2_into;
using adaptive::exp_into;
using adaptive::hypot;
using adaptive::hypot_into;
using adaptive::log;
using adaptive::log10;
using adaptive::log10_into;
using adaptive::log2;
using adaptive::log2_into;
using adaptive::log_into;
using adaptive::pow;
using adaptive::pow_base;
using adaptive::pow_base_into;
using adaptive::pow_into;
using adaptive::sin;
using adaptive::sin_into;
using adaptive::sinh;
using adaptive::sinh_into;
using adaptive::sqrt;
using adaptive::sqrt_into;
using adaptive::tan;
using adaptive::tan_into;
using adaptive::tanh;
using adaptive::tanh_into;
} // namespace beman::inside::math

#endif // BEMAN_INSIDE_CMATH_HPP
