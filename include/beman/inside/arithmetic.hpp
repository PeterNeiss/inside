// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_ARITHMETIC_HPP
#define BEMAN_INSIDE_ARITHMETIC_HPP

#include <beman/inside/detail/config.hpp>

#include <beman/inside/detail/compare.hpp> // the inside type and its comparisons
#include <beman/inside/casts.hpp>
#include <beman/inside/detail/rep.hpp>

#include <algorithm>
#include <ranges>
#include <type_traits> // std::common_type

//---------------------------------------------------------------------------
// Free-function arithmetic over detail::addition/multiplication/division/
// modulo:
//   add, sub, mul, div, mod, divmod   with a policy, hooks or an errc&
//   mul_into<Out>, div_into<Out>      the exact result, rounded once into Out
//   add_all, mul_all, sum<Target>     folds; sum checks the total once
//   min, max, midpoint                ADL-found, exact on mixed grids
//   dot, cross, lerp                  2-D helpers that widen like + and *
//   common_inside_t, std::common_type the hull of two grids
// and the operators, including the std::expected-lift overloads (an error
// operand passes through; detail::lift_operand_v names the operand types).
//---------------------------------------------------------------------------
namespace beman::inside {
//---------------------------------------------------------------------------
// add / sub / mul / div / mod / divmod — each takes one of three trailing forms:
//   op(l, r [, policy [, action]])   explicit per-call policy (+ action)
//   op(l, r, on_overflow(λ), ...)    tagged actions (policy = their implied flags)
//   op(l, r, ec [, action])          error-code form (checked, reports into ec)
// detail::arith normalizes the form to (policy, action) for the op's core.
//---------------------------------------------------------------------------
#define BEMAN_INSIDE_WHY_arith_action                                                                               \
    "add/sub/mul/div/mod fire only on_overflow; on_clamp / on_wrap / on_error apply to assignment — use them with " \
    "b.with(...) or a cast"

namespace detail {
// Arithmetic fires only on_overflow; any other action would be silently
// ignored, so it is rejected.
template <class A>
inline constexpr bool arith_accepts_action =
    std::same_as<std::remove_cvref_t<A>, no_action> || overflow_action<std::remove_cvref_t<A>>;

template <class Op, class L, class R, policy_like P = policy<>, class A = no_action>
constexpr auto arith(Op op, const L& l, const R& r, P&& pol = {}, A&& act = {}) {
    static_assert(arith_accepts_action<A>, BEMAN_INSIDE_WHY_arith_action);
    return op(l, r, std::forward<P>(pol), std::forward<A>(act));
}

// Action-first form: on_overflow actions (the only kind arithmetic fires;
// any other tag is rejected below with a message, not a bare mismatch).
template <class A>
inline constexpr bool is_action_tag_v =
    is_overflow_action<A>::value || is_clamp_action<A>::value || is_wrap_action<A>::value || is_error_action<A>::value;

template <class Op, class L, class R, class... Actions>
    requires(sizeof...(Actions) >= 1) && (is_action_tag_v<std::remove_cvref_t<Actions>> && ...)
constexpr auto arith(Op op, const L& l, const R& r, Actions&&... acts) {
    static_assert((overflow_action<std::remove_cvref_t<Actions>> && ...), BEMAN_INSIDE_WHY_arith_action);
    return op(l, r, make_policy<merged_implied_flags<Actions...>>(), pick_action<is_overflow_action>(acts...));
}

template <class Op, class L, class R, class A = no_action>
constexpr auto arith(Op op, const L& l, const R& r, errc& ec, A&& act = {}) {
    static_assert(arith_accepts_action<A>, BEMAN_INSIDE_WHY_arith_action);
    return op(l, r, make_policy<checked>(ec), std::forward<A>(act));
}

struct add_op {
    template <class L, class R, class P, class A>
    constexpr auto operator()(const L& l, const R& r, P&& p, A&& a) const {
        return addition<L, R>::add(l, r, std::forward<P>(p), std::forward<A>(a));
    }
};
struct sub_op {
    template <class L, class R, class P, class A>
    constexpr auto operator()(const L& l, const R& r, P&& p, A&& a) const {
        return add_op{}(l, -r, std::forward<P>(p), std::forward<A>(a));
    }
};
struct mul_op {
    template <class L, class R, class P, class A>
    constexpr auto operator()(const L& l, const R& r, P&& p, A&& a) const {
        return multiplication<L, R>::mul(l, r, std::forward<P>(p), std::forward<A>(a));
    }
};
struct div_op {
    template <class L, class R, class P, class A>
    constexpr auto operator()(const L& l, const R& r, P&& p, A&& a) const {
        return division<L, R, flags_of<P>>::div(l, r, p, std::forward<A>(a));
    }
};
struct mod_op {
    template <class L, class R, class P, class A>
    constexpr auto operator()(const L& l, const R& r, P&& p, A&& a) const {
        return modulo<L, R, flags_of<P>>::mod(l, r, p, std::forward<A>(a));
    }
};
struct divmod_op {
    template <class L, class R, class P, class A>
    constexpr auto operator()(const L& l, const R& r, P&& p, A&& a) const {
        return quotient_remainder<L, R, flags_of<P>>::divmod(l, r, p, std::forward<A>(a));
    }
};
} // namespace detail

// A deleted overload takes each op's failing case: `requires { a + b; }` is
// then false, and an integer inside cannot fall back on its imax conversion
// and the built-in operator.

#define BEMAN_INSIDE_WHY_add "addition: a wide-index operand meets a continuous result (not supported yet)"
#define BEMAN_INSIDE_WHY_sub "subtraction: a wide-index operand meets a continuous result (not supported yet)"
#define BEMAN_INSIDE_WHY_mul                                                                                       \
    "multiplication: a wide-index operand meets a continuous result (not supported yet) - round the product into " \
    "a coarser type with mul_into<Out>(a, b)"
#define BEMAN_INSIDE_WHY_div \
    "division: no result grid - the divisor grid is the point 0, or an unbounded continuous divisor reaches 0"
#define BEMAN_INSIDE_WHY_mod "modulo requires integer-valued grids"
#define BEMAN_INSIDE_WHY_divmod "divmod requires integer-valued grids"

namespace detail {
// The policy flags a call's trailing arguments select, as arith normalizes
// them: a policy's own, checked for an errc&, an action's implied flags.
template <class... Args>
inline constexpr policy_flag call_flags = [] {
    if constexpr (sizeof...(Args) == 0)
        return none;
    else if constexpr (policy_like<std::remove_cvref_t<Args...[0]>>)
        return flags_of<Args...[0]>;
    else if constexpr (std::same_as<Args...[0], errc&>)
        return checked;
    else
        return merged_implied_flags<Args...>;
}();

// Each op's well-formedness under flags F is detail::add_defined & co.
// (detail/addition.hpp, multiplication.hpp, division.hpp): the first
// constraint of its function and operator, so a requires-expression or
// `if constexpr` probe is false where the op cannot compile.
} // namespace detail

#define BEMAN_INSIDE_ARITH_FN(name, op)                                             \
    template <insidable L, insidable R, class... Args>                              \
        requires detail::name                                                       \
    ##_defined<L, R, detail::call_flags<Args...>>&&                                 \
        requires(L const& l, R const& r, Args && ... args)                          \
    {                                                                               \
        detail::arith(detail::op{}, l, r, std::forward<Args>(args)...);             \
    }                                                                               \
    [[nodiscard]] constexpr auto name(L const& lhs, R const& rhs, Args&&... args) { \
        return detail::arith(detail::op{}, lhs, rhs, std::forward<Args>(args)...);  \
    }                                                                               \
    template <insidable L, insidable R, class... Args>                              \
        requires(!detail::name##_defined<L, R, detail::call_flags<Args...>>)        \
    auto name(L const&, R const&, Args&&...) = delete (BEMAN_INSIDE_WHY_##name);

BEMAN_INSIDE_ARITH_FN(add, add_op)
BEMAN_INSIDE_ARITH_FN(sub, sub_op)
BEMAN_INSIDE_ARITH_FN(mul, mul_op)
BEMAN_INSIDE_ARITH_FN(div, div_op)
BEMAN_INSIDE_ARITH_FN(mod, mod_op)
BEMAN_INSIDE_ARITH_FN(divmod, divmod_op)
#undef BEMAN_INSIDE_ARITH_FN

//---------------------------------------------------------------------------
// mul_into<Out> / div_into<Out> — the exact product or quotient, rounded once
// onto Out by Out's policy (plus the per-call flags F). Neither forms the
// product or quotient grid, so they work where `a * b` would meet a wide index
// with a continuous result, or a quotient past the 64-bit fraction. A store that
// fails is reported through the policy (it throws under `checked`).
// div_into returns std::expected<Out, errc> when the divisor's grid holds 0 —
// then every failure, a zero divisor or a failed store, is the error — else Out.
//---------------------------------------------------------------------------
namespace detail {
template <insidable Out, std::size_t K, typename P>
constexpr Out store_exact_into(const wide_rational<K>& v, P&& policy) {
    Out out{};
    assign_exact<rational>(out, v, policy, no_action{});
    return out;
}
// The same store from a value with 64-bit numbers: an inside or a rational.
template <insidable Out, numeric V, typename P>
constexpr Out store_into(const V& v, P&& policy) {
    Out out{};
    assignment<Out, V>::assign(out, v, policy, no_action{});
    return out;
}
// Native paths: the exact product as an inside when its grid has 64-bit raws,
// else the product or quotient as a 64-bit rational when it fits. Each rounds
// once, as the wide path does; past them the wide path is the fallback.
// An inside-to-inside store accepts the product only where its compile-time
// checks pass: notches that fit (or snap) and an interval that meets Out's (or
// clamp/wrap). The wide path reports those cases at run time instead.
template <insidable Out, policy_flag F, insidable A, insidable B>
inline constexpr bool native_product = [] {
    if constexpr (!mul_defined<A, B>)
        return false;
    else {
        using P                   = typename multiplication<A, B>::result;
        constexpr policy_flag all = flags_of<Out> | F;
        return values_fit_imax<P> && (notches_compatible<Out, P> || has_flag(all, snap)) &&
               (has_flag(all, clamp) || has_flag(all, wrap) || !excludes(interval_of<Out>, interval_of<P>));
    }
}();
template <insidable A, insidable B>
inline constexpr bool rational_operands = rational_readable<A> && rational_readable<B>;

template <insidable Out, insidable A, insidable B, typename P>
constexpr Out mul_into_wide(const A& a, const B& b, P&& policy) {
    using E = wide_rational<exact_limbs<A, B, Out>>;
    return store_exact_into<Out>(E{exact_of(a)} * E{exact_of(b)}, policy);
}
template <insidable Out, insidable A, insidable B, typename P>
constexpr Out div_into_wide(const A& a, const B& b, P&& policy) {
    using E = wide_rational<exact_limbs<A, B, Out>>;
    return store_exact_into<Out>(E{exact_of(a)} / E{exact_of(b)}, policy);
}
} // namespace detail

template <insidable Out, policy_flag F = none, insidable A, insidable B>
[[nodiscard]] constexpr Out mul_into(const A& a, const B& b) {
    auto policy = make_policy<flags_of<Out> | F>();
    if constexpr (detail::native_product<Out, F, A, B>)
        return detail::store_into<Out>(a * b, policy);
    else {
        if constexpr (detail::rational_operands<A, B>)
            if (const auto p = detail::try_mul(detail::as_rational(a), detail::as_rational(b)))
                return detail::store_into<Out>(*p, policy);
        return detail::mul_into_wide<Out>(a, b, policy);
    }
}

template <insidable Out, policy_flag F = none, insidable A, insidable B>
[[nodiscard]] constexpr auto div_into(const A& a, const B& b)
    -> std::conditional_t<detail::divisor_excludes_zero<B>, Out, std::expected<Out, errc>> {
    const auto quotient = [&](auto&& policy) {
        if constexpr (detail::rational_operands<A, B>)
            if (const auto q = detail::try_div(detail::as_rational(a), detail::as_rational(b)))
                return detail::store_into<Out>(*q, policy);
        return detail::div_into_wide<Out>(a, b, policy);
    };
    if constexpr (detail::divisor_excludes_zero<B>)
        return quotient(make_policy<flags_of<Out> | F>());
    else {
        if (b == 0)
            return std::unexpected{errc::division_by_zero};
        return detail::try_store<Out, flags_of<Out> | F>([&](Out& out, auto&& policy) { out = quotient(policy); });
    }
}

// Binary operators: +, -, * use the default policy; / and % carry the
// operands' own policies (snap/rounding select the native integer paths).
// Each is constrained on its op's well-formedness, like the named forms.
template <insidable L, insidable R>
    requires detail::add_defined<L, R>
[[nodiscard]] constexpr auto operator+(L lhs, R rhs) {
    return beman::inside::add(lhs, rhs);
}
template <insidable L, insidable R>
    requires(!detail::add_defined<L, R>)
auto operator+(L, R) = delete (BEMAN_INSIDE_WHY_add);

template <insidable L, insidable R>
    requires detail::sub_defined<L, R>
[[nodiscard]] constexpr auto operator-(L lhs, R rhs) {
    return beman::inside::sub(lhs, rhs);
}
template <insidable L, insidable R>
    requires(!detail::sub_defined<L, R>)
auto operator-(L, R) = delete (BEMAN_INSIDE_WHY_sub);

template <insidable L, insidable R>
    requires detail::mul_defined<L, R>
[[nodiscard]] constexpr auto operator*(L lhs, R rhs) {
    return beman::inside::mul(lhs, rhs);
}
template <insidable L, insidable R>
    requires(!detail::mul_defined<L, R>)
auto operator*(L, R) = delete (BEMAN_INSIDE_WHY_mul);

template <insidable L, insidable R>
    requires detail::div_defined<L, R, flags_of<L> | flags_of<R>>
[[nodiscard]] constexpr auto operator/(L lhs, R rhs) {
    return beman::inside::div(lhs, rhs, make_policy<flags_of<L> | flags_of<R>>());
}
template <insidable L, insidable R>
    requires(!detail::div_defined<L, R, flags_of<L> | flags_of<R>>)
auto operator/(L, R) = delete (BEMAN_INSIDE_WHY_div);

template <insidable L, insidable R>
    requires detail::mod_defined<L, R, flags_of<L> | flags_of<R>>
[[nodiscard]] constexpr auto operator%(L lhs, R rhs) {
    return beman::inside::mod(lhs, rhs, make_policy<flags_of<L> | flags_of<R>>());
}
template <insidable L, insidable R>
    requires(!detail::mod_defined<L, R, flags_of<L> | flags_of<R>>)
auto operator%(L, R) = delete (BEMAN_INSIDE_WHY_mod);
#undef BEMAN_INSIDE_WHY_arith_action
#undef BEMAN_INSIDE_WHY_add
#undef BEMAN_INSIDE_WHY_sub
#undef BEMAN_INSIDE_WHY_mul
#undef BEMAN_INSIDE_WHY_div
#undef BEMAN_INSIDE_WHY_mod
#undef BEMAN_INSIDE_WHY_divmod

//---------------------------------------------------------------------------
// add_all / mul_all — variadic folds (pairwise widening, same as `a + b + c`
// but reads cleaner; matches Chromium's `CheckAdd(a, b, c)`).
//---------------------------------------------------------------------------
template <insidable First, insidable... Rest>
[[nodiscard]] constexpr auto add_all(const First& first, const Rest&... rest) {
    return (first + ... + rest);
}

template <insidable First, insidable... Rest>
[[nodiscard]] constexpr auto mul_all(const First& first, const Rest&... rest) {
    return (first * ... * rest);
}

//---------------------------------------------------------------------------
// sum<Target> — bulk reduction with ONE deferred range check. Per-element
// `target += b` re-validates every step (blocks vectorization); this computes
// the exact total and stores it into Target once, by Target's policy
// (semantic difference: the *total* is validated, not every prefix).
// Notched integer raws (wide ones too) sum value indices exactly — Σvalue =
// Notch·ΣJ — in a wide integer with 64 bits of headroom for the count; ≤32-bit
// raws add in imax blocks of 2^30 elements, the loop that vectorizes.
// Continuous elements add as 64-bit
// rationals; a total past that reports overflow through Target's policy.
//---------------------------------------------------------------------------
template <insidable Target, std::ranges::input_range Rng>
    requires insidable<std::remove_cvref_t<std::ranges::range_reference_t<Rng>>>
[[nodiscard]] constexpr Target sum(Rng&& r) {
    using B = std::remove_cvref_t<std::ranges::range_reference_t<Rng>>;
    Target out{};
    auto   policy = make_policy<flags_of<Target>>();

    if constexpr (detail::integer_storage<B> && !detail::point_storage<B> && detail::notched<B>) {
        // The total in units of U, the value unit: the notch on an anchored
        // grid, so each value counts as its value index.
        constexpr detail::grid_rational U = grid_of<B>.value_unit();
        constexpr int bits = detail::signed_value_bits_of({detail::units_lo<B, U>, detail::units_hi<B, U>}) + 64;
        using I            = detail::wide_sint<detail::limbs_for_bits(bits)>;
        I total{0};
        if constexpr (!detail::wide_index_storage<B> && sizeof(detail::raw_t<B>) <= 4) {
            // Each value is base + raw·scale units (scale 1 when anchored).
            constexpr imax base =
                detail::index_storage<B> ? static_cast<imax>(detail::exact_quotient(lower_of<B>, U)) : 0;
            constexpr imax scale = static_cast<imax>(detail::exact_quotient(notch_of<B>, U));
            auto           it    = std::ranges::begin(r);
            auto           end   = std::ranges::end(r);
            while (it != end) {
                imax acc = 0, cnt = 0;
                // (A random-access range whose end is a sentinel, not a
                // reachable iterator, has no distance to it: the loop below.)
                if constexpr (std::ranges::random_access_range<Rng> &&
                              std::sized_sentinel_for<std::ranges::sentinel_t<Rng>, std::ranges::iterator_t<Rng>>) {
                    const imax block = std::min<imax>(end - it, imax{1} << 30);
                    for (imax j = 0; j < block; ++j)
                        acc += detail::raw_imax(it[j]);
                    it += block;
                    cnt = block;
                } else {
                    for (; it != end && cnt < (imax{1} << 30); ++it, ++cnt)
                        acc += detail::raw_imax(*it);
                }
                if constexpr (scale == 1)
                    total += I{acc} + I{cnt} * I{base};
                else
                    total += I{acc} * I{scale} + I{cnt} * I{base};
            }
        } else
            for (const auto& b : r)
                total += detail::value_in_units<I, U>(b);
        if constexpr (!detail::wide_valued<B>) {
            // A total within imax: the cheaper 64-bit rational store.
            constexpr imax lo = std::numeric_limits<imax>::min(), hi = std::numeric_limits<imax>::max();
            if (!(total < I{lo}) && !(I{hi} < total))
                if (const auto v =
                        detail::try_mul(detail::rational{static_cast<imax>(total)}, detail::to_rational(U))) {
                    detail::assignment<Target, detail::rational>::assign(out, *v, policy, no_action{});
                    return out;
                }
        }
        constexpr std::size_t K =
            detail::limbs_for_bits(I::bits + detail::exact_value_bits<B> + detail::exact_value_bits<Target>);
        using W = detail::wide_sint<K>;
        const detail::wide_rational<K> v{static_cast<W>(total) * static_cast<W>(detail::wide_numerator(U)),
                                         static_cast<W>(detail::wide_denominator(U))};
        no_action                      no_op;
        detail::assign_exact<detail::rational>(out, v, policy, no_op);
    } else {
        detail::rational total{0};
        for (const auto& b : r) {
            const auto s = total + detail::as_rational(b);
            if (!s) [[unlikely]] {
                no_action no_op;
                detail::report_failure(out, policy, no_op, s.error());
                return out;
            }
            total = *s;
        }
        detail::assignment<Target, detail::rational>::assign(out, total, policy, no_action{});
    }
    return out;
}

//---------------------------------------------------------------------------
// dot / cross / lerp — 2-D inside-space vector helpers. Each widens its result
// grid like the underlying `+`/`*`, so no overflow and the result is a plain
// `inside`. (cross is the z-component, useful for "which side" tests.)
//---------------------------------------------------------------------------
[[nodiscard]] constexpr auto dot(insidable auto ax, insidable auto ay, insidable auto bx, insidable auto by) {
    return ax * bx + ay * by;
}

[[nodiscard]] constexpr auto cross(insidable auto ax, insidable auto ay, insidable auto bx, insidable auto by) {
    return ax * by - ay * bx;
}

// lerp(a, b, t) = a + (b - a) * t. `t` is itself an inside (typically a
// [0, 1] fixed-point grid), so the interpolation never leaves inside-space.
[[nodiscard]] constexpr auto lerp(insidable auto a, insidable auto b, insidable auto t) { return a + (b - a) * t; }

//---------------------------------------------------------------------------
// common_inside — the "hull" type able to hold every value of L and R exactly:
// interval hull + notch gcd (grid `hull`), checked like arithmetic results.
// Backs the std::common_type specialization and mixed-grid min/max below.
//---------------------------------------------------------------------------
namespace detail {
template <insidable Lhs, insidable Rhs>
struct common_inside {
    using type = inside<hull(grid_of<Lhs>, grid_of<Rhs>)>;
};

// Same type stays itself (policy included) — mirrors std::common_type<T, T>.
template <insidable Same>
struct common_inside<Same, Same> {
    using type = Same;
};
} // namespace detail

template <insidable Lhs, insidable Rhs>
using common_inside_t = typename detail::common_inside<Lhs, Rhs>::type;

//---------------------------------------------------------------------------
// std-vocabulary helpers — ADL-found `min` / `max` / `midpoint` for generic
// code. min/max mirror std; midpoint returns the *exact* average on a refined
// grid (so, unlike std::midpoint, it neither rounds nor overflows). There is
// no free `beman::inside::clamp` (the name is the policy flag — use clamp_cast<Target>).
//---------------------------------------------------------------------------
// Both return the common type: the operand type itself for one type, else
// the hull, which holds each operand exactly.
template <insidable Lhs, insidable Rhs>
[[nodiscard]] constexpr auto min(Lhs a, Rhs b) -> common_inside_t<Lhs, Rhs> {
    common_inside_t<Lhs, Rhs> ca{a}, cb{b};
    return (cb < ca) ? cb : ca;
}

template <insidable Lhs, insidable Rhs>
[[nodiscard]] constexpr auto max(Lhs a, Rhs b) -> common_inside_t<Lhs, Rhs> {
    common_inside_t<Lhs, Rhs> ca{a}, cb{b};
    return (ca < cb) ? cb : ca;
}

// The exact average, on the refined sum grid.
template <insidable Lhs, insidable Rhs>
[[nodiscard]] constexpr auto midpoint(Lhs a, Rhs b) {
    return (a + b) * just<1, 2>;
}

//---------------------------------------------------------------------------
// expected-lift operators — fallible results (division, modulo, checked
// rational arithmetic, beman::inside::math) chain without per-step unwrapping:
// `a / b * gain + offset` and `math::tan(x) * gain` stay a
// std::expected<inside, errc> end to end. The first error short-circuits and
// keeps its cause; an operation that fails inside the chain reports its own
// (overflow, division_by_zero, ...). One overload per operator covers all
// three shapes (expected op inside, inside op expected, expected op expected);
// the lambda's `l + r` re-enters resolution on the unwrapped values,
// inheriting whichever bare overload applies.
//---------------------------------------------------------------------------
namespace detail {
// Other types whose results chain the same way (units.hpp: quantity).
template <class T>
inline constexpr bool lift_operand_v = insidable<T>;

template <class L, class R>
concept expected_operands =
    (expected_like<L> || expected_like<R>) && (lift_operand_v<unwrap_t<L>> || lift_operand_v<unwrap_t<R>>);
} // namespace detail

#define BEMAN_INSIDE_LIFT_OP(op)                                                            \
    template <class L, class R>                                                             \
        requires detail::expected_operands<L, R> &&                                         \
                 requires(detail::unwrap_t<L> l, detail::unwrap_t<R> r) { l op r; }         \
    [[nodiscard]] constexpr auto operator op(L const& lhs, R const& rhs) {                  \
        return detail::lift([](auto const& l, auto const& r) { return l op r; }, lhs, rhs); \
    }

BEMAN_INSIDE_LIFT_OP(+)
BEMAN_INSIDE_LIFT_OP(-)
BEMAN_INSIDE_LIFT_OP(*)
BEMAN_INSIDE_LIFT_OP(/)
BEMAN_INSIDE_LIFT_OP(%)
#undef BEMAN_INSIDE_LIFT_OP

//---------------------------------------------------------------------------
// Grid-less scalar operands are rejected. A raw int/double carries no grid, so
// `inside op rawscalar` has no type-safe result; rather than silently escape
// into rational/double, these guidance overloads make it ill-formed with a fix
// (give the literal a grid: `1_ins` / `just<1>`, or an inside over its range).
// Compound assignment with a raw scalar is rejected the same way; comparisons
// with raw scalars are unaffected.
//
// Concrete (non-auto) return type on purpose: keeps these SFINAE-transparent,
// so `requires { b + 1; }` stays well-formed and the static_assert fires only
// on a call.
//---------------------------------------------------------------------------
template <typename A>
concept builtin_arithmetic = std::integral<A> || std::floating_point<A>;

#define BEMAN_INSIDE_SCALAR_MSG                                                     \
    "an inside cannot be combined with a raw scalar: give the scalar a grid — "     \
    "`1_ins`, `just<1>`, or `inside<{lo,hi}>{n}` for a runtime value with a known " \
    "range"
#define BEMAN_INSIDE_NO_SCALAR(op)                     \
    template <insidable B, builtin_arithmetic A>       \
    B operator op(B const&, A) {                       \
        static_assert(false, BEMAN_INSIDE_SCALAR_MSG); \
    }                                                  \
    template <builtin_arithmetic A, insidable B>       \
    B operator op(A, B const&) {                       \
        static_assert(false, BEMAN_INSIDE_SCALAR_MSG); \
    }                                                  \
    template <insidable B, builtin_arithmetic A>       \
    B& operator op## = (B&, A) {                       \
        static_assert(false, BEMAN_INSIDE_SCALAR_MSG); \
    }

BEMAN_INSIDE_NO_SCALAR(+)
BEMAN_INSIDE_NO_SCALAR(-)
BEMAN_INSIDE_NO_SCALAR(*)
BEMAN_INSIDE_NO_SCALAR(/)
BEMAN_INSIDE_NO_SCALAR(%)
#undef BEMAN_INSIDE_NO_SCALAR
#undef BEMAN_INSIDE_SCALAR_MSG

} // namespace beman::inside

// std::common_type is the grid hull (beman::inside::common_inside_t above), so
// mixed-grid insides work in generic code (std::max over two grids, ranges)
// without opting into numeric_limits.hpp.
template <beman::inside::grid G1, beman::inside::policy_flag P1, beman::inside::grid G2, beman::inside::policy_flag P2>
struct std::common_type<beman::inside::inside<G1, P1>, beman::inside::inside<G2, P2>>
    : beman::inside::detail::common_inside<beman::inside::inside<G1, P1>, beman::inside::inside<G2, P2>> {};

#endif // BEMAN_INSIDE_ARITHMETIC_HPP
