// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_POLICY_FLAG_HPP
#define BEMAN_INSIDE_POLICY_FLAG_HPP

#include <beman/inside/detail/config.hpp>

#include <beman/inside/detail/rounding.hpp>

#include <tuple>
#include <type_traits>
#include <utility>

// BEMAN_INSIDE_MATH_NO_FP (auto-enabled on freestanding targets) and the
// -ffast-math check are in config.hpp, which every header includes first.

namespace beman::inside {
//---------------------------------------------------------------------------
// policy_flag — the policy bits of inside<G, P> (none, ignore_zero,
// ignore_range, snap and the rounding modes, clamp, wrap, checked, unsafe),
// has_flag / is_checked, the rounding precedence (rounding_of), the hook
// factories on_clamp / on_wrap / on_error / on_overflow with their
// is_*_action traits, and the flags each hook implies.
//---------------------------------------------------------------------------
using policy_flag = unsigned long long;

// Check model: compile-time checks always run. When success can't be proven
// statically, compilation fails unless the matching ignore flag is set; else a
// runtime check is inserted that throws (or reports through an errc& parameter).
// Binary operations OR the flags of both operands.
inline constexpr policy_flag none{0ull};
inline constexpr policy_flag ignore_zero{1ull << 1};
inline constexpr policy_flag ignore_range{1ull << 2};
// `snap` — permission: an off-notch value is rounded to fit the grid instead
// of rejected. Without it, an off-notch value is a compile/runtime error and
// div/mod fall through to exact-rational results. Every round_* mode carries
// it; on its own it rounds toward zero.
inline constexpr policy_flag snap{1ull << 4};
// `round_toward_zero` — the snap bit alone: truncate toward zero (IEEE 754
// roundTowardZero). Last in dispatch, after every explicit mode.
inline constexpr policy_flag round_toward_zero{snap};
inline constexpr policy_flag round_nearest{(1ull << 5) | snap};
// Rounding modes each pick a unique bit and OR in `snap`. Conceptually
// exclusive; combining two is allowed but dispatch (storage.hpp) picks the
// first match: floor → ceil → half_even → nearest → toward zero (rounding_of).
inline constexpr policy_flag round_floor{(1ull << 6) | snap};
inline constexpr policy_flag round_ceil{(1ull << 7) | snap};
inline constexpr policy_flag round_half_even{(1ull << 8) | snap};

// runtime checking — on unless the policy carries `unsafe` (see is_checked).
// Spelling `checked` re-enables the checks alongside `unsafe`.
inline constexpr policy_flag checked{1ull << 34}; // runtime range/notch/overflow checks

// unary — mutually exclusive
inline constexpr policy_flag clamp{1ull << 32}; // saturate to boundary
inline constexpr policy_flag wrap{1ull << 33};  // modular arithmetic

// No flag picks the raw: storage follows the grid alone (grid.hpp
// raw_for, docs/storage.md).

// opt-out of `checked`: no domain/round/overflow/div-by-zero checks (reading
// out-of-range or dividing by zero is UB; `/= 0` no-ops, `a / 0` skips the
// check). Includes `snap` so notch-incompatible assigns compile.
namespace detail {
inline constexpr policy_flag unsafe_marker{1ull << 36};
// Marks a cursor (`cursor<T, Step>`): default construction starts at Lower,
// and `end(t)` names its past-the-end value. Results never carry it.
inline constexpr policy_flag cursor_marker{1ull << 50};
} // namespace detail
inline constexpr policy_flag unsafe{detail::unsafe_marker | ignore_range | snap | ignore_zero};

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

// Runtime checks run unless the policy opts out with `unsafe`; an explicit
// `checked` wins over `unsafe`. So `inside<G, round_nearest>` and
// `inside<G, clamp>` are checked, exactly like the default `inside<G>`.
[[nodiscard]] constexpr bool is_checked(policy_flag set) noexcept {
    return has_flag(set, checked) || !has_flag(set, detail::unsafe_marker);
}

// Whether an out-of-range value does anything under a flag set: a clamp or
// wrap bit stores it, a checked range reports it. False under `unsafe`.
[[nodiscard]] constexpr bool range_handled(policy_flag set) noexcept {
    return has_any_flag(set, clamp | wrap) || (is_checked(set) && !has_flag(set, ignore_range));
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
// no_action — zero-overhead default for overflow callbacks
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
template <typename F>
struct on_overflow_t {
    [[no_unique_address]] F Fn;
};

//---------------------------------------------------------------------------
// CTAD-style factories — drop the on_overflow_t{lambda} brace-init.
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
template <typename F>
[[nodiscard]] constexpr auto on_overflow(F&& fn) {
    return on_overflow_t<std::remove_cvref_t<F>>{std::forward<F>(fn)};
}

namespace detail {
// Action detection: the `is_*_action` trait is the primary detector; the concept
// derives from it and strips cvref so the ref form matches the value form.
template <typename T>
struct is_clamp_action : std::false_type {};
template <typename F>
struct is_clamp_action<on_clamp_t<F>> : std::true_type {};
template <typename T>
struct is_wrap_action : std::false_type {};
template <typename F>
struct is_wrap_action<on_wrap_t<F>> : std::true_type {};
template <typename T>
struct is_error_action : std::false_type {};
template <typename F>
struct is_error_action<on_error_t<F>> : std::true_type {};
template <typename T>
struct is_overflow_action : std::false_type {};
template <typename F>
struct is_overflow_action<on_overflow_t<F>> : std::true_type {};

template <typename T>
concept clamp_action = is_clamp_action<std::remove_cvref_t<T>>::value;
template <typename T>
concept wrap_action = is_wrap_action<std::remove_cvref_t<T>>::value;
template <typename T>
concept error_action = is_error_action<std::remove_cvref_t<T>>::value;
template <typename T>
concept overflow_action = is_overflow_action<std::remove_cvref_t<T>>::value;

//---------------------------------------------------------------------------
// implied_flags<A> — single source of truth for "this action requires these
// policy bits". Used by inside::on_* and the action-first free-fn overloads.
//---------------------------------------------------------------------------
template <typename T>
inline constexpr policy_flag implied_flags = none;
template <typename F>
inline constexpr policy_flag implied_flags<on_clamp_t<F>> = clamp;
template <typename F>
inline constexpr policy_flag implied_flags<on_wrap_t<F>> = wrap;
template <typename F>
inline constexpr policy_flag implied_flags<on_error_t<F>> = checked;
template <typename F>
inline constexpr policy_flag implied_flags<on_overflow_t<F>> = checked;

//---------------------------------------------------------------------------
// Pack helpers — let policy_ref/assignment/arithmetic accept Actions... packs.
// The `is_*_action` traits are passed as template-template parameters.
//---------------------------------------------------------------------------

// True if any element of the pack matches the trait.
template <template <typename> class Trait, typename... As>
inline constexpr bool has_action = (Trait<std::remove_cvref_t<As>>::value || ... || false);

// How many pack elements match.
template <template <typename> class Trait, typename... As>
inline constexpr unsigned count_action_matches = (0u + ... + (Trait<std::remove_cvref_t<As>>::value ? 1u : 0u));

// OR of implied_flags<plain_t<A>> across the pack.
template <typename... As>
inline constexpr policy_flag merged_implied_flags = (none | ... | implied_flags<std::remove_cvref_t<As>>);

// pick_action<Trait>(actions...) returns a reference to the first pack element
// matching the trait, or a static `no_action` fallback if none does. Conflict
// diagnostics elsewhere ensure at most one match.
template <template <typename> class Trait>
inline no_action& pick_action_fallback() {
    static no_action n;
    return n;
}

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
        return pick_action_fallback<Trait>();
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
