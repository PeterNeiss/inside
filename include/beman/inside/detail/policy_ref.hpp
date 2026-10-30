// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_POLICY_REF_HPP
#define BEMAN_INSIDE_DETAIL_POLICY_REF_HPP

#include <beman/inside/detail/config.hpp>

#include <beman/inside/detail/assignment.hpp>
#include <beman/inside/policy.hpp>

#include <expected>
#include <tuple>
#include <utility>

//---------------------------------------------------------------------------
// policy_ref and policy_buffer — the proxies that b.policy(), with_snap(),
// with_clamp(), with_wrap() and on_*() return (core.hpp): an inside (by
// reference, or owned for a temporary) with a policy<...> and a tuple of on_*
// actions. Stores and compound ops flow through them, routing each action to
// the right callback at the right pipeline stage.
//---------------------------------------------------------------------------
namespace beman::inside {
//---------------------------------------------------------------------------
// policy_ref — variadic in actions (stores std::tuple<As...>). policy_ref
// pre-picks the matching action per call path, so assignment/arithmetic keep
// their single-A signatures. Payoff: the arithmetic and narrowing stages of a
// compound op can each fire a different action (e.g. on_overflow + on_clamp).
//---------------------------------------------------------------------------
namespace detail {
// The single assignment-time action among `actions` (at most one such tag is
// present), or no_action.
template <typename... As>
constexpr decltype(auto) assignment_action(std::tuple<As...>& actions) {
    if constexpr (has_action<is_clamp_action, As...>)
        return pick_action_in<is_clamp_action>(actions);
    else if constexpr (has_action<is_wrap_action, As...>)
        return pick_action_in<is_wrap_action>(actions);
    else if constexpr (has_action<is_error_action, As...>)
        return pick_action_in<is_error_action>(actions);
    else
        return no_action{};
}

// Shared assignment dispatch: store `src` into `dst` under `policy` + that
// action. Backs both policy_ref (dst = the wrapped inside) and policy_buffer
// (dst = a fresh target), so the conversion/assignment logic lives in exactly
// one place.
template <insidable Dst, numeric C, typename P, typename... As>
constexpr Dst& dispatch_assign(Dst& dst, const C& src, P& policy, std::tuple<As...>& actions) {
    return assignment<Dst, C>::assign(dst, src, policy, assignment_action(actions));
}

// The same into a fresh Target: the value read-out of policy_ref and
// policy_buffer. A failed store (errc&, on_error) leaves it value-initialized,
// not garbage.
template <insidable Target, numeric C, typename P, typename... As>
constexpr Target dispatch_new(const C& src, P& policy, std::tuple<As...>& actions) {
    Target r{};
    dispatch_assign(r, src, policy, actions);
    return r;
}

// policy_buffer — the rvalue-receiver sibling of policy_ref. `with_snap()` etc.
// on a *temporary* return this instead: it OWNS the inside by value (the temporary
// is moved in), so the snapped value can be returned/stored without dangling —
// `auto square(small n){ return (n*n).with_snap(); }` is safe. Value read-out only
// (no operator=: assigning into a throwaway is meaningless). Same constrained
// conversion as policy_ref, so it stays SFINAE-friendly.
template <insidable B, typename P, typename... As>
struct policy_buffer {
    B                                       Owned;
    P                                       Policy;
    [[no_unique_address]] std::tuple<As...> Actions;

    template <insidable Target>
        requires inside_assignable<Target, B, flags_of<Target> | flags_of<P>>
    constexpr operator Target() {
        return dispatch_new<Target>(Owned, Policy, Actions);
    }
};

template <insidable B, typename P, typename... As>
struct policy_ref {
  private:
    // Conflict diagnostics: at most one assignment-time tag (clamp / wrap /
    // error), at most one of each kind, no clamp+wrap.
    static constexpr unsigned ClampCount    = count_action_matches<is_clamp_action, As...>;
    static constexpr unsigned WrapCount     = count_action_matches<is_wrap_action, As...>;
    static constexpr unsigned ErrorCount    = count_action_matches<is_error_action, As...>;
    static constexpr unsigned OverflowCount = count_action_matches<is_overflow_action, As...>;

    static_assert(ClampCount + WrapCount + ErrorCount <= 1,
                  "on_clamp / on_wrap / on_error are mutually exclusive in a single policy_ref");
    static_assert(ClampCount <= 1, "duplicate on_clamp");
    static_assert(WrapCount <= 1, "duplicate on_wrap");
    static_assert(ErrorCount <= 1, "duplicate on_error");
    static_assert(OverflowCount <= 1, "duplicate on_overflow");

  public:
    B& Ref;
    P  Policy;
    // `[[no_unique_address]]` is load-bearing: each captureless action lambda
    // is an empty type, and without this attribute the tuple would pad each
    // one out to a byte. With it, `policy_ref<B, P>` carrying no actions has
    // the same size as `policy_ref<B, P, no_action>`.
    [[no_unique_address]] std::tuple<As...> Actions;

    // Stores go through dispatch_assign under this ref's Policy and actions:
    // into Ref for `operator=`, into a fresh target for the conversion below,
    // so a one-shot snap can be read out as a value (`(a * b).with_snap()`).
    template <numeric C>
    constexpr B& operator=(const C& other) {
        return dispatch_assign(Ref, other, Policy, Actions);
    }

    // Value read-out: a one-shot policy ref converts to any inside the assignment
    // could satisfy, applying the target's own policy (range) plus this ref's
    // carried flags (notch/rounding) via uses_flag's merge. Makes
    // `Target t = (a * b).with_snap();` / `return (a * b).with_snap();` compile.
    // Constrained so the proxy stays SFINAE-friendly (no over-broad convertibility).
    template <insidable Target>
        requires inside_assignable<Target, B, flags_of<Target> | flags_of<P>>
    constexpr operator Target() {
        return dispatch_new<Target>(Ref, Policy, Actions);
    }

    // expected<C> sink — unwrap once at the proxy boundary so callers can chain
    // checked arithmetic into `.with_clamp() = ...` without per-step `.value()`.
    // An error goes through this ref's policy (on_overflow / on_error, or the
    // errc& sink), as an arithmetic result's does.
    template <numeric C>
    constexpr B& operator=(const std::expected<C, errc>& other) {
        return finalize_arith(other, "policy_ref::operator= of an expected without a value");
    }

  private:
    // The value an op's result holds (an expected's value type) assigns back.
    template <typename R>
    static constexpr bool stores_back = inside_assignable<B, detail::unwrap_t<R>, flags_of<B> | flags_of<P>>;

  public:
    //-------------------------------------------------------------------------
    // insidable RHS overloads — route through the inside's arithmetic, then
    // assign via dispatch_assign so callbacks fire on the narrowing back to B.
    // An expected<inside> result carrying an error (rational-raw overflow,
    // division by zero) surfaces its errc through on_overflow if registered,
    // else report. Constrained on the free op they call and on storing its
    // result back under this policy, so a requires probe sees what compiles:
    // a 1/16 sum does not store into an integer grid without snap.
    //-------------------------------------------------------------------------
    template <insidable C>
        requires requires(const B& b, const C& c, P& p) { add(b, c, p); } &&
                 stores_back<decltype(add(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))>
    constexpr B& operator+=(const C& rhs) {
        return finalize_arith(add(Ref, rhs, Policy), "policy_ref::operator+= overflow");
    }

    template <insidable C>
        requires requires(const B& b, const C& c, P& p) { sub(b, c, p); } &&
                 stores_back<decltype(sub(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))>
    constexpr B& operator-=(const C& rhs) {
        return finalize_arith(sub(Ref, rhs, Policy), "policy_ref::operator-= overflow");
    }

    template <insidable C>
        requires requires(const B& b, const C& c, P& p) { mul(b, c, p); } &&
                 stores_back<decltype(mul(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))>
    constexpr B& operator*=(const C& rhs) {
        return finalize_arith(mul(Ref, rhs, Policy), "policy_ref::operator*= overflow");
    }

    // A zero divisor is reported (on_error / ignore_zero) and leaves Ref
    // unchanged, like inside::operator/=: div/mod under ignore_zero skip their
    // own check, so dividing here would be a division by zero.
    template <insidable C>
        requires requires(const B& b, const C& c, P& p) { div(b, c, p); } &&
                 stores_back<decltype(div(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))>
    constexpr B& operator/=(const C& rhs) {
        if (rhs == 0) {
            if constexpr (!has_flag(flags_of<C>, ignore_zero)) // either operand silences it
                report_zero(errc::division_by_zero, "policy_ref::operator/= division by zero");
            return Ref;
        }
        return finalize_arith(div(Ref, rhs, Policy), "policy_ref::operator/= division/overflow");
    }

    template <insidable C>
        requires requires(const B& b, const C& c, P& p) { mod(b, c, p); } &&
                 stores_back<decltype(mod(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))>
    constexpr B& operator%=(const C& rhs) {
        if (rhs == 0) {
            if constexpr (!has_flag(flags_of<C>, ignore_zero)) // either operand silences it
                report_zero(errc::division_by_zero, "policy_ref::operator%= division by zero");
            return Ref;
        }
        return finalize_arith(mod(Ref, rhs, Policy), "policy_ref::operator%= division/overflow");
    }

    //-------------------------------------------------------------------------
    // rational RHS overloads — the only non-inside operand a compound assign
    // accepts. Lets callers write `b += rational{1,3}`. Raw int/float/double are
    // ill-formed: give the scalar a grid (`1_ins` / `just<1>` / `inside<{lo,hi}>{n}`).
    // Ref as a rational, checked op, finalize_arith.
    //-------------------------------------------------------------------------
    template <std::same_as<rational> C>
    constexpr B& operator+=(const C& rhs) {
        return finalize_arith(rational{Ref} + rhs, "policy_ref::operator+= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator-=(const C& rhs) {
        return finalize_arith(rational{Ref} - rhs, "policy_ref::operator-= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator*=(const C& rhs) {
        return finalize_arith(rational{Ref} * rhs, "policy_ref::operator*= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator/=(const C& rhs) {
        if (is_canonical_zero(rhs)) {
            report_zero(errc::division_by_zero, "policy_ref::operator/= division by zero");
            return Ref;
        }
        return finalize_arith(rational{Ref} / rhs, "policy_ref::operator/= division/overflow");
    }

  private:
    constexpr void report_zero(errc code, const char* what) {
        if constexpr (has_action<is_error_action, As...>)
            pick_action_in<is_error_action>(Actions).Fn(Ref, code, what);
        else if constexpr (!uses_flag<B, P, ignore_zero>)
            Policy.report(code);
    }

    template <typename R>
    constexpr B& finalize_arith(R&& result, [[maybe_unused]] const char* msg) {
        if constexpr (expected_like<R>) {
            if (!result.has_value()) [[unlikely]] {
                if (result.error() == errc::division_by_zero)
                    report_zero(errc::division_by_zero, msg); // on_error / ignore_zero, like rational /=
                else if constexpr (has_action<is_overflow_action, As...>)
                    pick_action_in<is_overflow_action>(Actions).Fn(Ref, result.error());
                else
                    Policy.report(result.error());
                return Ref;
            }
            return dispatch_assign(Ref, result.value(), Policy, Actions);
        } else
            return dispatch_assign(Ref, std::forward<R>(result), Policy, Actions);
    }
};
} // namespace detail
} // namespace beman::inside

#endif // BEMAN_INSIDE_DETAIL_POLICY_REF_HPP
