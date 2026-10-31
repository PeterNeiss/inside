// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_POLICY_HPP
#define BEMAN_INSIDE_POLICY_HPP

#include <beman/inside/detail/config.hpp>

#include <beman/inside/detail/error.hpp>         // errc, raise, constexpr_error
#include <beman/inside/detail/introspection.hpp> // insidable, flags_of_impl
#include <beman/inside/policy_flag.hpp>

#include <expected>
#include <type_traits>

//---------------------------------------------------------------------------
// policy — the runtime policy carrier.
//   detail::call_policy<F, E> — compile-time flags F plus an optional error_ref E holding a
//                   beman::inside::errc& (EBO so the no-error-code form is zero-sized).
//   make_policy<F>(), and as_call_policy, which turns a named flag passed as a
//                   call's policy (div(a, b, round_floor)) into make_policy<F>().
// The proxies that b.policy(), with_snap() and on_*() return are in
// detail/policy_ref.hpp.
//---------------------------------------------------------------------------
namespace beman::inside {
//---------------------------------------------------------------------------
// call_policy — the policy of one call or store, what make_policy returns.
// Derives from E for EBO: throwing form (E == empty_ref) is zero-sized;
// `call_policy(ec)` makes E == error_ref carrying a `beman::inside::errc&`. No
// virtuals, resolved at compile time. (The error-code channel reports `errc`
// directly — there is no <system_error> dependency.) Users spell it through
// make_policy<F>() / make_policy<F>(ec), or pass a flag.
//---------------------------------------------------------------------------
namespace detail {
struct empty_ref {};
struct error_ref {
    constexpr error_ref(errc& ec) : Code{ec} {}
    errc& Code;
};

template <policy_flag F = none, typename E = empty_ref>
struct call_policy : E {
    constexpr call_policy() = default;
    constexpr call_policy(errc& ec)
        requires std::same_as<E, error_ref>
        : E(ec) {}

    static constexpr policy_flag Flags = F;

    static constexpr bool test(policy_flag flag) { return has_flag(F, flag); }

    static constexpr bool checks_range() {
        if consteval {
            return true;
        }
        return is_checked(F);
    }

    static constexpr bool checks_rounding() {
        if consteval {
            return true;
        }
        return is_checked(F) && not test(snap);
    }

    // Cheap default report: no message construction. error_ref mode records the
    // code (sticky: keeps the first error) — also during constant evaluation, so
    // try_make / parse / tan_into return error values in constexpr code.
    // Throw mode funnels through the installed handler via an outlined cold
    // helper; at compile time it throws, so the build fails with a text that
    // names the code and the fix.
    constexpr void report(errc code) const {
        if constexpr (std::is_same_v<E, error_ref>)
            E::Code = E::Code != errc{} ? E::Code : code;
        else {
            if consteval {
                raise(code, store_failure_message(code));
            }
            raise(code);
        }
    }
};

call_policy(errc&) -> call_policy<none, error_ref>;
} // namespace detail

//---------------------------------------------------------------------------
// is_policy_v — true for call_policy<F,E> specializations, false otherwise.
// Used to gate free-fn overloads so they don't accidentally bind P = action tag.
//---------------------------------------------------------------------------
namespace detail {
template <typename T>
inline constexpr bool is_policy_v = false;
template <policy_flag F, typename E>
inline constexpr bool is_policy_v<call_policy<F, E>> = true;

// Concept form of is_policy_v — pulls cvref off so the constraint matches
// forwarded `call_policy<F,E>` references in template parameters.
template <typename T>
concept policy_like = is_policy_v<std::remove_cvref_t<T>>;

// flags_of<call_policy<F, E>> is F. Lets the value+policy constructor and
// policy_ref's conversion fold the per-call flags into their
// `inside_assignable` check, so a one-shot clamp/round actually relaxes the
// constraint it enables.
template <policy_flag F, typename E>
inline constexpr policy_flag flags_of_impl<call_policy<F, E>> = F;

// True for policy specializations that carry a beman::inside::errc& reference.
// Free-fn arithmetic uses this to decide whether to call policy.report on
// failure (which sets ec) vs. returning a silent std::unexpected (no-arg form).
template <typename T>
inline constexpr bool uses_error_ref = false;
template <policy_flag F>
inline constexpr bool uses_error_ref<call_policy<F, error_ref>> = true;
} // namespace detail

//---------------------------------------------------------------------------
// make_policy
//---------------------------------------------------------------------------
template <policy_flag F = none>
[[nodiscard]] constexpr auto make_policy() {
    return detail::call_policy<F, detail::empty_ref>{};
}

template <policy_flag F = none>
[[nodiscard]] constexpr auto make_policy(errc& ec) {
    return detail::call_policy<F, detail::error_ref>{ec};
}

//---------------------------------------------------------------------------
// report_or_unexpected — the zero-divisor result of division and modulo:
// an errc& policy (uses_error_ref<P>) is set too; the error is returned.
//---------------------------------------------------------------------------
namespace detail {
template <insidable Result, typename P>
constexpr std::expected<Result, errc> report_or_unexpected(P&& policy, errc code) {
    if constexpr (uses_error_ref<std::remove_cvref_t<P>>)
        policy.report(code);
    return std::unexpected{code};
}
} // namespace detail

//---------------------------------------------------------------------------
// as_call_policy — a call's trailing policy argument as a policy object: a
// named flag (div(a, b, round_floor), B{v, round_floor}) is make_policy<F>();
// a policy passes through.
//---------------------------------------------------------------------------
namespace detail {
template <class Pol>
constexpr decltype(auto) as_call_policy(Pol&& pol) {
    if constexpr (flag_like<Pol>)
        return make_policy<std::remove_cvref_t<Pol>::value>();
    else
        return std::forward<Pol>(pol);
}
} // namespace detail
} // namespace beman::inside

#endif // BEMAN_INSIDE_POLICY_HPP
