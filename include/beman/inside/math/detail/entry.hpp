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
// beman::inside::math::adaptive — the math engine behind math.hpp (which
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
namespace beman::inside::math::adaptive {
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
// imax. Rounded down there, the value lies in [y, y + F); only when y is
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
template <insidable Out, imax den>
constexpr std::optional<imax> bracket_slot(imax num, bool negative) noexcept {
    using namespace ::beman::inside::detail;
    constexpr round_mode m = rounding_for<Out, call_policy<none>>;
    const auto [q, r]      = floor_divmod(num, den);
    const imax k           = q + rounds_up(m,
                                           negative,
                                           classify_remainder(m, static_cast<umax>(r), static_cast<umax>(den)),
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
        constexpr grid_rational F       = fine_unit<Out>;
        constexpr imax          c       = static_cast<imax>(exact_quotient(lower_of<Out>, F)); // Lower in F
        constexpr imax          s       = static_cast<imax>(exact_quotient(notch_of<Out>, F)); // Notch in F (even)
        using Lo                        = bracket_t<Out, round_floor, F>;
        using Hi                        = bracket_t<Out, round_ceil, F>;
        using R                         = decltype(fn.template operator()<Lo>(anchored_input(xs)...));
        constexpr bool checked          = expected_like<R>;
        using Res                       = std::conditional_t<checked, std::expected<Out, errc>, Out>;
        const R lo                      = fn.template operator()<Lo>(anchored_input(xs)...);
        const auto               unwrap = [](const auto& r) -> const auto& {
            if constexpr (checked)
                return *r;
            else
                return r;
        };
        if constexpr (checked)
            if (!lo)
                return Res{std::unexpected{lo.error()}};
        const imax j = value_in_units<imax, F>(unwrap(lo)); // y = j·F
        const imax t = j - c;                               // y − Lower, in F
        // The value lies inside (y, y + F): 2t + 1 half units of F.
        std::optional<imax> k;
        if (t % (s / 2) != 0) [[likely]]
            k = bracket_slot<Out, 2 * s>(2 * t + 1, j < 0);
        else { // y is a boundary of Out: is the value exactly y?
            const auto hi = fn.template operator()<Hi>(anchored_input(xs)...);
            if constexpr (checked)
                if (!hi)
                    return Res{std::unexpected{hi.error()}};
            k = value_in_units<imax, F>(unwrap(hi)) == j ? bracket_slot<Out, s>(t, j < 0)
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
                    return engine::cold_call([&]() -> decltype(integer()) {
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

// An _into form's body after the table: the tiers, with the double and
// dd attempts as expressions in r (nothing of them without an FPU). FP and
// DD in parentheses.
#ifndef BEMAN_INSIDE_MATH_NO_FP
    #define BEMAN_INSIDE_ENGINE_TIERS(Out, FP, DD, fp_call, dd_call, ...)                                    \
        return ::beman::inside::math::adaptive::tiers<Out, FP, DD>([&](auto& r) -> bool { return fp_call; }, \
                                                                   [&](auto& r) -> bool { return dd_call; }, \
                                                                   [&] { return __VA_ARGS__; });
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

// The table tier's lookup, inside an _into form: In has few slots and every
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
    static_assert(has_flag(flags_of<Out>, snap) || !engine::notched<Out>,
                  "beman::inside::math: the result is rounded onto Out's grid - Out must permit rounding "
                  "(declare it with round_nearest, round_floor, ...)");
}

// One-input functions: the domain (checked on In's grid, `true` for none),
// the double kernel's argument range (fp_ok), and the integer core.
#define BEMAN_INSIDE_ENGINE_UNARY(fn, domain, msg, fp_ok, ...)                                           \
    template <insidable In>                                                                              \
    inline constexpr bool fn##_domain = domain;                                                          \
    template <insidable Out, insidable In>                                                               \
    [[nodiscard]] constexpr Out fn##_into(In x) {                                                        \
        static_assert(fn##_domain<In>, "beman::inside::math::" #fn ": " msg);                            \
        require_rounding<Out>();                                                                         \
        if constexpr (anchoring::unanchored_call<Out, In>)                                               \
            return anchoring::via_anchored<Out>([]<insidable O>(auto v) { return fn##_into<O>(v); }, x); \
        else {                                                                                           \
            using core = __VA_ARGS__;                                                                    \
            BEMAN_INSIDE_ENGINE_TABLE(Out, In, x)                                                        \
            BEMAN_INSIDE_ENGINE_KERNEL_TIERS(                                                            \
                Out,                                                                                     \
                In,                                                                                      \
                fn,                                                                                      \
                x,                                                                                       \
                (fp_ok),                                                                                 \
                engine::evaluate<Out, engine::start_bits<Out>>(core{engine::exact_input(x)}))            \
        }                                                                                                \
    }

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
                          (lower_of<In> > 0),
                          "input must be strictly positive",
                          true,
                          engine::log_core<engine::input_limbs<In>, engine::input_bits<In>>)
BEMAN_INSIDE_ENGINE_UNARY(log2,
                          (lower_of<In> > 0),
                          "input must be strictly positive",
                          true,
                          engine::logb_core<engine::input_limbs<In>, 2, engine::input_bits<In>>)
BEMAN_INSIDE_ENGINE_UNARY(log10,
                          (lower_of<In> > 0),
                          "input must be strictly positive",
                          true,
                          engine::logb_core<engine::input_limbs<In>, 10, engine::input_bits<In>>)
BEMAN_INSIDE_ENGINE_UNARY(asin,
                          (lower_of<In> >= -1 && upper_of<In> <= 1),
                          "input must be in [-1, 1]",
                          true,
                          engine::asin_core<engine::input_limbs<In>, engine::input_bits<In>>)
BEMAN_INSIDE_ENGINE_UNARY(acos,
                          (lower_of<In> >= -1 && upper_of<In> <= 1),
                          "input must be in [-1, 1]",
                          true,
                          engine::acos_core<engine::input_limbs<In>, engine::input_bits<In>>)
BEMAN_INSIDE_ENGINE_UNARY(
    acosh,
    (lower_of<In> >= 1),
    "input must be at least 1",
    engine::in_max<In> <= 0x1p500,
    engine::ahyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::ahyp::acosh, engine::input_bits<In>>)
BEMAN_INSIDE_ENGINE_UNARY(
    atanh,
    (lower_of<In> > -1 && upper_of<In> < 1),
    "input must be in (-1, 1)",
    true,
    engine::ahyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::ahyp::atanh, engine::input_bits<In>>)
#undef BEMAN_INSIDE_ENGINE_UNARY

// sqrt of a non-negative input.
template <insidable Out, insidable In>
    requires(lower_of<In> >= 0)
[[nodiscard]] constexpr Out sqrt_into(In x) {
    require_rounding<Out>();
    if constexpr (anchoring::unanchored_call<Out, In>)
        return anchoring::via_anchored<Out>([]<insidable O>(auto v) { return sqrt_into<O>(v); }, x);
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

// sqrt of a mixed-sign input: domain_error on a negative value.
template <insidable Out, insidable In>
    requires(lower_of<In> < 0)
[[nodiscard]] constexpr std::expected<Out, errc> sqrt_into(In x) {
    require_rounding<Out>();
    if constexpr (anchoring::unanchored_call<Out, In>)
        return anchoring::via_anchored<Out>([]<insidable O>(auto v) { return sqrt_into<O>(v); }, x);
    else {
        const auto v = engine::exact_input(x);
        if (v.Num.negative())
            return std::unexpected(errc::domain_error);
        return engine::evaluate<Out, engine::start_bits<Out>>(
            engine::sqrt_core<engine::input_limbs<In>, engine::input_bits<In>>{v});
    }
}

// tan: overflow when the result leaves Out (without clamp).
template <insidable In>
inline constexpr bool tan_domain = true;
template <insidable Out, insidable In>
[[nodiscard]] constexpr std::expected<Out, errc> tan_into(In x) {
    require_rounding<Out>();
    static_assert(!has_flag(flags_of<Out>, wrap),
                  "beman::inside::math::tan: wrap is not supported - tan has no bound near its poles, so "
                  "the folded value cannot be computed; take an Out with clamp or the default policy");
    if constexpr (anchoring::unanchored_call<Out, In>)
        return anchoring::via_anchored<Out>([]<insidable O>(auto v) { return tan_into<O>(v); }, x);
    else {
        using core =
            engine::trig_core<engine::input_limbs<In>, engine::in_mag<In>, engine::trig::tan, engine::out_kmax<Out>>;
        BEMAN_INSIDE_ENGINE_TIERS(Out,
                                  (engine::fp_tier<Out, engine::fp_tan, In> && engine::in_max<In> <= 0x1p20),
                                  (engine::dd_tier<Out, In> && engine::in_max<In> <= 0x1p20),
                                  (engine::fp_attempt_tan<Out>(x, r)),
                                  (engine::dd_attempt_tan<Out>(x, r)),
                                  engine::evaluate_checked<Out, engine::start_bits<Out>>(core{engine::exact_input(x)}))
    }
}

template <insidable Out, insidable InY, insidable InX>
[[nodiscard]] constexpr Out atan2_into(InY y, InX x) {
    require_rounding<Out>();
    if constexpr (anchoring::unanchored_call<Out, InY, InX>)
        return anchoring::via_anchored<Out>([]<insidable O>(auto a, auto b) { return atan2_into<O>(a, b); }, y, x);
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

template <insidable Out, insidable InX, insidable InY>
[[nodiscard]] constexpr Out hypot_into(InX x, InY y) {
    require_rounding<Out>();
    if constexpr (anchoring::unanchored_call<Out, InX, InY>)
        return anchoring::via_anchored<Out>([]<insidable O>(auto a, auto b) { return hypot_into<O>(a, b); }, x, y);
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

// pow: 0^y is 0 for y > 0 and 1 for y = 0; division_by_zero for 0^y with
// y < 0 (a pole); domain_error for a negative base; overflow when the
// result leaves Out (without clamp).
template <insidable Out, insidable InB, insidable InE>
[[nodiscard]] constexpr std::expected<Out, errc> pow_into(InB base, InE exp) {
    require_rounding<Out>();
    if constexpr (anchoring::unanchored_call<Out, InB, InE>)
        return anchoring::via_anchored<Out>([]<insidable O>(auto a, auto b) { return pow_into<O>(a, b); }, base, exp);
    else {
        const auto integer = [&]() -> std::expected<Out, errc> {
            const auto b = engine::exact_input(base);
            if (b.Num.negative())
                return std::unexpected(errc::domain_error);
            if (b.Num.is_zero()) {
                const auto e = engine::exact_input(exp);
                if (e.Num.negative())
                    return std::unexpected(errc::division_by_zero);
                return Out::try_make(e.Num.is_zero() ? 1 : 0); // Out's policy: range, clamp
            }
            using core = engine::pow_core<engine::input_limbs<InB>,
                                          engine::input_limbs<InE>,
                                          engine::in_mag<InE>,
                                          engine::pow_out_kmax<Out, InB, InE>,
                                          engine::input_bits<InB>,
                                          engine::input_bits<InE>>;
            return engine::evaluate_checked<Out, engine::start_bits<Out>>(core{b, engine::exact_input(exp)});
        };
        BEMAN_INSIDE_ENGINE_TIERS(Out,
                                  (engine::fp_tier<Out, engine::fp_pow, InB, InE>),
                                  (engine::dd_tier<Out, InB, InE>),
                                  (engine::fp_attempt_pow<Out>(base, exp, r)),
                                  (engine::dd_attempt_pow<Out>(base, exp, r)),
                                  integer())
    }
}

// Base^x for a compile-time integer Base ≥ 2.
template <insidable Out, imax Base, insidable In>
[[nodiscard]] constexpr Out pow_base_into(In x) {
    static_assert(Base >= 2, "beman::inside::math::pow_base: Base must be at least 2");
    require_rounding<Out>();
    if constexpr (anchoring::unanchored_call<Out, In>)
        return anchoring::via_anchored<Out>([]<insidable O>(auto v) { return pow_base_into<O, Base>(v); }, x);
    else {
        using core = engine::pow_core<2,
                                      engine::input_limbs<In>,
                                      engine::in_mag<In>,
                                      engine::pow_base_out_kmax<Out, In, Base>,
                                      66,
                                      engine::input_bits<In>,
                                      Base>;
        BEMAN_INSIDE_ENGINE_TIERS(
            Out,
            (engine::fp_tier<Out, engine::fp_pow_base, In> && engine::in_max<In> <= 1000),
            (engine::dd_tier<Out, In> && engine::in_max<In> <= 700),
            (engine::fp_attempt_pow_base<Out, Base>(x, r)),
            (engine::dd_attempt_pow_base<Out, Base>(x, r)),
            engine::evaluate<Out, engine::start_bits<Out>>(core{engine::exact_of<2>(Base), engine::exact_input(x)}))
    }
}

//---------------------------------------------------------------------------
// Auto forms: Out deduced from In — the range the function takes over In,
// rounded outward to In's notch, with In's notch and policy (rounding to
// nearest). sin and cos give [-1, 1], tan [-1024, 1024] (closer to a pole
// the expected reports overflow), atan2 [-π, π] on the gcd of both notches.
//---------------------------------------------------------------------------
template <insidable In>
consteval bool deducible() noexcept {
    static_assert(has_flag(flags_of<In>, snap),
                  "beman::inside::math: a deduced result is rounded onto the input's grid - its operand "
                  "must permit rounding (declare it with round_nearest, round_floor, ...)");
    static_assert(::beman::inside::detail::notched<In>,
                  "beman::inside::math: a deduced output takes the input's notch - the input needs one");
    return true;
}

namespace results {
template <insidable In>
using exp =
    engine::increasing_t<In, engine::exp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::exp_kmax<In>>>;
template <insidable In>
using exp2 =
    engine::increasing_t<In, engine::exp2_core<engine::input_limbs<In>, engine::in_mag<In>, engine::exp_kmax<In>>>;
template <insidable In>
using log = engine::increasing_t<In, engine::log_core<engine::input_limbs<In>, engine::input_bits<In>>>;
template <insidable In>
using log2 = engine::increasing_t<In, engine::logb_core<engine::input_limbs<In>, 2, engine::input_bits<In>>>;
template <insidable In>
using log10 = engine::increasing_t<In, engine::logb_core<engine::input_limbs<In>, 10, engine::input_bits<In>>>;
template <insidable In>
using sqrt = engine::increasing_t<In, engine::sqrt_core<engine::input_limbs<In>, engine::input_bits<In>>>;
template <insidable In>
using cbrt = engine::increasing_t<In, engine::cbrt_core<engine::input_limbs<In>, engine::input_bits<In>>>;
template <insidable In>
using atan = engine::increasing_t<In, engine::atan_core<engine::input_limbs<In>>>;
template <insidable In>
using asin = engine::increasing_t<In, engine::asin_core<engine::input_limbs<In>, engine::input_bits<In>>>;
template <insidable In>
using acos = engine::decreasing_t<In, engine::acos_core<engine::input_limbs<In>, engine::input_bits<In>>>;
template <insidable In>
using sinh = engine::increasing_t<
    In,
    engine::hyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::hyp::sinh, engine::exp_kmax<In>>>;
template <insidable In>
using tanh =
    engine::increasing_t<In, engine::hyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::hyp::tanh, 1>>;
template <insidable In>
using asinh = engine::increasing_t<
    In,
    engine::ahyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::ahyp::asinh, engine::input_bits<In>>>;
template <insidable In>
using acosh = engine::increasing_t<
    In,
    engine::ahyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::ahyp::acosh, engine::input_bits<In>>>;
template <insidable In>
using atanh = engine::increasing_t<
    In,
    engine::ahyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::ahyp::atanh, engine::input_bits<In>>>;
template <insidable In>
using sin = inside<{{-1, 1}, notch_of<In>}, engine::result_policy<In>>;
template <insidable In>
using cos = sin<In>;
template <insidable In>
using tan = inside<{{-1024, 1024}, notch_of<In>}, engine::result_policy<In>>;

// cosh is even: its least value is 1 when In spans 0, else at the end
// nearer 0.
template <insidable In>
using cosh_core =
    engine::hyp_core<engine::input_limbs<In>, engine::in_mag<In>, engine::hyp::cosh, engine::exp_kmax<In>>;
template <insidable In>
inline constexpr grid_rational cosh_lo = (lower_of<In> <= 0 && upper_of<In> >= 0) ? grid_rational{1}
                                         : (lower_of<In> > 0) ? engine::bound_at<In, cosh_core<In>, false, false>
                                                              : engine::bound_at<In, cosh_core<In>, true, false>;
template <insidable In>
inline constexpr grid_rational cosh_hi =
    std::max(engine::bound_at<In, cosh_core<In>, true, true>, engine::bound_at<In, cosh_core<In>, false, true>);
template <insidable In>
using cosh = engine::result_grid_t<In, cosh_lo<In>, cosh_hi<In>>;

// sqrt of a mixed-sign input: [0, √max(|Lower|, |Upper|)].
template <insidable In>
inline constexpr grid_rational max_abs = std::max(upper_of<In>, -lower_of<In>);
template <insidable In>
using sqrt_signed = engine::result_grid_t<In,
                                          grid_rational{0},
                                          engine::lattice_bound<notch_of<In>, true>(
                                              engine::sqrt_core<engine::input_limbs<In>, engine::input_bits<In>>{
                                                  engine::grid_input<In>(max_abs<In>)})>;

// pow_base: pow_core with the base bound.
template <imax Base, std::size_t E, int Mag, int KMax>
struct pow_base_core : engine::pow_core<2, E, Mag, KMax> {
    constexpr pow_base_core(::beman::inside::detail::wide_rational<E> x)
        : engine::pow_core<2, E, Mag, KMax>{engine::exact_of<2>(Base), x} {}
};
template <imax Base, insidable In>
using pow_base_t = engine::increasing_t<
    In,
    pow_base_core<Base,
                  engine::input_limbs<In>,
                  engine::in_mag<In>,
                  static_cast<int>(engine::max_abs_int<In>) * std::bit_width(static_cast<umax>(Base)) + 2>>;

// atan2: [−π, π] rounded outward on the gcd notch (π = atan2(0, −1)).
template <insidable A, insidable B>
inline constexpr grid_rational pi_up = engine::lattice_bound<engine::gcd_notch<A, B>, true>(
    engine::atan2_core<1>{engine::exact_of<1>(0), engine::exact_of<1>(-1)});
template <insidable InY, insidable InX>
using atan2 = inside<{{-pi_up<InY, InX>, pi_up<InY, InX>}, engine::gcd_notch<InY, InX>}, engine::result_policy<InY>>;

// hypot: [0, hypot of the largest magnitudes] on the gcd notch.
template <insidable InX, insidable InY>
inline constexpr std::size_t hypot_limbs = ::beman::inside::detail::limbs_for_bits(engine::hypot_bits<InX, InY>);
template <insidable InX, insidable InY>
inline constexpr grid_rational hypot_hi = [] {
    using F   = ::beman::inside::detail::wide_rational<hypot_limbs<InX, InY>>;
    const F a = ::beman::inside::detail::exact_of_grid<hypot_limbs<InX, InY>>(max_abs<InX>);
    const F b = ::beman::inside::detail::exact_of_grid<hypot_limbs<InX, InY>>(max_abs<InY>);
    return engine::lattice_bound<engine::gcd_notch<InX, InY>, true>(
        engine::sqrt_core<hypot_limbs<InX, InY>, engine::hypot_bits<InX, InY>>{a * a + b * b});
}();
template <insidable InX, insidable InY>
using hypot = inside<{{0, hypot_hi<InX, InY>}, engine::gcd_notch<InX, InY>}, engine::result_policy<InX>>;

// pow: b^e is monotone in each argument for b > 0, so the extremes are at
// the corners of the input rectangle. Notch of the base.
template <insidable InB, insidable InE>
using pow_core = engine::pow_core<engine::input_limbs<InB>,
                                  engine::input_limbs<InE>,
                                  engine::in_mag<InE>,
                                  engine::pow_kmax<InB, InE>,
                                  engine::input_bits<InB>,
                                  engine::input_bits<InE>>;
template <insidable InB, insidable InE, bool BUp, bool EUp, bool Up>
inline constexpr grid_rational pow_corner = engine::lattice_bound<notch_of<InB>, Up>(
    pow_core<InB, InE>{engine::grid_input<InB>(BUp ? upper_of<InB> : lower_of<InB>),
                       engine::grid_input<InE>(EUp ? upper_of<InE> : lower_of<InE>)});
// The least (Up: greatest) bound over the four corners.
template <insidable InB, insidable InE, bool Up>
inline constexpr grid_rational pow_extreme = [] {
    const std::initializer_list<grid_rational> c{pow_corner<InB, InE, false, false, Up>,
                                                 pow_corner<InB, InE, false, true, Up>,
                                                 pow_corner<InB, InE, true, false, Up>,
                                                 pow_corner<InB, InE, true, true, Up>};
    return Up ? std::max(c) : std::min(c);
}();
template <insidable InB, insidable InE>
using pow =
    inside<{{pow_extreme<InB, InE, false>, pow_extreme<InB, InE, true>}, notch_of<InB>}, engine::result_policy<InB>>;
} // namespace results

#define BEMAN_INSIDE_ENGINE_AUTO(fn)                                                           \
    template <insidable In>                                                                    \
    [[nodiscard]] constexpr auto fn(In x) {                                                    \
        static_assert(deducible<In>());                                                        \
        if constexpr (fn##_domain<In>)                                                         \
            return fn##_into<results::fn<In>>(x);                                              \
        else                                                                                   \
            return fn##_into<inside<{0, 1}, round_nearest>>(x); /* the _into domain message */ \
    }

BEMAN_INSIDE_ENGINE_AUTO(exp)
BEMAN_INSIDE_ENGINE_AUTO(exp2)
BEMAN_INSIDE_ENGINE_AUTO(sin)
BEMAN_INSIDE_ENGINE_AUTO(cos)
BEMAN_INSIDE_ENGINE_AUTO(tan)
BEMAN_INSIDE_ENGINE_AUTO(atan)
BEMAN_INSIDE_ENGINE_AUTO(sinh)
BEMAN_INSIDE_ENGINE_AUTO(cosh)
BEMAN_INSIDE_ENGINE_AUTO(tanh)
BEMAN_INSIDE_ENGINE_AUTO(asinh)
BEMAN_INSIDE_ENGINE_AUTO(cbrt)
BEMAN_INSIDE_ENGINE_AUTO(log)
BEMAN_INSIDE_ENGINE_AUTO(log2)
BEMAN_INSIDE_ENGINE_AUTO(log10)
BEMAN_INSIDE_ENGINE_AUTO(asin)
BEMAN_INSIDE_ENGINE_AUTO(acos)
BEMAN_INSIDE_ENGINE_AUTO(acosh)
BEMAN_INSIDE_ENGINE_AUTO(atanh)
#undef BEMAN_INSIDE_ENGINE_AUTO

template <insidable In>
    requires(lower_of<In> >= 0)
[[nodiscard]] constexpr auto sqrt(In x) {
    static_assert(deducible<In>());
    return sqrt_into<results::sqrt<In>>(x);
}

template <insidable In>
    requires(lower_of<In> < 0)
[[nodiscard]] constexpr auto sqrt(In x) {
    static_assert(deducible<In>());
    return sqrt_into<results::sqrt_signed<In>>(x);
}

template <imax Base, insidable In>
[[nodiscard]] constexpr auto pow_base(In x) {
    static_assert(deducible<In>());
    static_assert(Base >= 2, "beman::inside::math::pow_base: Base must be at least 2");
    return pow_base_into<results::pow_base_t<Base, In>, Base>(x);
}

template <insidable InY, insidable InX>
[[nodiscard]] constexpr auto atan2(InY y, InX x) {
    static_assert(deducible<InY>() && deducible<InX>());
    return atan2_into<results::atan2<InY, InX>>(y, x);
}

template <insidable InX, insidable InY>
[[nodiscard]] constexpr auto hypot(InX x, InY y) {
    static_assert(deducible<InX>() && deducible<InY>());
    return hypot_into<results::hypot<InX, InY>>(x, y);
}

template <insidable InB, insidable InE>
    requires(lower_of<InB> > 0)
[[nodiscard]] constexpr auto pow(InB base, InE exp) {
    static_assert(deducible<InB>() && deducible<InE>());
    return pow_into<results::pow<InB, InE>>(base, exp);
}

//---------------------------------------------------------------------------
// fn<Mode>(x): the deduced form with the rounding mode named on the call, so
// the input's type need not carry one. The input is read as the same grid
// with Mode as its rounding (the same raw), the output deduced as for fn(x),
// and that output rounds by exactly Mode.
//---------------------------------------------------------------------------
inline constexpr policy_flag rounding_bits = round_nearest | round_floor | round_ceil | round_half_even; // snap too

template <policy_flag Mode>
concept rounding_mode = Mode == round_toward_zero || Mode == round_nearest || Mode == round_floor ||
                        Mode == round_ceil || Mode == round_half_even;

template <policy_flag Mode, insidable In>
[[nodiscard]] constexpr auto with_mode(const In& x) noexcept {
    return inside<grid_of<In>, (flags_of<In> & ~(::beman::inside::detail::cursor_marker | rounding_bits)) | Mode>::
        from_raw(x.raw());
}

// The deduced output D (an inside, or an expected of one) rounding by Mode.
template <class D, policy_flag Mode>
using moded_t = inside<grid_of<::beman::inside::detail::unwrap_t<D>>,
                       (flags_of<::beman::inside::detail::unwrap_t<D>> & ~rounding_bits) | Mode>;

#define BEMAN_INSIDE_ENGINE_MODED(fn)                        \
    template <policy_flag Mode, insidable In>                \
        requires rounding_mode<Mode>                         \
    [[nodiscard]] constexpr auto fn(In x) {                  \
        const auto v = with_mode<Mode>(x);                   \
        return fn##_into<moded_t<decltype(fn(v)), Mode>>(v); \
    }
BEMAN_INSIDE_ENGINE_MODED(exp)
BEMAN_INSIDE_ENGINE_MODED(exp2)
BEMAN_INSIDE_ENGINE_MODED(sin)
BEMAN_INSIDE_ENGINE_MODED(cos)
BEMAN_INSIDE_ENGINE_MODED(tan)
BEMAN_INSIDE_ENGINE_MODED(atan)
BEMAN_INSIDE_ENGINE_MODED(sinh)
BEMAN_INSIDE_ENGINE_MODED(cosh)
BEMAN_INSIDE_ENGINE_MODED(tanh)
BEMAN_INSIDE_ENGINE_MODED(asinh)
BEMAN_INSIDE_ENGINE_MODED(cbrt)
BEMAN_INSIDE_ENGINE_MODED(log)
BEMAN_INSIDE_ENGINE_MODED(log2)
BEMAN_INSIDE_ENGINE_MODED(log10)
BEMAN_INSIDE_ENGINE_MODED(asin)
BEMAN_INSIDE_ENGINE_MODED(acos)
BEMAN_INSIDE_ENGINE_MODED(acosh)
BEMAN_INSIDE_ENGINE_MODED(atanh)
BEMAN_INSIDE_ENGINE_MODED(sqrt)
#undef BEMAN_INSIDE_ENGINE_MODED

template <imax Base, policy_flag Mode, insidable In>
    requires rounding_mode<Mode>
[[nodiscard]] constexpr auto pow_base(In x) {
    const auto v = with_mode<Mode>(x);
    return pow_base_into<moded_t<decltype(pow_base<Base>(v)), Mode>, Base>(v);
}

template <policy_flag Mode, insidable InY, insidable InX>
    requires rounding_mode<Mode>
[[nodiscard]] constexpr auto atan2(InY y, InX x) {
    const auto vy = with_mode<Mode>(y);
    const auto vx = with_mode<Mode>(x);
    return atan2_into<moded_t<decltype(atan2(vy, vx)), Mode>>(vy, vx);
}

template <policy_flag Mode, insidable InX, insidable InY>
    requires rounding_mode<Mode>
[[nodiscard]] constexpr auto hypot(InX x, InY y) {
    const auto vx = with_mode<Mode>(x);
    const auto vy = with_mode<Mode>(y);
    return hypot_into<moded_t<decltype(hypot(vx, vy)), Mode>>(vx, vy);
}

template <policy_flag Mode, insidable InB, insidable InE>
    requires rounding_mode<Mode>
[[nodiscard]] constexpr auto pow(InB base, InE exp) {
    const auto vb = with_mode<Mode>(base);
    const auto ve = with_mode<Mode>(exp);
    return pow_into<moded_t<decltype(pow(vb, ve)), Mode>>(vb, ve);
}
} // namespace beman::inside::math::adaptive

#undef BEMAN_INSIDE_ENGINE_TIERS
#undef BEMAN_INSIDE_ENGINE_KERNEL_TIERS
#undef BEMAN_INSIDE_ENGINE_TABLE

#endif // BEMAN_INSIDE_MATH_DETAIL_ENTRY_HPP
