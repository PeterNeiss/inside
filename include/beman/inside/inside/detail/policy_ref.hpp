// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_INSIDE_DETAIL_POLICY_REF_HPP
#define BEMAN_INSIDE_INSIDE_DETAIL_POLICY_REF_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/inside/detail/assignment.hpp>
#include <beman/inside/inside/detail/call_policy.hpp>
#include <beman/inside/inside/detail/lift.hpp>

#include <expected>
#include <tuple>
#include <utility>

//---------------------------------------------------------------------------
// policy_ref and policy_buffer — the proxies that b.policy<F>(), b.policy(ec)
// and on_*() return (inside/detail/core.hpp): an inside (by
// reference, or owned for a temporary) with a call_policy<...> and a tuple of on_*
// actions. Stores and compound ops flow through them, routing each action to
// the right callback at the right pipeline stage.
//---------------------------------------------------------------------------
namespace beman::inside {
//---------------------------------------------------------------------------
// policy_ref — variadic in actions (stores std::tuple<As...>; an on_* hook
// gives one, a policy override none). policy_ref pre-picks the matching
// action per call path, so assignment keeps its single-A signature.
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
// policy_buffer. A failed store (errc&, on_error) leaves it at its default
// value (0, or Lower), not garbage.
template <insidable Target, numeric C, typename P, typename... As>
constexpr Target dispatch_new(const C& src, P& policy, std::tuple<As...>& actions) {
    Target r{};
    dispatch_assign(r, src, policy, actions);
    return r;
}

// policy_buffer — the rvalue-receiver sibling of policy_ref. `policy<F>()`
// on a *temporary* return this instead: it OWNS the inside by value (the temporary
// is moved in), so the snapped value can be returned/stored without dangling —
// `auto square(small n){ return (n*n).policy<round_toward_zero>(); }` is safe. Value read-out only
// (no operator=: assigning into a throwaway is meaningless). Same constrained
// conversion as policy_ref, so it stays SFINAE-friendly.
// A proxy store of a C into B is proven by the grids under the proxy's flags,
// or has a channel: an errc& policy or an on_error hook.
template <insidable B, typename C, typename P, typename... As>
inline constexpr bool proxy_store_ok =
    store_proven<B, C, flags_of<B> | flags_of<P>> || reports_into_errc<P> || has_action<is_error_action, As...>;

#define BEMAN_INSIDE_WHY_proxy_unproven                                                                         \
    "inside: the value may fall outside the target grid, or off its notch, at run time - add clamp, wrap or a " \
    "rounding mode to the policy, report with policy(ec) or on_error(f), or opt out with unsafe"

// A plain number that a proxy's grids cannot prove (a double, a rational, an
// int off the lattice): a constant, stored once while compiling under the
// proxy's flags F into B's grid, so a store that would fail is a compile
// error; at run time it is stored again with the proxy's hooks, which fire
// (an on_clamp hears its overshoot). A run-time value does not compile: it
// needs a report channel (policy(ec), on_error).
template <insidable B, policy_flag F>
struct constant_value {
    bool        Floating = false;
    long double Float    = 0; // a float, double or long double, exactly
    rational    Exact{};      // an integer or a rational

    template <std::floating_point X>
        requires inside_assignable<inside<grid_of<B>, F>, X, F>
    consteval constant_value(X x) : Floating{true}, Float{x} {
        trial(x);
    }
    template <class X>
        requires(std::integral<X> || std::same_as<X, rational>) && inside_assignable<inside<grid_of<B>, F>, X, F>
    consteval constant_value(const X& x) : Exact{x} {
        trial(x);
    }

    template <class Store>
    constexpr decltype(auto) visit(Store store) const {
        return Floating ? store(Float) : store(Exact);
    }

  private:
    template <class X>
    static consteval void trial(const X& x) {
        inside<grid_of<B>, F> twin;
        assignment<inside<grid_of<B>, F>, X>::assign(twin, x, make_policy<F>());
    }
};

template <insidable B, typename P, typename... As>
struct policy_buffer {
    B                                       Owned;
    P                                       Policy;
    [[no_unique_address]] std::tuple<As...> Actions;

    template <insidable Target>
        requires inside_assignable<Target, B, flags_of<Target> | flags_of<P>> && proxy_store_ok<Target, B, P, As...>
    constexpr operator Target() {
        return dispatch_new<Target>(Owned, Policy, Actions);
    }
};

template <insidable B, typename P, typename... As>
struct policy_ref {
    // (At most one action: each on_* hook makes a ref with its own.)
    B& Ref;
    P  Policy;
    // `[[no_unique_address]]` is load-bearing: each captureless action lambda
    // is an empty type, and without this attribute the tuple would pad each
    // one out to a byte. With it, `policy_ref<B, P>` carrying no actions has
    // the same size as `policy_ref<B, P, no_action>`.
    [[no_unique_address]] std::tuple<As...> Actions;

    // Stores go through dispatch_assign under this ref's Policy and actions:
    // into Ref for `operator=`, into a fresh target for the conversion below,
    // so a one-shot snap can be read out as a value (`(a * b).policy<round_toward_zero>()`).
    template <numeric C>
        requires proxy_store_ok<B, C, P, As...>
    constexpr B& operator=(const C& other) {
        return dispatch_assign(Ref, other, Policy, Actions);
    }
    // A constant the grids cannot prove: checked while compiling (above).
    constexpr B& operator=(const constant_value<B, flags_of<B> | plain_t<P>::Flags>& c) {
        return c.visit([&](const auto& v) -> B& { return dispatch_assign(Ref, v, Policy, Actions); });
    }
    template <insidable C>
        requires(!proxy_store_ok<B, C, P, As...>)
    B& operator=(const C&) = delete (BEMAN_INSIDE_WHY_proxy_unproven);

    // Value read-out: a one-shot policy ref converts to any inside the assignment
    // could satisfy, applying the target's own policy (range) plus this ref's
    // carried flags (notch/rounding) via uses_flag's merge. Makes
    // `Target t = (a * b).policy<round_toward_zero>();` / `return (a * b).policy<round_toward_zero>();` compile.
    // Constrained so the proxy stays SFINAE-friendly (no over-broad convertibility).
    template <insidable Target>
        requires inside_assignable<Target, B, flags_of<Target> | flags_of<P>> && proxy_store_ok<Target, B, P, As...>
    constexpr operator Target() {
        return dispatch_new<Target>(Ref, Policy, Actions);
    }

    // A std::expected is never stored implicitly, through a proxy either:
    // check it and store *e, or use B::try_make(e).
    template <numeric C>
    B& operator=(const std::expected<C, errc>&) = delete ("inside: a std::expected is not stored implicitly - "
                                                          "check it and store *e, or use T::try_make(e), "
                                                          "which passes its error on");

  private:
    // The value an op's result holds (an expected's value type) assigns back.
    template <typename R>
    static constexpr bool stores_back = inside_assignable<B, detail::unwrap_t<R>, flags_of<B> | flags_of<P>> &&
                                        proxy_store_ok<B, detail::unwrap_t<R>, P, As...>;
    // A divisor whose grid holds 0 needs a channel for the zero, or unsafe.
    template <insidable C>
    static constexpr bool divisor_ok =
        reports_into_errc<P> || has_action<is_error_action, As...> ||
        !is_checked(flags_of<B> | plain_t<P>::Flags | flags_of<C>) || !grid_of<C>.representable(grid_rational{0});

  public:
    //-------------------------------------------------------------------------
    // insidable RHS overloads — route through the inside's arithmetic, then
    // assign via dispatch_assign so callbacks fire on the narrowing back to B.
    // An expected<inside> result carrying an error (a zero divisor) surfaces
    // its errc through on_error if registered, else the policy. Constrained on the free op they call and on storing
    // its result back under this policy, so a requires probe sees what compiles: a 1/16 sum does not store into an
    // integer grid without snap.
    //-------------------------------------------------------------------------
    template <insidable C>
        requires requires(const B& b, const C& c, P& p) { add(b, c, p); } &&
                 stores_back<decltype(add(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))>
    constexpr B& operator+=(const C& rhs) {
        return finalize_arith(add(Ref, rhs, Policy));
    }

    template <insidable C>
        requires requires(const B& b, const C& c, P& p) { sub(b, c, p); } &&
                 stores_back<decltype(sub(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))>
    constexpr B& operator-=(const C& rhs) {
        return finalize_arith(sub(Ref, rhs, Policy));
    }

    template <insidable C>
        requires requires(const B& b, const C& c, P& p) { mul(b, c, p); } &&
                 stores_back<decltype(mul(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))>
    constexpr B& operator*=(const C& rhs) {
        return finalize_arith(mul(Ref, rhs, Policy));
    }

    // A zero divisor is reported (on_error, unless `unsafe` appears) and leaves
    // Ref unchanged, like inside::operator/=: div/mod under `unsafe` skip their
    // own check, so dividing here would be a division by zero.
    template <insidable C>
        requires requires(const B& b, const C& c, P& p) { div(b, c, p); } &&
                 stores_back<decltype(div(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))> &&
                 divisor_ok<C>
    constexpr B& operator/=(const C& rhs) {
        if (rhs == 0) {
            if constexpr (is_checked(flags_of<C>)) // either operand silences it
                report_error(errc::division_by_zero, "policy_ref::operator/= division by zero");
            return Ref;
        }
        return finalize_arith(div(Ref, rhs, Policy));
    }

    template <insidable C>
        requires requires(const B& b, const C& c, P& p) { mod(b, c, p); } &&
                 stores_back<decltype(mod(std::declval<const B&>(), std::declval<const C&>(), std::declval<P&>()))> &&
                 divisor_ok<C>
    constexpr B& operator%=(const C& rhs) {
        if (rhs == 0) {
            if constexpr (is_checked(flags_of<C>)) // either operand silences it
                report_error(errc::division_by_zero, "policy_ref::operator%= division by zero");
            return Ref;
        }
        return finalize_arith(mod(Ref, rhs, Policy));
    }

    // A rational has no grid: as on the inside itself, a compound assign
    // takes an inside (0.5_ins, just<1, 2>).
#define BEMAN_INSIDE_NO_RATIONAL_ASSIGN(op)                                                                \
    template <std::same_as<rational> C>                                                                    \
    B& operator op(const C&) = delete ("an inside cannot be combined with a rational: write the constant " \
                                       "as just<1, 2> or 0.5_ins (a grid), or store the rational into an " \
                                       "inside first");
    BEMAN_INSIDE_NO_RATIONAL_ASSIGN(+=)
    BEMAN_INSIDE_NO_RATIONAL_ASSIGN(-=)
    BEMAN_INSIDE_NO_RATIONAL_ASSIGN(*=)
    BEMAN_INSIDE_NO_RATIONAL_ASSIGN(/=)
    BEMAN_INSIDE_NO_RATIONAL_ASSIGN(%=)
#undef BEMAN_INSIDE_NO_RATIONAL_ASSIGN

  private:
    // A failed op leaves Ref unchanged: on_error takes the failure, else the
    // policy reports it — but `unsafe` skips a zero divisor.
    constexpr void report_error(errc code, const char* what) {
        if constexpr (has_action<is_error_action, As...>)
            pick_action_in<is_error_action>(Actions).Fn(Ref, code, what);
        else if (code != errc::division_by_zero || is_checked(flags_of<B> | plain_t<P>::Flags))
            Policy.report(code);
    }

    // An expected result (/ and % by a grid that holds 0) holds a value here:
    // its only error is the zero divisor, which /= and %= report first.
    template <typename R>
    constexpr B& finalize_arith(R&& result) {
        if constexpr (expected_like<R>)
            return dispatch_assign(Ref, *result, Policy, Actions);
        else
            return dispatch_assign(Ref, std::forward<R>(result), Policy, Actions);
    }
};
#undef BEMAN_INSIDE_WHY_proxy_unproven
} // namespace detail
} // namespace beman::inside

#endif // BEMAN_INSIDE_INSIDE_DETAIL_POLICY_REF_HPP
