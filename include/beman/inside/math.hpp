// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_MATH_HPP
#define BEMAN_INSIDE_MATH_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside.hpp>
#include <beman/inside/math/constants.hpp>
#include <beman/inside/math/integer.hpp>          // factorial, binomial, …, gcd, lcm
#include <beman/inside/math/detail/entry.hpp>     // the math engine
#include <beman/inside/math/detail/functions.hpp> // the call forms

#include <bit>
#include <cstdint>
#include <expected>

//---------------------------------------------------------------------------
// beman::inside::math — the transcendental functions (math/detail/entry.hpp),
// the grid operations (abs, sign, copysign, floor, ceil, round, trunc, fmod,
// pown), the counting functions (factorial, binomial, …, gcd, lcm from
// math/integer.hpp), the constants (pi, e, … from math/constants.hpp) and
// unit_amplitude<K>, the [-1, 1] output grid at resolution 1/K.
//
// Every transcendental returns the correctly rounded point of its output grid,
// under the output's rounding mode, at whatever precision that grid needs —
// so results are the same on every platform, at compile time and at runtime,
// with or without an FPU. Inputs and outputs may be grids past 64 bits.
// `fn(x)` deduces the output from the input (its notch and policy, the
// function's range over the input rounded outward); `fn<per<K>>(x)` and
// `fn<round_floor>(x)` name its notch or policy, `fn<Out>(x)` the output
// itself (math/detail/functions.hpp).
//
// BEMAN_INSIDE_MATH_NO_FP (auto-enabled when freestanding) leaves out the
// engine's double and dd tiers and <cmath>; results do not change. Builds
// with -ffast-math are rejected (inside/detail/config.hpp).
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
    return beman::inside::detail::stored<Out>(v);
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
        return l - (n * grid_round<round_mode::floor>(l / n));
    }
}();

// An exact math output's grid: the lattice when it has a notch, else
// continuous with the denominator bound d (an exact ±x or x − k·y keeps the
// inputs' denominators bounded, grid.hpp den_bound).
consteval grid exact_out_grid(const interval& iv, const grid_rational& notch, const grid_int& d) {
    return notch == 0 ? bounded_continuous(iv, d) : grid{iv, notch};
}

// The smallest |x|: the lattice's lowest non-negative point when x's range
// spans 0, else the limit nearer 0.
template <insidable Mag>
inline constexpr grid_rational abs_result_lower = (lower_of<Mag> <= 0 && upper_of<Mag> >= 0)  ? sym_floor<Mag>
                                                  : (abs(lower_of<Mag>) < abs(upper_of<Mag>)) ? abs(lower_of<Mag>)
                                                                                              : abs(upper_of<Mag>);

// |x| ranges over [smallest |x|, largest |x|].
template <insidable In>
using abs_result_t =
    inside<exact_out_grid({abs_result_lower<In>, abs_result_upper<In>}, sym_notch<In>, den_bound(grid_of<In>)),
           out_policy<In>>;

// sign(x) ∈ {sign(Lower) … sign(Upper)}, integer notch.
template <insidable In>
using sign_result_t = inside<{grid_sign(lower_of<In>), grid_sign(upper_of<In>)}, out_policy<In>>;

// copysign(mag, sgn): |mag| with sgn's possible signs. |mag| ranges over
// [m_lo, m_hi] (abs_result_lower, abs_result_upper); ±|mag| lies on
// sym_notch's lattice (mag's own when it passes through 0).

template <insidable Mag, insidable Sgn>
using copysign_result_t = inside<exact_out_grid({lower_of<Sgn> < 0 ? -abs_result_upper<Mag> : abs_result_lower<Mag>,
                                                 upper_of<Sgn> >= 0 ? abs_result_upper<Mag> : -abs_result_lower<Mag>},
                                                sym_notch<Mag>,
                                                den_bound(grid_of<Mag>)),
                                 out_policy<Mag>>;

template <insidable In, round_mode M>
using integer_result_t = inside<{{grid_round<M>(lower_of<In>), grid_round<M>(upper_of<In>)}, 1}, out_policy<In>>;

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
    return engine::store_exact<Out>(v, make_policy<flags_of<Out>>());
}

template <round_mode M, std::size_t E>
constexpr wide_rational<E> exact_to_int(const wide_rational<E>& v) noexcept {
    return {rounded_div<M>(v.Num, v.Den), wide_sint<E>{1}};
}

// Integer fast path for the algebraic tier: an anchored integer raw holds
// the value J·p/q (J its value index, Notch p/q), so its integer rounding is
// J·p over q rounded. It applies when the auto-deduced Out (which holds every
// result by construction) is the target and |J·p| fits int64.
template <insidable Out, insidable AutoOut, insidable In>
inline constexpr bool int_direct = [] {
    if constexpr (!std::same_as<Out, AutoOut> || !integer_storage<In> || !integer_storage<Out> || !notched<In> ||
                  !anchored<In> || wide_valued<In> || wide_valued<Out> || !value_indices_fit64<In>)
        return false;
    else {
        const std::uint64_t m =
            lower_index64<In>.Num > upper_index64<In>.Num ? lower_index64<In>.Num : upper_index64<In>.Num; // max |J|
        return m <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) / notch64<In>.Num;
    }
}();

// x rounded to an integer by M: exactly, on the value index, or through
// rational. Round is half away from zero, as rational round() is.
template <round_mode M, insidable Out, insidable In>
constexpr Out round_integer(In x) {
    if constexpr (any_wide_valued<Out, In>)
        return store_exact<Out>(exact_to_int<M>(engine::exact_input(x)));
    else if constexpr (int_direct<Out, integer_result_t<In, M>, In>) {
        constexpr std::int64_t p = static_cast<std::int64_t>(notch64<In>.Num);
        constexpr std::int64_t q = static_cast<std::int64_t>(abs_den(notch64<In>.Den));
        const std::int64_t     j = value_index<std::int64_t>(x) * p;
        if constexpr (p == 1 && std::has_single_bit(static_cast<std::uint64_t>(q)) && M != round_mode::half_even) {
            // A notch 2^-k: shifts. floor is the arithmetic shift, ceil its
            // mirror; nearest and trunc shift the magnitude (half away from 0).
            constexpr int k = std::countr_zero(static_cast<std::uint64_t>(q));
            if constexpr (M == round_mode::floor)
                return from_value_index<Out>(j >> k);
            else if constexpr (M == round_mode::ceil)
                return from_value_index<Out>(-((-j) >> k));
            else {
                const std::int64_t s = j >> 63, a = (j ^ s) - s; // sign mask, |j|
                const std::int64_t r = (a + (M == round_mode::nearest ? q / 2 : 0)) >> k;
                return from_value_index<Out>((r ^ s) - s);
            }
        } else
            return from_value_index<Out>(div_rounded(j, q, M));
    } else // (as a rational: the integer may pass int64, a uint64's)
        return store_value<Out>(round_to_integral(rational{x}, M));
}

// Gate for fmod's integer fast path. When both operands and Out are
// integer-backed on commensurable notches, fmod collapses to ONE integer
// remainder in units of g = gcd(notch of InX, notch of InY): with x
// = a·g and y = b·g, x − trunc(x/y)·y = (a − (a/b)·b)·g = (a % b)·g exactly (C++ % is truncated division, the same
// convention). Conditions:
//   * integer raws only (rational raws keep the rational path),
//   * non-zero notches, g on Out's grid (g / notch of Out integer),
//   * Out's interval covers the remainders: ±min(max|x|, max|y|), x's sign,
//   * all unit counts fit comfortably in int64 (headroom 4).
template <insidable Out, insidable InX, insidable InY>
inline constexpr bool fmod_int_fast = [] {
    // (y != 0 is fmod_nonzero's precondition.) Lower/g must be an integer:
    // the lattices pass through 0.
    if constexpr (!notched<InX> || !notched<InY> || !notched<Out> || !anchored<InX> || !anchored<InY> ||
                  !anchored<Out>)
        return false;
    else {
        // In grid numbers, exactly: a count past 64 bits makes the fast path
        // inapplicable, not the build fail.
        constexpr grid_rational g = gcd(notch_of<InX>, notch_of<InY>);
        if (!fits_rational(g) || grid_denominator(g / notch_of<Out>) != grid_int{1})
            return false;
        constexpr auto      mag  = [](const grid_rational& a, const grid_rational& b) { return a < b ? b : a; };
        const grid_rational maxx = mag(abs(lower_of<InX>), abs(upper_of<InX>));
        const grid_rational maxy = mag(abs(lower_of<InY>), abs(upper_of<InY>));
        // Out holds every remainder: |r| ≤ min(max|x|, max|y|), with x's sign.
        const grid_rational bound = maxx < maxy ? maxx : maxy;
        if (lower_of<Out> > (lower_of<InX> < 0 ? -bound : grid_rational{0}) ||
            upper_of<Out> < (upper_of<InX> > 0 ? bound : grid_rational{0}))
            return false;
        // Every unit count, and Out's value index of Lower, within int64/4.
        const grid_rational lim{std::numeric_limits<std::int64_t>::max() / 4};
        return !(lim < maxx / g) && !(lim < maxy / g) && !(lim < maxy / notch_of<Out>) &&
               !(lim < abs(lower_index_of<Out>));
    }
}();

// x mod y = x − trunc(x/y)·y (truncated division, matching std::fmod).
// Result has the sign of x. Pre: y != 0 (fmod_fn checks it).
template <insidable Out, insidable InX, insidable InY>
[[nodiscard]] constexpr Out fmod_nonzero(InX x, InY y) {
    // x − trunc(x/y)·y on exact values.
    auto exact = [&] {
        constexpr std::size_t E =
            2 * (engine::input_limbs<InX> > engine::input_limbs<InY> ? engine::input_limbs<InX>
                                                                     : engine::input_limbs<InY>)+1;
        using F = wide_rational<E>;
        const F    a{engine::exact_input(x)}, b{engine::exact_input(y)};
        const auto q = (a.Num * b.Den) / (a.Den * b.Num); // truncated
        return store_exact<Out>(a + F{-(q * b.Num), b.Den});
    };
    if constexpr (any_wide_valued<Out, InX, InY>)
        return exact();
    else if constexpr (fmod_int_fast<Out, InX, InY>) {
        // One integer remainder in g-units; bit-identical to the rational path.
        constexpr rational     g   = to_rational(gcd(notch_of<InX>, notch_of<InY>));
        constexpr std::int64_t wx  = trunc(notch64<InX> / g);
        constexpr std::int64_t wy  = trunc(notch64<InY> / g);
        constexpr std::int64_t wo  = trunc(g / notch64<Out>);
        constexpr std::int64_t lox = trunc(lower64<InX> / g); // exact: grid invariant
        constexpr std::int64_t loy = trunc(lower64<InY> / g);
        constexpr std::int64_t loo = trunc(lower64<Out> / notch64<Out>);
        const std::int64_t     a   = raw_int64(x) * wx + (index_storage<InX> ? lox : 0);
        const std::int64_t     b   = raw_int64(y) * wy + (index_storage<InY> ? loy : 0);
        const std::int64_t     r   = a % b; // |r| < |b|, in Out's range
        return Out::from_raw(raw_from_offset<Out>(r * wo - loo));
    } else {
        // In 64-bit rationals while every step fits (the quotient within
        // int64); else exactly — the remainder itself always fits Out.
        const rational xv = x, yv = y;
        if (const auto q = try_div(xv, yv); q && *q < rational{std::numeric_limits<std::int64_t>::max()} &&
                                            rational{std::numeric_limits<std::int64_t>::min()} < *q)
            if (const auto qy = try_mul(rational{trunc(*q)}, yv))
                if (const auto r = try_sub(xv, *qy)) [[likely]]
                    return store_value<Out>(*r);
        return exact();
    }
}

// fmod's result: |r| < |y| and |r| ≤ |x|, with the sign of x, on the gcd of
// both notches (x − k·y lies on that lattice, so the result is exact).
template <insidable InX, insidable InY>
inline constexpr grid_rational fmod_bound =
    abs_result_upper<InX> < abs_result_upper<InY> ? abs_result_upper<InX> : abs_result_upper<InY>;

template <insidable InX, insidable InY>
using fmod_result_t = inside<exact_out_grid({(lower_of<InX> < 0 ? -fmod_bound<InX, InY> : grid_rational{0}),
                                             (upper_of<InX> > 0 ? fmod_bound<InX, InY> : grid_rational{0})},
                                            engine::gcd_notch<InX, InY>,
                                            den_bound(grid_of<InX>) * den_bound(grid_of<InY>)),
                             with_default_rounding(out_policy<InX>)>;

//---------------------------------------------------------------------------
// pown<E> — compile-time integer powers, pure grid arithmetic. Repeated
// squaring in inside-space: every multiply widens the result grid
// corner-correctly, so the result is exact for exact inputs and negative
// bases are fine. No engine — works on any inside (like abs/floor/fmod).
// Negative exponents are deferred (they need the division error story).
//---------------------------------------------------------------------------
template <std::int64_t E, insidable In>
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

// pown<E>(x) exists when each squaring and multiply does: a continuous
// product past detail::max_fraction_bits has no type.
template <std::int64_t E, class In>
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

// An even power of a base that reaches below 0 squares |x| instead, so the
// result's range starts at the smallest |x|^E, never below 0.
template <std::int64_t E, insidable In>
inline constexpr bool pown_via_abs = E > 0 && E % 2 == 0 && lower_of<In> < 0;
template <std::int64_t E, insidable In>
using pown_base_t = std::conditional_t<pown_via_abs<E, In>, abs_result_t<In>, In>;

//---------------------------------------------------------------------------
// The tags (math/detail/functions.hpp) of the grid operations: exact, so
// each deduced output is the exact result's grid. abs and copysign take a
// notch symmetric about 0 (sym_notch), fmod the gcd of both inputs' notches;
// floor/ceil/round/trunc deduce notch 1 since their outputs are integers.
//---------------------------------------------------------------------------
template <std::size_t Arity>
struct exact_fn {
    static constexpr bool rounds = false;
    template <class... In>
    static constexpr bool takes = sizeof...(In) == Arity && (insidable<In> && ...);
};

// |x|.
struct abs_fn : exact_fn<1> {
    template <insidable In>
    using result = abs_result_t<In>;
    template <insidable Out, insidable In>
    [[nodiscard]] static constexpr Out eval(In x) {
        if constexpr (any_wide_valued<Out, In>)
            return store_exact<Out>(engine::abs(engine::exact_input(x)));
        else if constexpr (grid_of<Out> == grid_of<result<In>> && value_indices_fit_int64<In> &&
                           notch_of<Out> == notch_of<In>) {
            // The same lattice through 0: |x| is the value index's magnitude.
            // In uint64: |INT64_MIN| = 2^63 is past int64 (Out then reaches it).
            const std::int64_t j = value_index<std::int64_t>(x);
            return from_value_index<Out>(j < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(j)
                                               : static_cast<std::uint64_t>(j));
        } else
            return store_value<Out>(beman::inside::detail::abs(rational{x}));
    }
};

// sign(x) ∈ {−1, 0, 1}, by exact comparison (no decode).
struct sign_fn : exact_fn<1> {
    template <insidable In>
    using result = sign_result_t<In>;
    template <insidable Out, insidable In>
    [[nodiscard]] static constexpr Out eval(In x) {
        return store_value<Out>(std::int64_t{(x > 0) - (x < 0)});
    }
};

// copysign(mag, sgn) — |mag| with the sign of sgn; sgn == 0 counts as positive.
struct copysign_fn : exact_fn<2> {
    template <insidable Mag, insidable Sgn>
    using result = copysign_result_t<Mag, Sgn>;
    template <insidable Out, insidable Mag, insidable Sgn>
    [[nodiscard]] static constexpr Out eval(Mag mag, Sgn sgn) {
        if constexpr (any_wide_valued<Out, Mag>) {
            const auto a = engine::abs(engine::exact_input(mag));
            return store_exact<Out>(sgn < 0 ? -a : a);
        } else if constexpr (grid_of<Out> == grid_of<result<Mag, Sgn>> && value_indices_fit_int64<Mag> &&
                             notch_of<Out> == notch_of<Mag>) {
            // The same lattice through 0: ±|value index|, in uint64 (|INT64_MIN| =
            // 2^63 when sgn is never negative); the raw follows modulo 2^64.
            const std::int64_t  j = value_index<std::int64_t>(mag);
            const std::uint64_t a =
                j < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(j) : static_cast<std::uint64_t>(j);
            return from_value_index<Out>(sgn < 0 ? std::uint64_t{0} - a : a);
        } else {
            const rational a = beman::inside::detail::abs(rational{mag});
            return store_value<Out>(sgn < 0 ? -a : a);
        }
    }
};

// ⌊x⌋, ⌈x⌉, x rounded half away from zero, and x truncated toward zero.
template <round_mode M>
struct integer_fn : exact_fn<1> {
    template <insidable In>
    using result = integer_result_t<In, M>;
    template <insidable Out, insidable In>
    [[nodiscard]] static constexpr Out eval(In x) {
        return round_integer<M, Out>(x);
    }
};
using floor_fn = integer_fn<round_mode::floor>;
using ceil_fn  = integer_fn<round_mode::ceil>;
using round_fn = integer_fn<round_mode::nearest>;
using trunc_fn = integer_fn<round_mode::trunc>;

// Like `/`: a plain Out when y's grid excludes 0, else expected<Out, errc>
// with division_by_zero for y == 0.
struct fmod_fn : exact_fn<2> {
    template <insidable InX, insidable InY>
    using result = fmod_result_t<InX, InY>;
    template <insidable Out, insidable InX, insidable InY>
    [[nodiscard]] static constexpr auto eval(InX x, InY y) {
        if constexpr (beman::inside::detail::divisor_excludes_zero<InY>)
            return fmod_nonzero<Out>(x, y);
        else {
            if (y == 0)
                return std::expected<Out, errc>{std::unexpected(errc::division_by_zero)};
            return std::expected<Out, errc>{fmod_nonzero<Out>(x, y)};
        }
    }
};

template <std::int64_t E>
struct pown_fn {
    static constexpr bool rounds = false;
    template <class... In>
    static constexpr bool takes = [] {
        if constexpr (sizeof...(In) != 1 || !(insidable<In> && ...))
            return false;
        else
            return pown_defined<E, pown_base_t<E, In...[0]>>;
    }();
    template <insidable In>
    static constexpr auto value(In x) noexcept {
        if constexpr (pown_via_abs<E, In>)
            return pown_impl<E>(abs_fn::eval<abs_result_t<In>>(x));
        else
            return pown_impl<E>(x);
    }
    template <insidable In>
    using result = decltype(value(std::declval<In>()));
    template <insidable Out, insidable In>
    [[nodiscard]] static constexpr Out eval(In x) {
        return Out{value(x)};
    }
};
} // namespace detail

//---------------------------------------------------------------------------
// unit_amplitude<K> — amplitude grid [-1, 1] at 1/K resolution, rounding to
// nearest: a ready-made output for sin and cos (`math::sin<math::
// unit_amplitude<32768>>(angle)`; `math::sin<per<32768>>(angle)` deduces the
// same). All angles are radians, as in <cmath>.
//---------------------------------------------------------------------------
template <auto K> // a builtin or a grid number of any size
using unit_amplitude = inside<{{rational{-1}, rational{1}}, per<K>}, round_nearest>;

//---------------------------------------------------------------------------
// The functions (math/detail/functions.hpp states the call forms): the grid
// operations, exact, and the transcendentals of the adaptive engine.
//---------------------------------------------------------------------------
BEMAN_INSIDE_MATH_FUNCTION(abs, detail::abs_fn)
BEMAN_INSIDE_MATH_FUNCTION(sign, detail::sign_fn)
BEMAN_INSIDE_MATH_FUNCTION(copysign, detail::copysign_fn)
BEMAN_INSIDE_MATH_FUNCTION(floor, detail::floor_fn)
BEMAN_INSIDE_MATH_FUNCTION(ceil, detail::ceil_fn)
BEMAN_INSIDE_MATH_FUNCTION(round, detail::round_fn)
BEMAN_INSIDE_MATH_FUNCTION(trunc, detail::trunc_fn)
BEMAN_INSIDE_MATH_FUNCTION(fmod, detail::fmod_fn)
BEMAN_INSIDE_MATH_FUNCTION(pown, detail::pown_fn<E>, std::int64_t E)

BEMAN_INSIDE_MATH_FUNCTION(exp, detail::entry::exp_fn)
BEMAN_INSIDE_MATH_FUNCTION(exp2, detail::entry::exp2_fn)
BEMAN_INSIDE_MATH_FUNCTION(log, detail::entry::log_fn)
BEMAN_INSIDE_MATH_FUNCTION(log2, detail::entry::log2_fn)
BEMAN_INSIDE_MATH_FUNCTION(log10, detail::entry::log10_fn)
BEMAN_INSIDE_MATH_FUNCTION(sqrt, detail::entry::sqrt_fn)
BEMAN_INSIDE_MATH_FUNCTION(cbrt, detail::entry::cbrt_fn)
BEMAN_INSIDE_MATH_FUNCTION(sin, detail::entry::sin_fn)
BEMAN_INSIDE_MATH_FUNCTION(cos, detail::entry::cos_fn)
BEMAN_INSIDE_MATH_FUNCTION(tan, detail::entry::tan_fn)
BEMAN_INSIDE_MATH_FUNCTION(asin, detail::entry::asin_fn)
BEMAN_INSIDE_MATH_FUNCTION(acos, detail::entry::acos_fn)
BEMAN_INSIDE_MATH_FUNCTION(atan, detail::entry::atan_fn)
BEMAN_INSIDE_MATH_FUNCTION(atan2, detail::entry::atan2_fn)
BEMAN_INSIDE_MATH_FUNCTION(sinh, detail::entry::sinh_fn)
BEMAN_INSIDE_MATH_FUNCTION(cosh, detail::entry::cosh_fn)
BEMAN_INSIDE_MATH_FUNCTION(tanh, detail::entry::tanh_fn)
BEMAN_INSIDE_MATH_FUNCTION(asinh, detail::entry::asinh_fn)
BEMAN_INSIDE_MATH_FUNCTION(acosh, detail::entry::acosh_fn)
BEMAN_INSIDE_MATH_FUNCTION(atanh, detail::entry::atanh_fn)
BEMAN_INSIDE_MATH_FUNCTION(hypot, detail::entry::hypot_fn)
BEMAN_INSIDE_MATH_FUNCTION(pow, detail::entry::pow_fn)
BEMAN_INSIDE_MATH_FUNCTION(pow_base, detail::entry::pow_base_fn<Base>, auto Base)
} // namespace beman::inside::math

#endif // BEMAN_INSIDE_MATH_HPP
