// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_ARITHMETIC_HPP
#define BEMAN_INSIDE_INSIDE_ARITHMETIC_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/compare.hpp> // the inside type and its comparisons
#include <beman/inside/inside/casts.hpp>
#include <beman/inside/inside/detail/op_result.hpp>

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
// add / sub / mul / div / mod / divmod — each takes one of two trailing forms:
//   op(l, r [, policy])   explicit per-call policy (the division's rounding):
//                         a named flag (round_floor) or make_policy<F>()
//   op(l, r, ec)          error-code form (checked, a zero divisor sets ec)
// detail::arith normalizes the form to a policy for the op's core.
//---------------------------------------------------------------------------
#define BEMAN_INSIDE_WHY_arith_action                                                                             \
    "add/sub/mul/div/mod take no hooks: on_clamp / on_wrap / on_error apply to a store - use b.on_clamp(f) etc. " \
    "or a cast"

namespace detail {
template <class Op, class L, class R, policy_like P = call_policy<>>
constexpr auto arith(Op op, const L& l, const R& r, P&& pol = {}) {
    return op(l, r, std::forward<P>(pol));
}

// A named flag as the policy: div(a, b, round_floor).
template <class Op, class L, class R, policy_flag V>
constexpr auto arith(Op op, const L& l, const R& r, flag<V>) {
    return op(l, r, make_policy<V>());
}

template <class Op, class L, class R>
constexpr auto arith(Op op, const L& l, const R& r, errc& ec) {
    return op(l, r, make_policy<checked>(ec));
}

// A hook would be silently ignored (arithmetic cannot fail but for a zero
// divisor, which stays in the result): rejected with a message (the deleted
// overload in BEMAN_INSIDE_ARITH_FN).
template <class A>
inline constexpr bool is_action_tag_v =
    is_clamp_action<A>::value || is_wrap_action<A>::value || is_error_action<A>::value;

struct add_op {
    template <class L, class R, class P>
    constexpr auto operator()(const L& l, const R& r, P&& p) const {
        return addition<L, R>::add(l, r, std::forward<P>(p));
    }
};
struct sub_op {
    template <class L, class R, class P>
    constexpr auto operator()(const L& l, const R& r, P&& p) const {
        return add_op{}(l, -r, std::forward<P>(p));
    }
};
struct mul_op {
    template <class L, class R, class P>
    constexpr auto operator()(const L& l, const R& r, P&& p) const {
        return multiplication<L, R>::mul(l, r, std::forward<P>(p));
    }
};
struct div_op {
    template <class L, class R, class P>
    constexpr auto operator()(const L& l, const R& r, P&& p) const {
        return division<L, R, flags_of<P>>::div(l, r, p);
    }
};
struct mod_op {
    template <class L, class R, class P>
    constexpr auto operator()(const L& l, const R& r, P&& p) const {
        return modulo<L, R, flags_of<P>>::mod(l, r, p);
    }
};
struct divmod_op {
    template <class L, class R, class P>
    constexpr auto operator()(const L& l, const R& r, P&& p) const {
        return quotient_remainder<L, R, flags_of<P>>::divmod(l, r, p);
    }
};
} // namespace detail

// A deleted overload takes each op's failing case: `requires { a + b; }` is
// then false, and an inside cannot fall back on its rational conversion and
// rational's operator.

#define BEMAN_INSIDE_WHY_add                                                                                     \
    "addition: no result type - the exact sum's fractions pass 1024 bits (store an operand into a coarser type " \
    "first)"
#define BEMAN_INSIDE_WHY_sub                                                                                  \
    "subtraction: no result type - the exact difference's fractions pass 1024 bits (store an operand into a " \
    "coarser type first)"
#define BEMAN_INSIDE_WHY_mul                                                                                    \
    "multiplication: no result type - the exact product's fractions pass 1024 bits - round the product into a " \
    "coarser type with mul_into<Out>(a, b)"
#define BEMAN_INSIDE_WHY_div                                                                              \
    "division: no result type - the divisor grid is the point 0, or the exact quotient's fractions pass " \
    "1024 bits"
#define BEMAN_INSIDE_WHY_mod "modulo requires notched grids (a continuous grid has no lattice)"
#define BEMAN_INSIDE_WHY_divmod "divmod requires notched grids (a continuous grid has no lattice)"

namespace detail {
// The policy flags a call's trailing arguments select, as arith normalizes
// them: a policy's own, checked for an errc&, an action's implied flags.
template <class... Args>
inline constexpr policy_flag call_flags = []() -> policy_flag {
    if constexpr (sizeof...(Args) == 0)
        return none;
    else if constexpr (policy_like<std::remove_cvref_t<Args...[0]>> || flag_like<Args...[0]>)
        return flags_of<Args...[0]>;
    else if constexpr (std::same_as<Args...[0], errc&>)
        return checked;
    else
        return none; // a hook: rejected below
}();

// Each op's well-formedness under flags F is detail::add_defined & co.
// (inside/detail/addition.hpp, multiplication.hpp, division.hpp): the first
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
    auto name(L const&, R const&, Args&&...) = delete (BEMAN_INSIDE_WHY_##name);    \
    template <insidable L, insidable R, class... Args>                              \
        requires(detail::name##_defined<L, R, detail::call_flags<Args...>> &&       \
                 (detail::is_action_tag_v<std::remove_cvref_t<Args>> || ...))       \
    auto name(L const&, R const&, Args&&...) = delete (BEMAN_INSIDE_WHY_arith_action);

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
// product or quotient grid, so they work where that grid's fractions would pass
// the fraction limit, or a quotient past the 64-bit fraction. A store that
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
        return values_fit_int64<P> && (notches_compatible<Out, P> || has_flag(all, snap)) &&
               (has_flag(all, clamp) || has_flag(all, wrap) || !excludes(interval_of<Out>, interval_of<P>));
    }
}();
template <insidable A, insidable B>
inline constexpr bool rational_operands = rational_readable<A> && rational_readable<B>;

// A Q-format store rounds a fraction as its reduced form would (one
// division of the offset), so a quotient for it needs no gcd: x/y as the
// unreduced (xn·yd)/(xd·yn). Den 0 when y is 0 or a part passes 64 bits;
// try_div decides then.
template <insidable Out>
inline constexpr bool unreduced_store = q_format_fits<Out> && notched<Out>;
constexpr rational    unreduced_quotient(rational x, rational y) noexcept {
    std::uint64_t num, den;
    if (y.Num == 0 || mul_overflow(x.Num, abs_den(y.Den), &num) || mul_overflow(abs_den(x.Den), y.Num, &den) ||
        !std::in_range<std::int64_t>(den))
        return rational_from_parts(0, 0);
    const std::int64_t d = static_cast<std::int64_t>(den);
    return rational_from_parts(num, (x.Den < 0) != (y.Den < 0) ? -d : d);
}

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
// The quotient as a reduced 64-bit rational, else wide.
template <insidable Out, insidable A, insidable B, typename P>
constexpr Out div_into_reduced(A a, B b, P policy) {
    if constexpr (rational_operands<A, B>)
        if (const auto q = try_div(as_rational(a), as_rational(b)))
            return store_into<Out>(*q, policy);
    return div_into_wide<Out>(a, b, policy);
}
// The same out of line, behind the unreduced quotient (by value, so the fast
// path's operands stay in registers).
template <insidable Out, insidable A, insidable B, typename P>
[[gnu::cold]] [[gnu::noinline]] constexpr Out div_into_reduced_cold(A a, B b, P policy) {
    return div_into_reduced<Out>(a, b, policy);
}
} // namespace detail

template <insidable Out, policy_flag F = detail::none, insidable A, insidable B>
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

template <insidable Out, policy_flag F = detail::none, insidable A, insidable B>
[[nodiscard]] constexpr auto div_into(const A& a, const B& b)
    -> std::conditional_t<detail::divisor_excludes_zero<B>, Out, std::expected<Out, errc>> {
    const auto quotient = [&](auto&& policy) {
        if constexpr (detail::rational_operands<A, B> && detail::unreduced_store<Out>) {
            if (const auto q = detail::unreduced_quotient(detail::as_rational(a), detail::as_rational(b)); q.Den != 0)
                [[likely]]
                return detail::store_into<Out>(q, policy);
            return detail::div_into_reduced_cold<Out>(a, b, policy);
        } else
            return detail::div_into_reduced<Out>(a, b, policy);
    };
    if constexpr (detail::divisor_excludes_zero<B>)
        return quotient(make_policy<flags_of<Out> | F>());
    else {
        if (b == 0)
            return std::unexpected{errc::division_by_zero};
        return detail::try_store<Out, flags_of<Out> | F>([&](Out& out, auto&& policy) { out = quotient(policy); });
    }
}

// Binary operators: each result keeps the policy its operands share
// (detail::result_flags); / and % also divide under the operands' flags
// (snap/rounding select the native integer paths).
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

namespace detail {
// How many raws of B a 32-bit accumulator sums without overflow (unsigned
// for raws ≥ 0, else signed), at most 2^30: 32-bit lanes are twice as many
// per vector register as int64 ones. 0 when a raw passes the lane.
template <insidable B>
inline constexpr std::int64_t lane32_block = [] {
    const grid_int      lo = raw_lo_exact<B>, hi = raw_hi_exact<B>;
    const grid_int      mag   = lo.negative() && hi < -lo ? -lo : hi;
    const std::uint64_t limit = lo.negative() ? std::uint64_t{0x7fff'ffff} : std::uint64_t{0xffff'ffff};
    if (grid_int{limit} < mag)
        return std::int64_t{0};
    const std::uint64_t m = static_cast<std::uint64_t>(mag);
    return m == 0 ? std::int64_t{1} << 30
                  : static_cast<std::int64_t>(std::min<std::uint64_t>(limit / m, std::uint64_t{1} << 30));
}();
} // namespace detail

namespace detail {
// A sum of fractions p/q with q ≤ D is a whole number of 1/lcm(1, …, D):
// continuous elements add exactly as numerators over that denominator. Its
// bits are capped (D up to about 2800); lcm(1..D) ≥ 2^D for D ≥ 7, so a
// bound past twice the cap is past it without forming the lcm.
inline constexpr int max_continuous_sum_bits = 4096;

// lcm(1, …, d): the largest power of each prime up to d.
consteval grid_int lcm_up_to(std::uint64_t d) {
    grid_int l{1};
    for (std::uint64_t p = 2; p <= d; ++p) {
        bool prime = true;
        for (std::uint64_t f = 2; f * f <= p && prime; ++f)
            prime = p % f != 0;
        if (prime) {
            std::uint64_t pk = p;
            while (pk <= d / p)
                pk *= p;
            l = l * grid_int{pk};
        }
    }
    return l;
}

// The bits of lcm(1..D) for B's bound D (past the cap: cap + 1).
template <insidable B>
consteval int continuous_sum_bits_of() {
    const grid_int d = grid_numerator(grid_of<B>.MaxDenominator);
    if (grid_int{2 * max_continuous_sum_bits} < d)
        return max_continuous_sum_bits + 1;
    return bit_width_of(lcm_up_to(static_cast<std::uint64_t>(d)));
}
template <insidable B>
inline constexpr int continuous_sum_bits = continuous_sum_bits_of<B>();
} // namespace detail

//---------------------------------------------------------------------------
// sum<Target> — bulk reduction with ONE deferred range check. Per-element
// `target += b` re-validates every step (blocks vectorization); this computes
// the exact total and stores it into Target once, by Target's policy
// (semantic difference: the *total* is validated, not every prefix).
// Notched integer raws (wide ones too) sum value indices exactly — Σvalue =
// Notch·ΣJ — in a wide integer with 64 bits of headroom for the count; ≤32-bit
// raws add in blocks, the loop that vectorizes: in 32-bit lanes where a
// block of at least 64 fits them, else in int64 blocks of 2^30 elements.
// Continuous elements add exactly over lcm(1..D), D their denominator bound
// (lcm(1..D) at most 4096 bits); n copies of a point are n·value.
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
        constexpr int bits = detail::signed_value_bits({detail::units_lo<B, U>, detail::units_hi<B, U>}) + 64;
        using I            = detail::wide_sint<detail::limbs_for_bits(bits)>;
        I total{0};
        if constexpr (!detail::wide_index_storage<B> && sizeof(detail::raw_t<B>) <= 4) {
            // Each value is base + raw·scale units (scale 1 when anchored), in
            // the total's type: a grid far from 0 has a base past 64 bits.
            constexpr I base  = detail::index_storage<B> ? static_cast<I>(detail::units_lo<B, U>) : I{0};
            constexpr I scale = static_cast<I>(detail::exact_quotient(notch_of<B>, U));
            auto        it    = std::ranges::begin(r);
            auto        end   = std::ranges::end(r);
            while (it != end) {
                std::int64_t acc = 0, cnt = 0;
                // (A random-access range whose end is a sentinel, not a
                // reachable iterator, has no distance to it: the loop below.)
                if constexpr (std::ranges::random_access_range<Rng> &&
                              std::sized_sentinel_for<std::ranges::sentinel_t<Rng>, std::ranges::iterator_t<Rng>>) {
                    if constexpr (constexpr std::int64_t lanes = detail::lane32_block<B>; lanes >= 64) {
                        using L = std::conditional_t<detail::raw_lo_exact<B>.negative(), std::int32_t, std::uint32_t>;
                        const std::int64_t block = std::min<std::int64_t>(end - it, lanes);
                        L                  a     = 0;
                        for (std::int64_t j = 0; j < block; ++j)
                            a += static_cast<L>(it[j].raw());
                        acc = static_cast<std::int64_t>(a);
                        it += block;
                        cnt = block;
                    } else {
                        const std::int64_t block = std::min<std::int64_t>(end - it, std::int64_t{1} << 30);
                        for (std::int64_t j = 0; j < block; ++j)
                            acc += detail::raw_int64(it[j]);
                        it += block;
                        cnt = block;
                    }
                } else {
                    for (; it != end && cnt < (std::int64_t{1} << 30); ++it, ++cnt)
                        acc += detail::raw_int64(*it);
                }
                if constexpr (scale == I{1})
                    total += I{acc} + I{cnt} * base;
                else
                    total += I{acc} * scale + I{cnt} * base;
            }
        } else
            for (const auto& b : r)
                total += detail::value_in_units<I, U>(b);
        if constexpr (!detail::wide_valued<B>) {
            // A total within int64: the cheaper 64-bit rational store.
            constexpr std::int64_t lo = std::numeric_limits<std::int64_t>::min(),
                                   hi = std::numeric_limits<std::int64_t>::max();
            if (!(total < I{lo}) && !(I{hi} < total))
                if (const auto v =
                        detail::try_mul(detail::rational{static_cast<std::int64_t>(total)}, detail::to_rational(U))) {
                    detail::assignment<Target, detail::rational>::assign(out, *v, policy, detail::no_action{});
                    return out;
                }
        }
        constexpr std::size_t K =
            detail::limbs_for_bits(I::bits + detail::exact_value_bits<B> + detail::exact_value_bits<Target>);
        using W = detail::wide_sint<K>;
        const detail::wide_rational<K> v{static_cast<W>(total) * static_cast<W>(detail::grid_numerator(U)),
                                         static_cast<W>(detail::grid_denominator(U))};
        detail::no_action              no_op;
        detail::assign_exact<detail::rational>(out, v, policy, no_op);
    } else if constexpr (detail::point_storage<B>) {
        // n copies of the one value: n·value.
        std::uint64_t n = 0;
        for ([[maybe_unused]] const auto& b : r)
            ++n;
        constexpr std::size_t K = detail::limbs_for_bits(detail::exact_value_bits<B> + 66);
        using W                 = detail::wide_sint<K>;
        const detail::wide_rational<K> v =
            detail::exact_of_grid<K>(lower_of<B>) * detail::wide_rational<K>{W{n}, W{1}};
        detail::no_action no_op;
        detail::assign_exact<detail::rational>(out, v, policy, no_op);
    } else {
        // Continuous elements: each value p/q has q ≤ D, B's bound, so the
        // total is a whole number of 1/L, L = lcm(1..D); the numerators p·(L/q)
        // add exactly, with 64 bits of headroom for the count.
        static_assert(detail::continuous_sum_bits<B> <= detail::max_continuous_sum_bits,
                      "sum: the elements' denominator bound is too large for an exact total (lcm(1..D) passes "
                      "4096 bits) - store them onto a notched grid first");
        constexpr int bits = detail::continuous_sum_bits<B>;
        using N            = detail::wide_uint<detail::limbs_for_bits(bits)>;
        constexpr N L      = static_cast<N>(
            detail::lcm_up_to(static_cast<std::uint64_t>(detail::grid_numerator(grid_of<B>.MaxDenominator))));
        constexpr std::size_t K = detail::limbs_for_bits(bits + detail::exact_value_bits<B> + 66);
        using I                 = detail::wide_sint<K>;
        I total{0};
        for (const auto& b : r) {
            auto e  = detail::exact_of(b); // Den > 0
            auto qr = detail::divmod_small(L, static_cast<std::uint64_t>(e.Den));
            if (qr.Remainder != 0 || bit_width_of(e.Den) > 64) [[unlikely]] { // (ADL: wide_int's friend)
                e  = detail::reduced(e); // a raw built unreduced: its reduced Den ≤ D divides L
                qr = detail::divmod_small(L, static_cast<std::uint64_t>(e.Den));
            }
            total += static_cast<I>(e.Num) * static_cast<I>(qr.Quotient);
        }
        const detail::wide_rational<K> v{total, static_cast<I>(L)};
        detail::no_action              no_op;
        detail::assign_exact<detail::rational>(out, v, policy, no_op);
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

namespace detail {
// lerp's type: a + (b - a)·t's lattice; when t's grid lies in [0, 1] the
// result lies between a and b, so the interval narrows to the hull of a's and
// b's ranges, snapped inward onto that lattice.
template <insidable W, insidable A, insidable B, insidable T>
consteval grid lerp_grid() {
    grid g = grid_of<W>;
    if constexpr (lower_of<T> >= 0 && upper_of<T> <= 1) {
        grid_rational lo = lower_of<A> < lower_of<B> ? lower_of<A> : lower_of<B>;
        grid_rational hi = upper_of<A> < upper_of<B> ? upper_of<B> : upper_of<A>;
        if (g.Notch != 0) { // onto the lattice, inward
            const grid_rational base = g.Interval.Lower, n = g.Notch;
            lo = base + n * grid_round<round_mode::ceil>((lo - base) / n);
            hi = base + n * grid_round<round_mode::floor>((hi - base) / n);
        }
        // Only ever narrower: a point (the lerp of constants) stays a point.
        g.Interval =
            interval{g.Interval.Lower < lo ? lo : g.Interval.Lower, hi < g.Interval.Upper ? hi : g.Interval.Upper};
    }
    return g;
}
} // namespace detail

// lerp(a, b, t) = a + (b - a) * t. `t` is itself an inside (typically a
// [0, 1] fixed-point grid), so the interpolation never leaves inside-space;
// with t in [0, 1] the result's range is that of a and b together.
template <insidable A, insidable B, insidable T>
[[nodiscard]] constexpr auto lerp(A a, B b, T t) {
    const auto w = a + (b - a) * t;
    using W      = std::remove_cvref_t<decltype(w)>;
    using R      = inside<detail::lerp_grid<W, A, B, T>(), flags_of<W>>;
    if constexpr (std::is_same_v<R, W>)
        return w;
    else
        return R{w, unsafe}; // within R by the bound above
}

//---------------------------------------------------------------------------
// common_inside — the "hull" type able to hold every value of L and R exactly:
// interval hull + notch gcd (grid `hull`), under the policy both share, as
// for arithmetic results (result_flags). Backs the std::common_type
// specialization.
//---------------------------------------------------------------------------
namespace detail {
template <insidable Lhs, insidable Rhs>
struct common_inside {
    using type = inside<hull(grid_of<Lhs>, grid_of<Rhs>), result_flags<Lhs, Rhs>>;
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
// no free `beman::inside::clamp` (the name is the policy flag — use Target{v, clamp}).
//---------------------------------------------------------------------------
namespace detail {
// The type of min(a, b) or max(a, b): the hull's lattice over the values the
// result can take, [min(lowers), min(uppers)] or [max(lowers), max(uppers)].
// One type stays itself, as for the common type.
template <insidable Lhs, insidable Rhs, bool Max>
consteval grid extreme_grid() {
    grid           h = hull(grid_of<Lhs>, grid_of<Rhs>);
    const interval a = grid_of<Lhs>.Interval, b = grid_of<Rhs>.Interval;
    if constexpr (Max)
        h.Interval = interval{a.Lower < b.Lower ? b.Lower : a.Lower, h.Interval.Upper};
    else
        h.Interval = interval{h.Interval.Lower, a.Upper < b.Upper ? a.Upper : b.Upper};
    return h;
}
template <insidable Lhs, insidable Rhs, bool Max>
struct extreme_type {
    using type = inside<extreme_grid<Lhs, Rhs, Max>(), result_flags<Lhs, Rhs>>;
};
template <insidable Same, bool Max>
struct extreme_type<Same, Same, Max> {
    using type = Same;
};
} // namespace detail

// The smaller and the larger operand, typed by the values they can take: the
// winner always fits, so it is stored without a run-time check.
// An operand whose range never reaches the other's is decided at compile time
// (its store into R would be ill-formed: the ranges are disjoint).
template <insidable Lhs, insidable Rhs>
[[nodiscard]] constexpr auto min(Lhs a, Rhs b) -> typename detail::extreme_type<Lhs, Rhs, false>::type {
    using R = typename detail::extreme_type<Lhs, Rhs, false>::type;
    if constexpr (std::is_same_v<Lhs, Rhs>)
        return (b < a) ? b : a;
    else if constexpr (upper_of<Lhs> <= lower_of<Rhs>) // a always wins (a tie keeps a)
        return R{a, unsafe};
    else if constexpr (upper_of<Rhs> < lower_of<Lhs>) // b always wins
        return R{b, unsafe};
    else
        return (b < a) ? R{b, unsafe} : R{a, unsafe};
}

template <insidable Lhs, insidable Rhs>
[[nodiscard]] constexpr auto max(Lhs a, Rhs b) -> typename detail::extreme_type<Lhs, Rhs, true>::type {
    using R = typename detail::extreme_type<Lhs, Rhs, true>::type;
    if constexpr (std::is_same_v<Lhs, Rhs>)
        return (a < b) ? b : a;
    else if constexpr (upper_of<Rhs> <= lower_of<Lhs>) // a always wins (a tie keeps a)
        return R{a, unsafe};
    else if constexpr (upper_of<Lhs> < lower_of<Rhs>) // b always wins
        return R{b, unsafe};
    else
        return (a < b) ? R{b, unsafe} : R{a, unsafe};
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
// Deleted, with the fix as the reason: `requires { b + 1; }` is false, as
// for every other operation that cannot compile.
//---------------------------------------------------------------------------
namespace detail {
template <typename A>
concept builtin_arithmetic = std::integral<A> || std::floating_point<A>;
} // namespace detail

#define BEMAN_INSIDE_SCALAR_MSG                                                     \
    "an inside cannot be combined with a raw scalar: give the scalar a grid — "     \
    "`1_ins`, `just<1>`, or `inside<{lo,hi}>{n}` for a runtime value with a known " \
    "range"
#define BEMAN_INSIDE_NO_SCALAR(op)                                    \
    template <insidable B, detail::builtin_arithmetic A>              \
    void operator op(B const&, A) = delete (BEMAN_INSIDE_SCALAR_MSG); \
    template <detail::builtin_arithmetic A, insidable B>              \
    void operator op(A, B const&) = delete (BEMAN_INSIDE_SCALAR_MSG); \
    template <insidable B, detail::builtin_arithmetic A>              \
    void operator op## = (B&, A) = delete (BEMAN_INSIDE_SCALAR_MSG);

BEMAN_INSIDE_NO_SCALAR(+)
BEMAN_INSIDE_NO_SCALAR(-)
BEMAN_INSIDE_NO_SCALAR(*)
BEMAN_INSIDE_NO_SCALAR(/)
BEMAN_INSIDE_NO_SCALAR(%)
#undef BEMAN_INSIDE_NO_SCALAR
#undef BEMAN_INSIDE_SCALAR_MSG

// An inside with a rational: through the implicit operator rational() it
// would become rational arithmetic, an expected<rational, errc> without a
// grid. Deleted, with the spelling that keeps the grid; so are the compound
// assignments (inside/detail/core.hpp, policy_ref.hpp).
#define BEMAN_INSIDE_RATIONAL_MSG                                                                \
    "an inside cannot be combined with a rational: write the constant as just<1, 2> or 0.5_ins " \
    "(a grid), or store the rational into an inside first"
#define BEMAN_INSIDE_NO_RATIONAL(op)                                           \
    template <insidable B, std::same_as<rational> Q>                           \
    void operator op(B const&, Q const&) = delete (BEMAN_INSIDE_RATIONAL_MSG); \
    template <std::same_as<rational> Q, insidable B>                           \
    void operator op(Q const&, B const&) = delete (BEMAN_INSIDE_RATIONAL_MSG);

BEMAN_INSIDE_NO_RATIONAL(+)
BEMAN_INSIDE_NO_RATIONAL(-)
BEMAN_INSIDE_NO_RATIONAL(*)
BEMAN_INSIDE_NO_RATIONAL(/)
BEMAN_INSIDE_NO_RATIONAL(%)
#undef BEMAN_INSIDE_NO_RATIONAL
#undef BEMAN_INSIDE_RATIONAL_MSG

} // namespace beman::inside

// std::common_type is the grid hull (beman::inside::common_inside_t above), so
// mixed-grid insides work in generic code (std::max over two grids, ranges).
template <beman::inside::grid G1, beman::inside::policy_flag P1, beman::inside::grid G2, beman::inside::policy_flag P2>
struct std::common_type<beman::inside::inside<G1, P1>, beman::inside::inside<G2, P2>>
    : beman::inside::detail::common_inside<beman::inside::inside<G1, P1>, beman::inside::inside<G2, P2>> {};

#endif // BEMAN_INSIDE_INSIDE_ARITHMETIC_HPP
