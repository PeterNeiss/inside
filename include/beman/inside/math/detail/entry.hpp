// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_MATH_DETAIL_ENTRY_HPP
#define BEMAN_INSIDE_MATH_DETAIL_ENTRY_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/math/detail/tiers.hpp> // the double and dd tiers, over math_cores.hpp

#include <algorithm>
#include <cstddef>
#include <expected>
#include <initializer_list>
#include <optional>
#include <utility>

//---------------------------------------------------------------------------
// beman::inside::math::detail::entry — the math engine behind math.hpp (which
// states the guarantee). The decision step and the driver are in
// math/detail/adaptive.hpp. Domains are the mathematical ones; a result past
// Out's range goes through Out's policy, as any assignment.
//
// Each call takes the first tier that applies: a compile-time table for
// small inputs; the double kernels (math/detail/fp.hpp), then the dd kernels
// (math/detail/dd.hpp), each returning a result only when its error bound
// decides the slot (their glue: math/detail/tiers.hpp); and the integer path
// (math/detail/cores.hpp: exact square and cube roots, series in wide fixed
// point), which decides every result and is the only tier at compile time and
// without an FPU.
//---------------------------------------------------------------------------
namespace beman::inside::math::detail::entry {
namespace engine = ::beman::inside::math::detail::engine;
using ::beman::inside::detail::grid_rational;
using ::beman::inside::detail::wide_valued;

//---------------------------------------------------------------------------
// Unanchored grids. The engine counts results in Out's notch from 0 and
// inputs by their value index, so a lattice that does not pass through 0
// ({{0.5, 10.5}, 1}) is computed through anchored grids:
//   * an input is viewed exactly on its value unit gcd(Notch, Lower);
//   * an output is computed rounded down and up on the grid of every Out
//     point and every half point between two (unit gcd(Notch/2, Lower)),
//     one notch wider on each side. Equal, the value is that point; else it
//     lies strictly between two neighbours there, where no rounding
//     boundary of Out falls, so their midpoint rounds as the value does.
// wrap would fold the wider grid differently, so it needs an anchored Out.
//---------------------------------------------------------------------------
namespace anchoring {
using ::beman::inside::detail::anchored;

template <insidable In>
constexpr auto anchored_input(const In& x) {
    if constexpr (anchored<In>)
        return x;
    else
        return inside<grid{grid_of<In>.Interval, grid_of<In>.value_unit()},
                      flags_of<In> & ~::beman::inside::detail::cursor_marker>{x};
}

template <insidable Out>
inline constexpr grid_rational half_unit =
    ::beman::inside::detail::gcd(notch_of<Out> / grid_rational{2}, lower_of<Out>);

template <insidable Out, policy_flag R, grid_rational Unit = half_unit<Out>>
using bracket_t =
    inside<grid{{lower_of<Out> - notch_of<Out>, upper_of<Out> + notch_of<Out>}, Unit}, R | (flags_of<Out> & clamp)>;

// The integer form: a bracket 1024 times finer than half_unit, counted in
// int64. Rounded down there, the value lies in [y, y + F); only when y is
// itself a rounding boundary of Out (a point or a half point — at most one
// bracket point in 1024) does rounding up decide whether the value is exactly y.
// Then Out's slot follows from the bracket index by one integer division.
template <insidable Out>
inline constexpr grid_rational fine_unit = half_unit<Out> / grid_rational{1024};

template <insidable Out>
inline constexpr bool integer_bracket = [] {
    using ::beman::inside::detail::signed_value_bits, ::beman::inside::detail::units_lo,
        ::beman::inside::detail::units_hi;
    if constexpr (wide_valued<Out>)
        return false;
    else {
        using B = bracket_t<Out, round_floor, fine_unit<Out>>;
        return !wide_valued<B> && signed_value_bits({units_lo<B, fine_unit<Out>>, units_hi<B, fine_unit<Out>>}) < 60;
    }
}();

// Out's slot for a value known as `num/den` bracket units past Lower (in
// value space: `negative` is the value's sign), or nothing past Out's range.
template <insidable Out, std::int64_t den>
constexpr std::optional<std::int64_t> bracket_slot(std::int64_t num, bool negative) noexcept {
    using namespace ::beman::inside::detail;
    constexpr round_mode m = rounding_for<Out, call_policy<none>>;
    const auto [q, r]      = floor_divmod(num, den);
    const std::int64_t k =
        q + rounds_up(m,
                      negative,
                      classify_remainder(m, static_cast<std::uint64_t>(r), static_cast<std::uint64_t>(den)),
                      ((q & 1) != 0) != lower_index_odd<Out>);
    if (k < 0 || std::cmp_greater(k, max_index_of<Out>))
        return std::nullopt;
    return k;
}

// fn.operator()<O>(xs...) for an anchored O: Out's result, or its error.
template <insidable Out, typename Fn, insidable... Ins>
constexpr auto via_anchored(const Fn& fn, const Ins&... xs) {
    static_assert(anchored<Out> || !has_flag(flags_of<Out>, wrap),
                  "beman::inside::math: wrap onto a grid that does not pass through 0 is not supported - "
                  "use clamp, or an anchored output grid");
    using ::beman::inside::detail::expected_like;
    if constexpr (anchored<Out>)
        return fn.template operator()<Out>(anchored_input(xs)...);
    else if constexpr (integer_bracket<Out>) {
        using namespace ::beman::inside::detail;
        constexpr grid_rational F = fine_unit<Out>;
        constexpr std::int64_t  c = static_cast<std::int64_t>(exact_quotient(lower_of<Out>, F)); // Lower in F
        constexpr std::int64_t  s = static_cast<std::int64_t>(exact_quotient(notch_of<Out>, F)); // Notch in F (even)
        using Lo                  = bracket_t<Out, round_floor, F>;
        using Hi                  = bracket_t<Out, round_ceil, F>;
        using R                   = decltype(fn.template operator()<Lo>(anchored_input(xs)...));
        constexpr bool checked    = expected_like<R>;
        using Res                 = std::conditional_t<checked, std::expected<Out, errc>, Out>;
        const R lo                = fn.template operator()<Lo>(anchored_input(xs)...);
        const auto               unwrap = [](const auto& r) -> const auto& {
            if constexpr (checked)
                return *r;
            else
                return r;
        };
        if constexpr (checked)
            if (!lo)
                return Res{std::unexpected{lo.error()}};
        const std::int64_t j = value_in_units<std::int64_t, F>(unwrap(lo)); // y = j·F
        const std::int64_t t = j - c;                                       // y − Lower, in F
        // The value lies inside (y, y + F): 2t + 1 half units of F.
        std::optional<std::int64_t> k;
        if (t % (s / 2) != 0) [[likely]]
            k = bracket_slot<Out, 2 * s>(2 * t + 1, j < 0);
        else { // y is a boundary of Out: is the value exactly y?
            const auto hi = fn.template operator()<Hi>(anchored_input(xs)...);
            if constexpr (checked)
                if (!hi)
                    return Res{std::unexpected{hi.error()}};
            k = value_in_units<std::int64_t, F>(unwrap(hi)) == j ? bracket_slot<Out, s>(t, j < 0)
                                                                 : bracket_slot<Out, 2 * s>(2 * t + 1, j < 0);
        }
        if (k) [[likely]]
            return Res{Out::from_raw(raw_of_slot<Out>(*k))};
        // Past Out's range: the bracket value through Out's policy (it rounds
        // as the value does, and lies on the same side of the range).
        const auto v = exact_of(unwrap(lo)) +
                       wide_rational<exact_min_limbs>{wide_sint<exact_min_limbs>{1}, wide_sint<exact_min_limbs>{2}} *
                           exact_of_grid<exact_min_limbs>(F);
        if constexpr (checked)
            return try_store<Out, flags_of<Out>>(
                [&](Out& out, auto&& policy) { out = engine::store_exact<Out>(v, policy); });
        else
            return engine::store_exact<Out>(v, make_policy<flags_of<Out>>());
    } else {
        const auto lo                   = fn.template operator()<bracket_t<Out, round_floor>>(anchored_input(xs)...);
        const auto hi                   = fn.template operator()<bracket_t<Out, round_ceil>>(anchored_input(xs)...);
        auto                        mid = [](const auto& a, const auto& b) {
            using ::beman::inside::detail::exact_of;
            const auto s = exact_of(a) + exact_of(b);
            return decltype(s){s.Num, s.Den + s.Den};
        };
        if constexpr (expected_like<decltype(lo)>) {
            using R = std::expected<Out, errc>;
            if (!lo)
                return R{std::unexpected{lo.error()}};
            if (!hi)
                return R{std::unexpected{hi.error()}};
            return ::beman::inside::detail::try_store<Out, flags_of<Out>>(
                [&](Out& out, auto&& policy) { out = engine::store_exact<Out>(mid(*lo, *hi), policy); });
        } else
            return engine::store_exact<Out>(mid(lo, hi), make_policy<flags_of<Out>>());
    }
}

// Whether a call needs via_anchored.
template <insidable Out, insidable... Ins>
inline constexpr bool unanchored_call = !anchored<Out> || (!anchored<Ins> || ...);
} // namespace anchoring

// The tiers in order at runtime: the double kernel (when FP), the dd
// kernel (when DD), the integer path. Each fast tier stores a decided
// result and returns true; whatever runs after the first one is cold.
// Constant evaluation takes the integer path, and so does every call under
// math::driver.IntegerOnly (BEMAN_INSIDE_MATH_INSTRUMENT).
template <insidable Out, bool FP, bool DD, typename F, typename D, typename I>
[[gnu::always_inline]] constexpr auto
tiers([[maybe_unused]] const F& fp, [[maybe_unused]] const D& dd, const I& integer_path) -> decltype(integer_path()) {
#if BEMAN_INSIDE_MATH_INSTRUMENT
    const auto integer = [&] {
        engine::note<engine::probe::integer_run>();
        return integer_path();
    };
#else
    const I& integer = integer_path;
#endif
    if constexpr (FP || DD)
        if !consteval {
            if (!engine::integer_only()) {
                if constexpr (FP) {
                    if (Out r; fp(r))
                        return r;
                    return engine::cold_call([=]() -> decltype(integer()) {
                        if constexpr (DD)
                            if (Out r; dd(r))
                                return r;
                        return integer();
                    });
                } else {
                    if (Out r; dd(r))
                        return r;
                    return engine::cold_call(integer);
                }
            }
        }
    return integer();
}

// A rounding function's body after the table: the tiers, with the double
// and dd attempts as expressions in r (nothing of them without an FPU). FP
// and DD in parentheses.
#ifndef BEMAN_INSIDE_MATH_NO_FP
    #define BEMAN_INSIDE_ENGINE_TIERS(Out, FP, DD, fp_call, dd_call, ...)                                         \
        return ::beman::inside::math::detail::entry::tiers<Out, FP, DD>([=](auto& r) -> bool { return fp_call; }, \
                                                                        [=](auto& r) -> bool { return dd_call; }, \
                                                                        [=] { return __VA_ARGS__; });
#else
    #define BEMAN_INSIDE_ENGINE_TIERS(Out, FP, DD, fp_call, dd_call, ...)                                         \
        ::beman::inside::math::detail::engine::note<::beman::inside::math::detail::engine::probe::integer_run>(); \
        return __VA_ARGS__;
#endif

// The tiers of a one-input kernel fn, where ok holds.
#define BEMAN_INSIDE_ENGINE_KERNEL_TIERS(Out, In, fn, x, ok, ...)                  \
    BEMAN_INSIDE_ENGINE_TIERS(Out,                                                 \
                              (engine::fp_tier<Out, engine::fp_##fn, In> && (ok)), \
                              (engine::dd_tier<Out, In> && (ok)),                  \
                              (engine::fp_attempt<Out, engine::fp_##fn>(x, r)),    \
                              (engine::dd_attempt<Out, engine::fp_##fn>(x, r)),    \
                              __VA_ARGS__)

// The table tier's lookup, inside an eval: In has few slots and every
// result lies in Out's range.
#define BEMAN_INSIDE_ENGINE_TABLE(Out, In, x)                                           \
    if constexpr (engine::table_input<In> && engine::table_output<Out>) {               \
        using table = engine::result_table<Out, In, engine::start_bits<Out>, [](In v) { \
            return core{engine::exact_input(v)};                                        \
        }>;                                                                             \
        if constexpr (table::Table.Valid)                                               \
            return Out::from_raw(table::Table.Raw[engine::offset_of(x)]);               \
    }

// Every transcendental rounds onto the output grid: Out's policy needs a
// rounding mode, like any assignment that may round.
template <insidable Out>
consteval void require_rounding() noexcept {
    static_assert(has_flag(flags_of<Out>, beman::inside::detail::snap) || !engine::notched<Out>,
                  "beman::inside::math: the result is rounded onto Out's grid - Out must permit rounding "
                  "(declare it with round_nearest, round_floor, ...)");
}

// tan's domain: an input interval strictly inside one branch (kπ − π/2,
// kπ + π/2), decided at compile time with π bounded to 2^-200. A lattice
// value is rational and a pole is not, but tan near a pole passes any bound,
// so an interval that holds one is rejected, as log's reaching 0 is. Gap
// bounds the distance from the nearer pole below: |tan| < 1/Gap on In.
namespace tan_branch {
using ::beman::inside::detail::grid_int;
inline constexpr grid_rational notch = grid_rational{1} / grid_rational{grid_int{1} << 200};
inline constexpr grid_rational pi_lo =
    engine::lattice_bound<notch, false>(engine::atan2_core<1>{engine::exact_of<1>(0), engine::exact_of<1>(-1)});
inline constexpr grid_rational pi_hi =
    engine::lattice_bound<notch, true>(engine::atan2_core<1>{engine::exact_of<1>(0), engine::exact_of<1>(-1)});

// A bound on m·π: below (up false) or above.
consteval grid_rational pi_times(const grid_rational& m, bool up) { return (m < 0) != up ? m * pi_hi : m * pi_lo; }

struct result {
    bool          Ok;
    grid_rational Gap;
};

template <insidable In>
consteval result of() {
    using ::beman::inside::detail::grid_round, ::beman::inside::detail::round_mode;
    const grid_rational lo = lower_of<In>, hi = upper_of<In>, half = grid_rational{1} / grid_rational{2};
    const grid_rational k     = grid_round<round_mode::floor>((lo + hi) / (grid_rational{2} * pi_lo) + half);
    const grid_rational below = pi_times(k - half, true), above = pi_times(k + half, false);
    if (!(below < lo && hi < above))
        return {false, grid_rational{0}};
    return {true, std::min(lo - below, above - hi)};
}

// A magnitude bound 2^kmax on tan over In's branch: |tan| < 1/Gap.
template <insidable In>
consteval int kmax_of() {
    using ::beman::inside::detail::grid_round, ::beman::inside::detail::round_mode;
    const grid_rational inv = grid_rational{1} / of<In>().Gap;
    return bit_width_of(::beman::inside::detail::grid_numerator(grid_round<round_mode::ceil>(inv))) + 1;
}
} // namespace tan_branch

template <insidable In>
inline constexpr bool tan_domain = tan_branch::of<In>().Ok;
template <insidable In>
inline constexpr int tan_kmax = tan_branch::kmax_of<In>();

// Base^x for a compile-time integer Base ≥ 2: an int64, or a grid integer of
// any size (one within int64 takes the int64 path; one past it skips the
// double and dd tiers, whose ln Base is a 64-bit table).
template <auto Base>
inline constexpr bool grid_base = std::same_as<decltype(Base), grid_rational>;
template <auto Base>
inline constexpr bool base_ok = [] {
    if constexpr (std::integral<decltype(Base)>)
        return Base >= 2;
    else if constexpr (grid_base<Base>)
        return Base >= 2 && ::beman::inside::detail::grid_denominator(Base) == ::beman::inside::detail::grid_int{1};
    else
        return false;
}();

// pow over a non-negative base grid (pown<E> takes a negative base): 0^y is
// 0 for y > 0 and 1 for y = 0. Its one run-time error is the pole 0^y with
// y < 0 (division_by_zero), possible only when the base grid holds 0 and the
// exponent grid negatives: then the result is an expected, else a plain Out.
template <insidable InB, insidable InE>
inline constexpr bool pow_pole = lower_of<InB> == 0 && lower_of<InE> < 0;

//---------------------------------------------------------------------------
// Deduced outputs on notch N: the range the function takes over In, rounded
// outward to N, with In's policy (rounding to nearest unless it names a
// mode). sin and cos give [-1, 1], tan its range over In's branch, atan2
// [-π, π]. Every one is a plain inside: a domain the input grid leaves is a
// compile error.
//---------------------------------------------------------------------------
namespace results {
template <grid_rational N, insidable In>
using exp =
    engine::increasing_t<In, N, engine::exp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::exp_kmax<In>>>;
template <grid_rational N, insidable In>
using exp2 =
    engine::increasing_t<In, N, engine::exp2_core<engine::input_limbs<In>, engine::in_mag<In>, engine::exp_kmax<In>>>;
template <grid_rational N, insidable In>
using log = engine::increasing_t<In, N, engine::log_core<engine::input_limbs<In>, engine::input_bits<In>>>;
template <grid_rational N, insidable In>
using log2 = engine::increasing_t<In, N, engine::logb_core<engine::input_limbs<In>, 2, engine::input_bits<In>>>;
template <grid_rational N, insidable In>
using log10 = engine::increasing_t<In, N, engine::logb_core<engine::input_limbs<In>, 10, engine::input_bits<In>>>;
template <grid_rational N, insidable In>
using sqrt = engine::increasing_t<In, N, engine::sqrt_core<engine::input_limbs<In>, engine::input_bits<In>>>;
template <grid_rational N, insidable In>
using cbrt = engine::increasing_t<In, N, engine::cbrt_core<engine::input_limbs<In>, engine::input_bits<In>>>;
template <grid_rational N, insidable In>
using atan = engine::increasing_t<In, N, engine::atan_core<engine::input_limbs<In>>>;
template <grid_rational N, insidable In>
using asin = engine::increasing_t<In, N, engine::asin_core<engine::input_limbs<In>, engine::input_bits<In>>>;
template <grid_rational N, insidable In>
using acos = engine::decreasing_t<In, N, engine::acos_core<engine::input_limbs<In>, engine::input_bits<In>>>;
template <grid_rational N, insidable In>
using sinh = engine::increasing_t<
    In,
    N,
    engine::hyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::hyp::sinh, engine::exp_kmax<In>>>;
template <grid_rational N, insidable In>
using tanh =
    engine::increasing_t<In, N, engine::hyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::hyp::tanh, 1>>;
template <grid_rational N, insidable In>
using asinh = engine::increasing_t<
    In,
    N,
    engine::ahyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::ahyp::asinh, engine::input_bits<In>>>;
template <grid_rational N, insidable In>
using acosh = engine::increasing_t<
    In,
    N,
    engine::ahyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::ahyp::acosh, engine::input_bits<In>>>;
template <grid_rational N, insidable In>
using atanh = engine::increasing_t<
    In,
    N,
    engine::ahyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::ahyp::atanh, engine::input_bits<In>>>;
template <grid_rational N, insidable In>
using sin = inside<{{-1, 1}, N}, engine::result_policy<In>>;
template <grid_rational N, insidable In>
using cos = sin<N, In>;
// tan is increasing on its branch (tan_domain).
template <grid_rational N, insidable In>
using tan = engine::increasing_t<
    In,
    N,
    engine::trig_core<engine::input_limbs<In>, engine::in_mag<In>, engine::trig::tan, tan_kmax<In>>>;

// cosh is even: its least value is 1 when In spans 0, else at the end
// nearer 0.
template <insidable In>
using cosh_core =
    engine::hyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::hyp::cosh, engine::exp_kmax<In>>;
template <grid_rational N, insidable In>
inline constexpr grid_rational cosh_lo = (lower_of<In> <= 0 && upper_of<In> >= 0) ? grid_rational{1}
                                         : (lower_of<In> > 0) ? engine::bound_at<In, N, cosh_core<In>, false, false>
                                                              : engine::bound_at<In, N, cosh_core<In>, true, false>;
template <grid_rational N, insidable In>
inline constexpr grid_rational cosh_hi =
    std::max(engine::bound_at<In, N, cosh_core<In>, true, true>, engine::bound_at<In, N, cosh_core<In>, false, true>);
template <grid_rational N, insidable In>
using cosh = engine::result_grid_t<In, N, cosh_lo<N, In>, cosh_hi<N, In>>;

// max(|Lower|, |Upper|) (hypot's bound).
template <insidable In>
inline constexpr grid_rational max_abs = std::max(upper_of<In>, -lower_of<In>);

// pow_base: pow_core with the base bound.
template <auto Base, std::size_t E, int Mag, int KMax>
struct pow_base_core : engine::pow_core<engine::base_limbs<Base>, E, Mag, KMax> {
    constexpr pow_base_core(::beman::inside::detail::wide_rational<E> x)
        : engine::pow_core<engine::base_limbs<Base>, E, Mag, KMax>{engine::exact_base<Base>(), x} {}
};
template <auto Base, insidable In>
inline constexpr int pow_base_kmax = [] {
    static_assert(engine::max_abs_int<In> <= (std::numeric_limits<int>::max() - 2) / engine::base_bits<Base>,
                  "beman::inside::math::pow_base: the deduced output needs |x|·bit_width(Base) below 2^31 - name "
                  "the output: pow_base<B, C>(x)");
    return static_cast<int>(engine::max_abs_int<In>) * engine::base_bits<Base> + 2;
}();
template <grid_rational N, auto Base, insidable In>
using pow_base = engine::
    increasing_t<In, N, pow_base_core<Base, engine::input_limbs<In>, engine::in_mag<In>, pow_base_kmax<Base, In>>>;

// atan2: [−π, π] rounded outward on N (π = atan2(0, −1)).
template <grid_rational N>
inline constexpr grid_rational pi_up =
    engine::lattice_bound<N, true>(engine::atan2_core<1>{engine::exact_of<1>(0), engine::exact_of<1>(-1)});
template <grid_rational N, insidable InY, insidable InX>
using atan2 = inside<{{-pi_up<N>, pi_up<N>}, N}, engine::result_policy<InY>>;

// hypot: [0, hypot of the largest magnitudes] on N.
template <insidable InX, insidable InY>
inline constexpr std::size_t hypot_limbs = ::beman::inside::detail::limbs_for_bits(engine::hypot_bits<InX, InY>);
// (A consteval function, not an immediately invoked lambda: the lambda would
// become immediate, and GCC 16 then takes the address of its invoker.)
template <grid_rational N, insidable InX, insidable InY>
consteval grid_rational hypot_upper() {
    using F   = ::beman::inside::detail::wide_rational<hypot_limbs<InX, InY>>;
    const F a = ::beman::inside::detail::exact_of_grid<hypot_limbs<InX, InY>>(max_abs<InX>);
    const F b = ::beman::inside::detail::exact_of_grid<hypot_limbs<InX, InY>>(max_abs<InY>);
    return engine::lattice_bound<N, true>(
        engine::sqrt_core<hypot_limbs<InX, InY>, engine::hypot_bits<InX, InY>>{a * a + b * b});
}
template <grid_rational N, insidable InX, insidable InY>
inline constexpr grid_rational hypot_hi = hypot_upper<N, InX, InY>();
template <grid_rational N, insidable InX, insidable InY>
using hypot = inside<{{0, hypot_hi<N, InX, InY>}, N}, engine::result_policy<InX>>;

// pow: b^e is monotone in each argument for b > 0, so the extremes are at
// the corners of the input rectangle.
template <insidable InB, insidable InE>
using pow_core = engine::pow_core<engine::input_limbs<InB>,
                                  engine::input_limbs<InE>,
                                  engine::in_mag<InE>,
                                  engine::pow_kmax<InB, InE>,
                                  engine::input_bits<InB>,
                                  engine::input_bits<InE>>;
template <grid_rational N, insidable InB, insidable InE, bool BUp, bool EUp, bool Up>
inline constexpr grid_rational pow_corner =
    engine::lattice_bound<N, Up>(pow_core<InB, InE>{engine::grid_input<InB>(BUp ? upper_of<InB> : lower_of<InB>),
                                                    engine::grid_input<InE>(EUp ? upper_of<InE> : lower_of<InE>)});
// The least (Up: greatest) bound over the four corners.
template <grid_rational N, insidable InB, insidable InE, bool Up>
inline constexpr grid_rational pow_extreme = [] {
    static_assert(lower_of<InB> > 0,
                  "beman::inside::math::pow: the deduced output needs a base grid above 0 - name the output: "
                  "pow<C>(b, e)");
    const std::initializer_list<grid_rational> c{pow_corner<N, InB, InE, false, false, Up>,
                                                 pow_corner<N, InB, InE, false, true, Up>,
                                                 pow_corner<N, InB, InE, true, false, Up>,
                                                 pow_corner<N, InB, InE, true, true, Up>};
    return Up ? std::max(c) : std::min(c);
}();
template <grid_rational N, insidable InB, insidable InE>
using pow = inside<{{pow_extreme<N, InB, InE, false>, pow_extreme<N, InB, InE, true>}, N}, engine::result_policy<InB>>;
} // namespace results

//---------------------------------------------------------------------------
// The tags (math/detail/functions.hpp): what every rounding function has in
// common, by arity; then each one's domain, deduced output and evaluation.
//---------------------------------------------------------------------------
template <std::size_t Arity>
struct rounding_fn {
    static constexpr bool rounds = true;
    template <class... In>
    static constexpr bool takes = sizeof...(In) == Arity && (insidable<In> && ...);
    template <class... In>
    static constexpr bool domain = true;
    // The arguments' notch: the gcd of their value units (0 when one has none).
    template <class... In>
    static constexpr grid_rational notch = [] {
        if constexpr (Arity == 1)
            return notch_of<In...[0]>;
        else
            return engine::gcd_notch<In...[0], In...[1]>;
    }();
};

// One-input functions: the domain (checked on In's grid, `true` for none),
// the double kernel's argument range (fp_ok), and the integer core.
#define BEMAN_INSIDE_ENGINE_UNARY(fn, domain_v, msg, fp_ok, ...)                                                 \
    struct fn##_fn : rounding_fn<1> {                                                                            \
        template <class... In>                                                                                   \
        static constexpr bool domain = [] {                                                                      \
            using In0 [[maybe_unused]] = In...[0];                                                               \
            return domain_v;                                                                                     \
        }();                                                                                                     \
        template <grid_rational N, insidable In>                                                                 \
        using result_on = results::fn<N, In>;                                                                    \
        template <insidable Out, insidable In>                                                                   \
        [[nodiscard]] static constexpr Out eval(In x) {                                                          \
            static_assert(domain<In>, "beman::inside::math::" #fn ": " msg);                                     \
            require_rounding<Out>();                                                                             \
            if constexpr (anchoring::unanchored_call<Out, In>)                                                   \
                return anchoring::via_anchored<Out>([]<insidable O>(auto v) { return fn##_fn::eval<O>(v); }, x); \
            else {                                                                                               \
                using core = __VA_ARGS__;                                                                        \
                BEMAN_INSIDE_ENGINE_TABLE(Out, In, x)                                                            \
                BEMAN_INSIDE_ENGINE_KERNEL_TIERS(                                                                \
                    Out,                                                                                         \
                    In,                                                                                          \
                    fn,                                                                                          \
                    x,                                                                                           \
                    (fp_ok),                                                                                     \
                    engine::evaluate<Out, engine::start_bits<Out>>(core{engine::exact_input(x)}))                \
            }                                                                                                    \
        }                                                                                                        \
    };

BEMAN_INSIDE_ENGINE_UNARY(exp,
                          true,
                          "",
                          engine::in_max<In> <= 700,
                          engine::exp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::exp_out_kmax<Out, In>>)
BEMAN_INSIDE_ENGINE_UNARY(
    exp2,
    true,
    "",
    engine::in_max<In> <= 1000,
    engine::exp2_core<engine::input_limbs<In>, engine::in_mag<In>, engine::exp_out_kmax<Out, In>>)
BEMAN_INSIDE_ENGINE_UNARY(sin,
                          true,
                          "",
                          engine::in_max<In> <= 0x1p20,
                          engine::trig_core<engine::input_limbs<In>, engine::in_mag<In>, engine::trig::sin, 1>)
BEMAN_INSIDE_ENGINE_UNARY(cos,
                          true,
                          "",
                          engine::in_max<In> <= 0x1p20,
                          engine::trig_core<engine::input_limbs<In>, engine::in_mag<In>, engine::trig::cos, 1>)
BEMAN_INSIDE_ENGINE_UNARY(atan, true, "", true, engine::atan_core<engine::input_limbs<In>>)
BEMAN_INSIDE_ENGINE_UNARY(
    sinh,
    true,
    "",
    engine::in_max<In> <= 700,
    engine::hyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::hyp::sinh, engine::exp_out_kmax<Out, In>>)
BEMAN_INSIDE_ENGINE_UNARY(
    cosh,
    true,
    "",
    engine::in_max<In> <= 700,
    engine::hyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::hyp::cosh, engine::exp_out_kmax<Out, In>>)
BEMAN_INSIDE_ENGINE_UNARY(tanh,
                          true,
                          "",
                          engine::in_max<In> <= 300,
                          engine::hyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::hyp::tanh, 1>)
BEMAN_INSIDE_ENGINE_UNARY(
    asinh,
    true,
    "",
    engine::in_max<In> <= 0x1p500,
    engine::ahyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::ahyp::asinh, engine::input_bits<In>>)
BEMAN_INSIDE_ENGINE_UNARY(cbrt, true, "", true, engine::cbrt_core<engine::input_limbs<In>, engine::input_bits<In>>)
BEMAN_INSIDE_ENGINE_UNARY(log,
                          (lower_of<In0> > 0),
                          "input must be strictly positive",
                          true,
                          engine::log_core<engine::input_limbs<In>, engine::input_bits<In>>)
BEMAN_INSIDE_ENGINE_UNARY(log2,
                          (lower_of<In0> > 0),
                          "input must be strictly positive",
                          true,
                          engine::logb_core<engine::input_limbs<In>, 2, engine::input_bits<In>>)
BEMAN_INSIDE_ENGINE_UNARY(log10,
                          (lower_of<In0> > 0),
                          "input must be strictly positive",
                          true,
                          engine::logb_core<engine::input_limbs<In>, 10, engine::input_bits<In>>)
BEMAN_INSIDE_ENGINE_UNARY(asin,
                          (lower_of<In0> >= -1 && upper_of<In0> <= 1),
                          "input must be in [-1, 1]",
                          true,
                          engine::asin_core<engine::input_limbs<In>, engine::input_bits<In>>)
BEMAN_INSIDE_ENGINE_UNARY(acos,
                          (lower_of<In0> >= -1 && upper_of<In0> <= 1),
                          "input must be in [-1, 1]",
                          true,
                          engine::acos_core<engine::input_limbs<In>, engine::input_bits<In>>)
BEMAN_INSIDE_ENGINE_UNARY(
    acosh,
    (lower_of<In0> >= 1),
    "input must be at least 1",
    engine::in_max<In> <= 0x1p500,
    engine::ahyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::ahyp::acosh, engine::input_bits<In>>)
BEMAN_INSIDE_ENGINE_UNARY(
    atanh,
    (lower_of<In0> > -1 && upper_of<In0> < 1),
    "input must be in (-1, 1)",
    true,
    engine::ahyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::ahyp::atanh, engine::input_bits<In>>)
#undef BEMAN_INSIDE_ENGINE_UNARY

// sqrt of a non-negative input. A grid that reaches below 0 is rejected, as
// log's reaching 0 is: the domain is the input grid's, decided at compile time.
struct sqrt_fn : rounding_fn<1> {
    template <class... In>
    static constexpr bool domain = lower_of<In...[0]> >= 0;
    template <grid_rational N, insidable In>
    using result_on = results::sqrt<N, In>;
    template <insidable Out, insidable In>
    [[nodiscard]] static constexpr Out eval(In x) {
        static_assert(domain<In>,
                      "beman::inside::math::sqrt: input must be non-negative - take abs(x), or store x into a "
                      "non-negative grid first");
        require_rounding<Out>();
        if constexpr (anchoring::unanchored_call<Out, In>)
            return anchoring::via_anchored<Out>([]<insidable O>(auto v) { return sqrt_fn::eval<O>(v); }, x);
        else {
            BEMAN_INSIDE_ENGINE_KERNEL_TIERS(
                Out,
                In,
                sqrt,
                x,
                true,
                engine::evaluate<Out, engine::start_bits<Out>>(
                    engine::sqrt_core<engine::input_limbs<In>, engine::input_bits<In>>{engine::exact_input(x)}))
        }
    }
};

// tan on one branch: a plain Out. A result past Out is Out's store, which its
// policy reports or clamps.
struct tan_fn : rounding_fn<1> {
    template <class... In>
    static constexpr bool domain = tan_domain<In...[0]>;
    template <grid_rational N, insidable In>
    using result_on = results::tan<N, In>;
    template <insidable Out, insidable In>
    [[nodiscard]] static constexpr Out eval(In x) {
        static_assert(tan_domain<In>,
                      "beman::inside::math::tan: the input interval holds a pole (an odd multiple of pi/2) - narrow "
                      "it to one branch, such as (-pi/2, pi/2)");
        require_rounding<Out>();
        static_assert(!has_flag(flags_of<Out>, wrap),
                      "beman::inside::math::tan: wrap is not supported - tan has no bound near its poles, so "
                      "the folded value cannot be computed; take an Out with clamp or the default policy");
        if constexpr (anchoring::unanchored_call<Out, In>)
            return anchoring::via_anchored<Out>([]<insidable O>(auto v) { return tan_fn::eval<O>(v); }, x);
        else {
            using core = engine::
                trig_core<engine::input_limbs<In>, engine::in_mag<In>, engine::trig::tan, engine::out_kmax<Out>>;
            BEMAN_INSIDE_ENGINE_TIERS(Out,
                                      (engine::fp_tier<Out, engine::fp_tan, In> && engine::in_max<In> <= 0x1p20),
                                      (engine::dd_tier<Out, In> && engine::in_max<In> <= 0x1p20),
                                      (engine::fp_attempt_tan<Out>(x, r)),
                                      (engine::dd_attempt_tan<Out>(x, r)),
                                      engine::evaluate<Out, engine::start_bits<Out>>(core{engine::exact_input(x)}))
        }
    }
};

struct atan2_fn : rounding_fn<2> {
    template <grid_rational N, insidable InY, insidable InX>
    using result_on = results::atan2<N, InY, InX>;
    template <insidable Out, insidable InY, insidable InX>
    [[nodiscard]] static constexpr Out eval(InY y, InX x) {
        require_rounding<Out>();
        if constexpr (anchoring::unanchored_call<Out, InY, InX>)
            return anchoring::via_anchored<Out>(
                []<insidable O>(auto a, auto b) { return atan2_fn::eval<O>(a, b); }, y, x);
        else {
            constexpr std::size_t E = std::max(engine::input_limbs<InY>, engine::input_limbs<InX>);
            using F                 = ::beman::inside::detail::wide_rational<E>;
            BEMAN_INSIDE_ENGINE_TIERS(Out,
                                      (engine::fp_tier<Out, engine::fp_atan2, InY, InX> &&
                                       engine::in_max<InY> <= 0x1p500 && engine::in_max<InX> <= 0x1p500),
                                      (engine::dd_tier<Out, InY, InX>),
                                      (engine::fp_attempt_atan2<Out>(y, x, r)),
                                      (engine::dd_attempt_atan2<Out>(y, x, r)),
                                      engine::evaluate<Out, engine::start_bits<Out>>(
                                          engine::atan2_core<E>{F{engine::exact_input(y)}, F{engine::exact_input(x)}}))
        }
    }
};

struct hypot_fn : rounding_fn<2> {
    template <grid_rational N, insidable InX, insidable InY>
    using result_on = results::hypot<N, InX, InY>;
    template <insidable Out, insidable InX, insidable InY>
    [[nodiscard]] static constexpr Out eval(InX x, InY y) {
        require_rounding<Out>();
        if constexpr (anchoring::unanchored_call<Out, InX, InY>)
            return anchoring::via_anchored<Out>(
                []<insidable O>(auto a, auto b) { return hypot_fn::eval<O>(a, b); }, x, y);
        else {
            constexpr int         Bits = engine::hypot_bits<InX, InY>;
            constexpr std::size_t E    = ::beman::inside::detail::limbs_for_bits(Bits);
            using F                    = ::beman::inside::detail::wide_rational<E>;
            BEMAN_INSIDE_ENGINE_TIERS(Out,
                                      (engine::fp_tier<Out, engine::fp_hypot, InX, InY> &&
                                       engine::in_max<InX> <= 0x1p500 && engine::in_max<InY> <= 0x1p500),
                                      (engine::dd_tier<Out, InX, InY>),
                                      (engine::fp_attempt_hypot<Out>(x, y, r)),
                                      (engine::dd_attempt_hypot<Out>(x, y, r)),
                                      engine::evaluate<Out, engine::start_bits<Out>>(engine::sqrt_core<E, Bits>{
                                          F{engine::exact_input(x)} * F{engine::exact_input(x)} +
                                          F{engine::exact_input(y)} * F{engine::exact_input(y)}}))
        }
    }
};

// A result past Out is Out's store, which its policy reports or clamps. The
// deduced output takes the base's notch.
struct pow_fn : rounding_fn<2> {
    template <class... In>
    static constexpr bool domain = lower_of<In...[0]> >= 0;
    template <class InB, class InE>
    static constexpr grid_rational notch = ::beman::inside::detail::notched<InE> ? notch_of<InB> : grid_rational{0};
    template <grid_rational N, insidable InB, insidable InE>
    using result_on = results::pow<N, InB, InE>;
    template <insidable Out, insidable InB, insidable InE>
    [[nodiscard]] static constexpr auto eval(InB base, InE exp)
        -> std::conditional_t<pow_pole<InB, InE>, std::expected<Out, errc>, Out> {
        static_assert(lower_of<InB> >= 0,
                      "beman::inside::math::pow: the base must be non-negative - pown<E>(x) takes a negative base "
                      "with an integer exponent");
        require_rounding<Out>();
        if constexpr (anchoring::unanchored_call<Out, InB, InE>)
            return anchoring::via_anchored<Out>(
                []<insidable O>(auto a, auto b) { return pow_fn::eval<O>(a, b); }, base, exp);
        else {
            using R            = std::conditional_t<pow_pole<InB, InE>, std::expected<Out, errc>, Out>;
            const auto integer = [=]() -> R {
                const auto b = engine::exact_input(base);
                if (b.Num.is_zero()) {
                    const auto e = engine::exact_input(exp);
                    if constexpr (pow_pole<InB, InE>) {
                        if (e.Num.negative())
                            return std::unexpected(errc::division_by_zero);
                        return Out::try_make(e.Num.is_zero() ? 1 : 0); // Out's policy: range, clamp
                    } else
                        return beman::inside::detail::stored<Out>(e.Num.is_zero() ? 1 : 0);
                }
                constexpr int KMax = engine::pow_out_kmax<Out, InB, InE>;
                using core         = engine::pow_core<engine::input_limbs<InB>,
                                                      engine::input_limbs<InE>,
                                                      engine::in_mag<InE>,
                                                      KMax,
                                                      engine::input_bits<InB>,
                                                      engine::input_bits<InE>,
                                                      0,
                                                      engine::pow_exact_limbs<Out, KMax>>;
                if constexpr (pow_pole<InB, InE>)
                    return engine::evaluate_checked<Out, engine::start_bits<Out>>(core{b, engine::exact_input(exp)});
                else
                    return engine::evaluate<Out, engine::start_bits<Out>>(core{b, engine::exact_input(exp)});
            };
            BEMAN_INSIDE_ENGINE_TIERS(Out,
                                      (engine::fp_tier<Out, engine::fp_pow, InB, InE>),
                                      (engine::dd_tier<Out, InB, InE>),
                                      (engine::fp_attempt_pow<Out>(base, exp, r)),
                                      (engine::dd_attempt_pow<Out>(base, exp, r)),
                                      integer())
        }
    }
};

// Base^x.
template <auto Base>
struct pow_base_fn : rounding_fn<1> {
    template <class... In>
    static constexpr bool domain = base_ok<Base>;
    template <grid_rational N, insidable In>
    using result_on = results::pow_base<N, Base, In>;
    template <insidable Out, insidable In>
    [[nodiscard]] static constexpr Out eval(In x) {
        static_assert(base_ok<Base>, "beman::inside::math::pow_base: Base must be an integer of at least 2");
        require_rounding<Out>();
        if constexpr (grid_base<Base> && ::beman::inside::detail::fits_int64(interval{Base, Base}))
            return pow_base_fn<static_cast<std::int64_t>(
                ::beman::inside::detail::grid_numerator(Base))>::template eval<Out>(x);
        else if constexpr (std::integral<decltype(Base)> &&
                           !::beman::inside::detail::fits_int64(interval{grid_rational{Base}, grid_rational{Base}}))
            return pow_base_fn<grid_rational{Base}>::template eval<Out>(x); // a builtin past int64: the grid path
        else if constexpr (anchoring::unanchored_call<Out, In>)
            return anchoring::via_anchored<Out>([]<insidable O>(auto v) { return pow_base_fn::eval<O>(v); }, x);
        else {
            constexpr int KMax = engine::pow_base_out_kmax<Out, In, Base>;
            using core         = engine::pow_core<engine::base_limbs<Base>,
                                                  engine::input_limbs<In>,
                                                  engine::in_mag<In>,
                                                  KMax,
                                                  grid_base<Base> ? engine::base_bits<Base> + 2 : 66,
                                                  engine::input_bits<In>,
                                                  Base,
                                                  engine::pow_exact_limbs<Out, KMax>>;
            BEMAN_INSIDE_ENGINE_TIERS(
                Out,
                (!grid_base<Base> && engine::fp_tier<Out, engine::fp_pow_base, In> && engine::in_max<In> <= 1000),
                (!grid_base<Base> && engine::dd_tier<Out, In> && engine::in_max<In> <= 700),
                (engine::fp_attempt_pow_base<Out, Base>(x, r)),
                (engine::dd_attempt_pow_base<Out, Base>(x, r)),
                engine::evaluate<Out, engine::start_bits<Out>>(
                    core{engine::exact_base<Base>(), engine::exact_input(x)}))
        }
    }
};
} // namespace beman::inside::math::detail::entry

namespace beman::inside::math::detail {
// The grids over which a deduced output (and so a checked named one) exists.
template <class In>
inline constexpr bool range_bounded<entry::exp_fn, In> = engine::max_abs_int<In> <= 4096;
template <class In>
inline constexpr bool range_bounded<entry::exp2_fn, In> = engine::max_abs_int<In> <= 4096;
template <class In>
inline constexpr bool range_bounded<entry::sinh_fn, In> = engine::max_abs_int<In> <= 4096;
template <class In>
inline constexpr bool range_bounded<entry::cosh_fn, In> = engine::max_abs_int<In> <= 4096;
template <insidable InB, insidable InE>
consteval bool pow_bounded() {
    if constexpr (!(lower_of<InB> > 0))
        return false;
    else
        return engine::max_abs_int<InE> * engine::log2_span<InB> <= (1 << 16);
}
template <class InB, class InE>
inline constexpr bool range_bounded<entry::pow_fn, InB, InE> = pow_bounded<InB, InE>();
template <auto Base, class In>
inline constexpr bool range_bounded<entry::pow_base_fn<Base>, In> =
    engine::max_abs_int<In> <= (std::numeric_limits<int>::max() - 2) / engine::base_bits<Base>;
} // namespace beman::inside::math::detail

#undef BEMAN_INSIDE_ENGINE_TIERS
#undef BEMAN_INSIDE_ENGINE_KERNEL_TIERS
#undef BEMAN_INSIDE_ENGINE_TABLE

#endif // BEMAN_INSIDE_MATH_DETAIL_ENTRY_HPP
