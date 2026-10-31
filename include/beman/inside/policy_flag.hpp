// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_POLICY_FLAG_HPP
#define BEMAN_INSIDE_POLICY_FLAG_HPP

#include <beman/inside/detail/config.hpp>

#include <beman/inside/detail/rounding.hpp>

#include <tuple>
#include <type_traits>
#include <utility>

namespace beman::inside {
//---------------------------------------------------------------------------
// policy_flag — the policy bits of inside<G, P> (none, snap and the
// rounding modes, clamp, wrap, checked, unsafe, uninitialized),
// has_flag / is_checked, the rounding precedence (rounding_of), the hook
// factories on_clamp / on_wrap / on_error with their is_*_action traits, and
// the flags each hook implies.
//---------------------------------------------------------------------------
using policy_flag = unsigned long long;

namespace detail {
// A named flag is an object of its own type that converts to its bits: it is
// a template argument (inside<G, round_floor>), combines with | (clamp |
// round_floor stays a flag), and selects a call's policy (div(a, b,
// round_floor)). A policy_flag value, its bits, is what every template
// takes, so inside<G, round_floor> is inside<G, 48>.
template <policy_flag V>
struct flag {
    static constexpr policy_flag value = V;
    constexpr                    operator policy_flag() const noexcept { return V; }
};
template <policy_flag A, policy_flag B>
constexpr flag<A | B> operator|(flag<A>, flag<B>) noexcept {
    return {};
}
template <class T>
inline constexpr bool is_flag_v = false;
template <policy_flag V>
inline constexpr bool is_flag_v<flag<V>> = true;
template <class T>
concept flag_like = is_flag_v<std::remove_cvref_t<T>>;
} // namespace detail

// Check model: compile-time checks always run. A store whose success can't be
// proven statically gets a runtime check that throws (or reports through an
// errc& parameter), unless `unsafe` appears: in the type, an operand, or the
// call's policy.
inline constexpr detail::flag<0ull> none{};
// `snap` — permission: an off-notch value is rounded to fit the grid instead
// of rejected. Without it, an off-notch value is a compile/runtime error and
// div/mod fall through to exact-rational results. Every round_* mode carries
// it; on its own it rounds toward zero.
inline constexpr detail::flag<1ull << 4> snap{};
// `round_toward_zero` — the snap bit alone: truncate toward zero (IEEE 754
// roundTowardZero). Last in dispatch, after every explicit mode.
inline constexpr detail::flag<snap>               round_toward_zero{};
inline constexpr detail::flag<(1ull << 5) | snap> round_nearest{};
// Rounding modes each pick a unique bit and OR in `snap`. Conceptually
// exclusive; combining two is allowed but dispatch (storage.hpp) picks the
// first match: floor → ceil → half_even → nearest → toward zero (rounding_of).
inline constexpr detail::flag<(1ull << 6) | snap> round_floor{};
inline constexpr detail::flag<(1ull << 7) | snap> round_ceil{};
inline constexpr detail::flag<(1ull << 8) | snap> round_half_even{};

// runtime checking — the default policy's name, no bits: every policy without
// `unsafe` checks (see is_checked), so inside<G>, inside<G, none> and
// inside<G, checked> are one type.
inline constexpr detail::flag<0ull> checked{};

// unary — mutually exclusive
inline constexpr detail::flag<1ull << 32> clamp{}; // saturate to boundary
inline constexpr detail::flag<1ull << 33> wrap{};  // modular arithmetic

// `uninitialized` — default construction leaves the raw uninitialized, like a
// built-in scalar (a trivial default ctor); reading it before a store is
// undefined. Without it a default-constructed inside is 0, or Lower on a grid
// without 0.
inline constexpr detail::flag<1ull << 37> uninitialized{};

// No flag picks the raw: storage follows the grid alone (grid.hpp
// raw_for, docs/storage.md).

// The one opt-out of the runtime checks: no range or zero-divisor check
// (reading an out-of-range value or dividing by zero is UB; `/= 0` no-ops,
// `a / 0` skips the check). Includes `snap`, so an off-notch value truncates.
namespace detail {
inline constexpr policy_flag unsafe_marker{1ull << 36};
// Marks a cursor (`cursor<T, Step>`): default construction starts at Lower,
// and `end(t)` names its past-the-end value. Results never carry it.
inline constexpr policy_flag cursor_marker{1ull << 50};
} // namespace detail
inline constexpr detail::flag<detail::unsafe_marker | snap> unsafe{};

namespace detail {
// The bits of the four explicit rounding modes (each also carries snap).
inline constexpr policy_flag mode_bits = (1ull << 5) | (1ull << 6) | (1ull << 7) | (1ull << 8);
// Every bit a flag names: a policy holds no other.
inline constexpr policy_flag known_bits =
    mode_bits | snap | clamp | wrap | unsafe_marker | uninitialized | cursor_marker;

// How many explicit rounding modes a flag set names (an inside takes one).
[[nodiscard]] constexpr int rounding_modes(policy_flag f) noexcept {
    int n = 0;
    for (policy_flag bit = 1ull << 5; bit <= (1ull << 8); bit <<= 1)
        n += (f & bit) != 0;
    return n;
}

// f with round_nearest added unless it names a mode already, which would win
// over round_nearest in rounding_of anyway: one mode either way.
[[nodiscard]] constexpr policy_flag with_default_rounding(policy_flag f) noexcept {
    return (f & mode_bits) != 0 ? f : f | round_nearest;
}
} // namespace detail

//---------------------------------------------------------------------------
// Flag-set membership predicates. `has_flag(set, flag)` is true iff EVERY bit
// of `flag` is present in `set` — reads better than the raw `(set & flag) ==
// flag` and is correct for composite flags (e.g. `round_nearest` carries
// `snap`), where a bare `set & flag`
// truthy test would misfire. `has_any_flag` tests for any overlap.
//---------------------------------------------------------------------------
[[nodiscard]] constexpr bool has_flag(policy_flag set, policy_flag flag) noexcept { return (set & flag) == flag; }

[[nodiscard]] constexpr bool has_any_flag(policy_flag set, policy_flag flags) noexcept {
    return (set & flags) != none;
}

// Runtime checks run unless the flag set carries `unsafe`, which wins over
// `checked` (the default policy's own bit, so `x.policy<unsafe>()` opts a
// default inside out). `inside<G, round_nearest>` and `inside<G, clamp>` are
// checked, exactly like the default `inside<G>`.
[[nodiscard]] constexpr bool is_checked(policy_flag set) noexcept { return !has_flag(set, detail::unsafe_marker); }

// Whether an out-of-range value does anything under a flag set: a clamp or
// wrap bit stores it, a checked range reports it. False under `unsafe`.
[[nodiscard]] constexpr bool range_handled(policy_flag set) noexcept {
    return has_any_flag(set, clamp | wrap) || is_checked(set);
}

namespace detail {
// The rounding mode a flag set selects — the ONE precedence every rounding
// path uses (integer and rational storage, division, math stores).
// An explicit directional or half-even mode beats round_nearest; `snap` alone, or no
// rounding flag at all, truncates toward zero. Ties of `nearest` go half
// away from zero. round_mode and the shared decision: detail/rounding.hpp.

[[nodiscard]] constexpr round_mode rounding_of(policy_flag f) noexcept {
    if (has_flag(f, round_floor))
        return round_mode::floor;
    if (has_flag(f, round_ceil))
        return round_mode::ceil;
    if (has_flag(f, round_half_even))
        return round_mode::half_even;
    if (has_flag(f, round_nearest))
        return round_mode::nearest;
    return round_mode::trunc;
}
} // namespace detail

//---------------------------------------------------------------------------
// no_action — zero-overhead default: a store without a hook
//---------------------------------------------------------------------------
struct no_action {};

//---------------------------------------------------------------------------
// tagged actions — opt-in callbacks for each failure path.
// The lambda receives the inside by mutable reference as its first argument,
// so the handler can override the value the policy was about to store.
//---------------------------------------------------------------------------
template <typename F>
struct on_clamp_t {
    [[no_unique_address]] F Fn;
};
template <typename F>
struct on_wrap_t {
    [[no_unique_address]] F Fn;
};
template <typename F>
struct on_error_t {
    [[no_unique_address]] F Fn;
};

//---------------------------------------------------------------------------
// CTAD-style factories — drop the on_error_t{lambda} brace-init.
//---------------------------------------------------------------------------
template <typename F>
[[nodiscard]] constexpr auto on_clamp(F&& fn) {
    return on_clamp_t<std::remove_cvref_t<F>>{std::forward<F>(fn)};
}
template <typename F>
[[nodiscard]] constexpr auto on_wrap(F&& fn) {
    return on_wrap_t<std::remove_cvref_t<F>>{std::forward<F>(fn)};
}
template <typename F>
[[nodiscard]] constexpr auto on_error(F&& fn) {
    return on_error_t<std::remove_cvref_t<F>>{std::forward<F>(fn)};
}

namespace detail {
// Action detection: the `is_*_action` trait is the primary detector (the
// pack helpers below take it as a template-template parameter); the concept
// derives from it and strips cvref so the ref form matches the value form.
template <template <typename> class Action, typename T>
inline constexpr bool is_action = false;
template <template <typename> class Action, typename F>
inline constexpr bool is_action<Action, Action<F>> = true;

template <typename T>
using is_clamp_action = std::bool_constant<is_action<on_clamp_t, T>>;
template <typename T>
using is_wrap_action = std::bool_constant<is_action<on_wrap_t, T>>;
template <typename T>
using is_error_action = std::bool_constant<is_action<on_error_t, T>>;

template <typename T>
concept clamp_action = is_clamp_action<std::remove_cvref_t<T>>::value;
template <typename T>
concept wrap_action = is_wrap_action<std::remove_cvref_t<T>>::value;
template <typename T>
concept error_action = is_error_action<std::remove_cvref_t<T>>::value;

//---------------------------------------------------------------------------
// implied_flags<A> — single source of truth for "this action requires these
// policy bits". Used by inside::on_*.
//---------------------------------------------------------------------------
template <typename T>
inline constexpr policy_flag implied_flags = none;
template <typename F>
inline constexpr policy_flag implied_flags<on_clamp_t<F>> = clamp;
template <typename F>
inline constexpr policy_flag implied_flags<on_wrap_t<F>> = wrap;
template <typename F>
inline constexpr policy_flag implied_flags<on_error_t<F>> = checked;

//---------------------------------------------------------------------------
// Pack helpers — let policy_ref and assignment accept Actions... packs.
// The `is_*_action` traits are passed as template-template parameters.
//---------------------------------------------------------------------------

// True if any element of the pack matches the trait.
template <template <typename> class Trait, typename... As>
inline constexpr bool has_action = (Trait<std::remove_cvref_t<As>>::value || ... || false);

// pick_action<Trait>(actions...) returns a reference to the first pack element
// matching the trait, or to `fallback_action` if none does. Conflict
// diagnostics elsewhere ensure at most one match.
inline no_action fallback_action;

template <template <typename> class Trait, typename... As>
constexpr auto& pick_action(As&... as) {
    constexpr std::size_t i = [] { // the first match, or sizeof...(As)
        std::size_t k = 0;
        (void)((Trait<std::remove_cvref_t<As>>::value || (++k, false)) || ...);
        return k;
    }();
    if constexpr (i < sizeof...(As))
        return as...[i];
    else
        return fallback_action;
}

// Same, but operating on a tuple.
template <template <typename> class Trait, typename Tuple>
constexpr auto& pick_action_in(Tuple& t) {
    auto& [... as] = t;
    return pick_action<Trait>(as...);
}

} // namespace detail
} // namespace beman::inside

#endif // BEMAN_INSIDE_POLICY_FLAG_HPP
