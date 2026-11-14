// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_MATH_DETAIL_FUNCTIONS_HPP
#define BEMAN_INSIDE_MATH_DETAIL_FUNCTIONS_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside.hpp>

#include <algorithm>
#include <initializer_list>
#include <type_traits>

//---------------------------------------------------------------------------
// The one call shape of every math function (math.hpp, math/integer.hpp):
//   f(x…)             the output deduced: the function's range over the
//                     arguments' types, on their notch, with their policy
//   f<Spec…>(x…)      the same with the notch (per<K>) and/or policy flags
//                     (round_floor, clamp, …) named, in any order
//   f<C>(x…)          onto C: the whole result type, or only its number (a
//                     quantity's unit, a multivector's blades deduced)
//   f<C, D…>(x…)      the arguments converted to D… first
// A function is a tag F:
//   takes<In…>        whether it takes these insides (its arity, its
//                     constraints)
//   rounds            whether its value rounds onto the output (the engine)
//                     or is exact (the grid operations, the counts)
//   result<In…>       an exact function's output, deduced
//   result_on<N, In…> a rounding function's output on notch N, and
//   notch<In…>        the notch it takes by default, and domain<In…>
//   eval<O>(x…)       the value onto the output type O
// The kinds of arguments map a call to the tag: insides here, quantities
// (units/math.hpp), multivectors (units/detail/geoalg_exp.hpp).
//---------------------------------------------------------------------------
namespace beman::inside::math::detail {
using namespace beman::inside::detail;

//---------------------------------------------------------------------------
// Output specs: a notch (a rational or a grid number: per<K>, 0.5_r, 1e-30_g)
// and policy flags, as template arguments of the call.
//---------------------------------------------------------------------------
struct out_spec {
    bool          HasNotch = false;
    grid_rational Notch{0};
    policy_flag   Flags = 0;
};

template <auto S>
inline constexpr bool notch_spec = std::same_as<decltype(S), rational> || std::same_as<decltype(S), grid_rational>;
template <auto S>
inline constexpr bool flag_spec = flag_like<decltype(S)> || std::same_as<decltype(S), policy_flag>;

template <auto S>
consteval out_spec add_spec(out_spec s) {
    if constexpr (notch_spec<S>) {
        s.HasNotch = true;
        s.Notch    = grid_rational{S};
    } else
        s.Flags |= static_cast<policy_flag>(S);
    return s;
}

template <auto... Spec>
consteval out_spec spec_of() {
    static_assert(((notch_spec<Spec> || flag_spec<Spec>) && ...),
                  "beman::inside::math: a template argument of f<...>(x) names the output's notch (per<K>) or "
                  "policy (round_floor, clamp, ...), or is the output type itself: f<C>(x)");
    static_assert((0 + ... + int{notch_spec<Spec>}) <= 1, "beman::inside::math: f<...>(x) takes one notch");
    out_spec s{};
    ((s = add_spec<Spec>(s)), ...);
    return s;
}

// What a call names: specs (nothing, for f(x)), or the output C.
template <out_spec S>
struct specs {};
template <class C>
struct named {};

// R's policy under the specs: their rounding and range handling replace R's
// (override_flags), and once anything is named the output rounds (by R's
// mode, else to nearest, unless the specs name one: round_toward_zero too).
consteval policy_flag spec_flags(policy_flag r, const out_spec& s) {
    if (!s.HasNotch && s.Flags == 0)
        return r;
    const policy_flag f = override_flags(r, s.Flags);
    return (s.Flags & (mode_bits | snap)) != 0 ? f : with_default_rounding(f);
}
template <insidable R, out_spec S>
using with_spec_flags_t = inside<grid_of<R>, spec_flags(flags_of<R>, S)>;

// An exact output R on notch N: its range rounded outward onto N.
template <insidable R, grid_rational N>
using on_notch_t = inside<
    grid{{grid_round<round_mode::floor>(lower_of<R> / N) * N, grid_round<round_mode::ceil>(upper_of<R> / N) * N}, N},
    flags_of<R>>;

// A rounding function's deduced output over insides In… for specs S: on the
// named notch, else on its own (the arguments' notch, which they need then).
// Outside the domain, a stand-in: eval states the domain.
template <class F, out_spec S, class... In>
consteval auto rounding_output() {
    if constexpr (!F::template domain<In...>)
        return std::type_identity<inside<{0, 1}, round_nearest>>{};
    else if constexpr (S.HasNotch)
        return std::type_identity<typename F::template result_on<S.Notch, In...>>{};
    else {
        static_assert(F::template notch<In...> != 0,
                      "beman::inside::math: a deduced output takes the input's notch - the input needs one, or "
                      "name it: f<per<K>>(x)");
        return std::type_identity<typename F::template result_on<F::template notch<In...>, In...>>{};
    }
}
template <class F, out_spec S, class... In>
consteval auto number_output() {
    if constexpr (F::rounds)
        return std::type_identity<with_spec_flags_t<typename decltype(rounding_output<F, S, In...>())::type, S>>{};
    else if constexpr (S.HasNotch)
        return std::type_identity<with_spec_flags_t<on_notch_t<typename F::template result<In...>, S.Notch>, S>>{};
    else
        return std::type_identity<with_spec_flags_t<typename F::template result<In...>, S>>{};
}
// F's output over insides In… for specs S.
template <class F, out_spec S, class... In>
using number_output_t = typename decltype(number_output<F, S, In...>())::type;

//---------------------------------------------------------------------------
// Kinds of arguments. An argument's rank: 1 an inside, 2 a quantity, 3 a
// multivector (the units headers add theirs); a call's is its highest, 0
// when an argument is none of them.
//---------------------------------------------------------------------------
template <class T>
inline constexpr int kind_rank = insidable<T> ? 1 : 0;

template <class... Args>
inline constexpr int rank_of = [] {
    int r = 0;
    for (const int k : {0, kind_rank<Args>...}) // (the leading 0: no arguments)
        r = std::max(r, k);
    return ((kind_rank<Args> == 0) || ...) ? 0 : r;
}();

// How F applies to arguments of one rank: takes (whether it does),
// output<S> (the output for specs S) and eval<O>(x…). The primary takes
// nothing.
template <class F, int Rank, class... Args>
struct kind {
    static constexpr bool takes = false;
};

// Insides: the tag itself.
template <class F, class... In>
struct kind<F, 1, In...> {
    static constexpr bool takes = F::template takes<In...>;

    template <class S>
    struct out;
    template <out_spec S>
    struct out<specs<S>> {
        using type = number_output_t<F, S, In...>;
    };
    template <class C>
    struct out<named<C>> {
        static_assert(insidable<C>, "beman::inside::math: f<C>(x) of insides - C is an inside");
        using type = C;
    };
    template <class S>
    using output = typename out<S>::type;

    template <class O>
    [[gnu::always_inline]] static constexpr auto eval(const In&... x) {
        return F::template eval<O>(x...);
    }
    template <class O>
    [[gnu::always_inline]] static constexpr auto eval_named(const In&... x);
};

template <class F, class... Args>
using kind_of = kind<F, rank_of<Args...>, Args...>;

template <class F, class... Args>
concept takes = kind_of<F, Args...>::takes;

//---------------------------------------------------------------------------
// A named output f<C>(x): C itself where C's range holds every value f takes
// over the input grids (or C clamps or wraps), else std::expected<C, errc>.
// The value is computed once onto a twin of C - C's notch and lattice, an
// interval that holds every value - then stored into C exactly, so the
// failure is only the range, and the rounding stays one.
//---------------------------------------------------------------------------
template <class>
inline constexpr bool is_named_v = false;
template <class C>
inline constexpr bool is_named_v<named<C>> = true;

// The range of f over In… rounded onto O's notch (1/MaxDenominator for a
// continuous O).
template <class F, class O, class... In>
using named_range_t =
    typename F::template result_on<notch_of<O> != 0 ? notch_of<O> : grid_rational{1} / grid_of<O>.MaxDenominator,
                                   In...>;

// Whether the library bounds f over In… (entry.hpp specializes it false for
// grids where a deduced output would not exist: exp of |x| past 4096, pow of
// a base grid that reaches 0, …). A named output of an unbounded f needs
// clamp or wrap.
template <class F, class... In>
inline constexpr bool range_bounded = true;

// (Outside f's domain the call goes straight to f, whose diagnostic names it.)
template <class F, class... In>
consteval bool in_domain() {
    if constexpr (requires { F::template domain<In...>; })
        return F::template domain<In...>;
    else
        return true;
}

template <class F, class O, class... In>
consteval bool named_fits() {
    if constexpr (!is_checked(flags_of<O>) || has_any_flag(flags_of<O>, clamp | wrap) || !in_domain<F, In...>())
        return true;
    else if constexpr (!range_bounded<F, In...>)
        return false;
    else if constexpr (!F::rounds)
        return store_proven<O, typename F::template result<In...>>;
    else
        return includes(interval_of<O>, interval_of<named_range_t<F, O, In...>>);
}

template <class F, class O, class... In>
consteval grid named_twin_grid() {
    using R                = named_range_t<F, O, In...>;
    const grid_rational lo = lower_of<O> < lower_of<R> ? lower_of<O> : lower_of<R>;
    const grid_rational hi = upper_of<R> < upper_of<O> ? upper_of<O> : upper_of<R>;
    if constexpr (notch_of<O> == 0)
        return grid{interval{lo, hi}, max_denominator{grid_of<O>.MaxDenominator}};
    else {
        const grid_rational n = notch_of<O>; // whole notches out from O's limits
        return grid{interval{lower_of<O> - grid_round<round_mode::ceil>((lower_of<O> - lo) / n) * n,
                             upper_of<O> + grid_round<round_mode::ceil>((hi - upper_of<O>) / n) * n},
                    n};
    }
}

template <class F, class O, class... In>
consteval auto named_twin() {
    if constexpr (F::rounds)
        return std::type_identity<inside<named_twin_grid<F, O, In...>(), flags_of<O>>>{};
    else
        return std::type_identity<typename F::template result<In...>>{};
}
template <class F, class O, class... In>
using named_twin_t = typename decltype(named_twin<F, O, In...>())::type;

template <class F, class O, class... In>
[[gnu::always_inline]] constexpr auto eval_named(const In&... x) {
    if constexpr (named_fits<F, O, In...>())
        return F::template eval<O>(x...);
    else if constexpr (!range_bounded<F, In...>)
        static_assert(false,
                      "beman::inside::math: f<C>(x) - the library bounds no range of f over these input grids, so "
                      "no grid proves C or checks it: give C clamp or wrap, or narrow the input grids");
    else
        return O::try_make(F::template eval<named_twin_t<F, O, In...>>(x...));
}

template <class F, class... In>
template <class O>
constexpr auto kind<F, 1, In...>::eval_named(const In&... x) {
    return detail::eval_named<F, O, In...>(x...);
}

// The call: F's value onto the output its kind makes of S.
template <class F, class S, class... Args>
[[nodiscard]] [[gnu::always_inline]] constexpr auto call(const Args&... x) {
    using K = kind_of<F, Args...>;
    if constexpr (is_named_v<S> && requires { K::template eval_named<typename K::template output<S>>(x...); })
        return K::template eval_named<typename K::template output<S>>(x...);
    else
        return K::template eval<typename K::template output<S>>(x...);
}
} // namespace beman::inside::math::detail

// The public templates of a math function `name` with tag `Tag`: f(x…),
// f<Spec…>(x…), and f<C>(x…), f<C, D…>(x…). The value pack of the first
// takes no type, so f<T>(x) always names the output. An optional leading
// template parameter (pow_base's base, pown's exponent) comes before them
// all: f<P>(x), f<P, Spec…>(x), f<P, C>(x).
#define BEMAN_INSIDE_MATH_FUNCTION(name, Tag, ...)                                                              \
    template <__VA_ARGS__ __VA_OPT__(, ) auto... Spec, class... In>                                             \
        requires ::beman::inside::math::detail::takes<Tag, In...>                                               \
    [[nodiscard]] [[gnu::always_inline]] constexpr auto name(const In&... x) {                                  \
        using specs_ = ::beman::inside::math::detail::specs<::beman::inside::math::detail::spec_of<Spec...>()>; \
        return ::beman::inside::math::detail::call<Tag, specs_>(x...);                                          \
    }                                                                                                           \
    template <__VA_ARGS__ __VA_OPT__(, ) class C, class... D>                                                   \
        requires ::beman::inside::math::detail::takes<Tag, D...>                                                \
    [[nodiscard]] [[gnu::always_inline]] constexpr auto name(const D&... x) {                                   \
        return ::beman::inside::math::detail::call<Tag, ::beman::inside::math::detail::named<C>>(x...);         \
    }

#endif // BEMAN_INSIDE_MATH_DETAIL_FUNCTIONS_HPP
