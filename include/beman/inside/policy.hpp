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
//   policy<F, E>  — compile-time flags F plus an optional error_ref E holding a
//                   beman::inside::errc& (EBO so the no-error-code form is zero-sized).
//   make_policy<F>() and the named policies rounded_toward_zero, rounded_nearest,
//                   rounded_floor, rounded_ceil, rounded_half_even, clamped, wrapped.
// The proxies that b.policy(), with_snap() and on_*() return are in
// detail/policy_ref.hpp.
//---------------------------------------------------------------------------
namespace beman::inside {
//---------------------------------------------------------------------------
// policy — derives from E for EBO: throwing form (E == empty_ref) is zero-sized;
// `policy(ec)` makes E == error_ref carrying a `beman::inside::errc&`. No virtuals,
// resolved at compile time. (The error-code channel reports `errc` directly —
// there is no <system_error> dependency.)
//---------------------------------------------------------------------------
namespace detail {
struct empty_ref {};
struct error_ref {
    constexpr error_ref(errc& ec) : Code{ec} {}
    errc& Code;
};
} // namespace detail

template <policy_flag F = none, typename E = detail::empty_ref>
struct policy : E {
    constexpr policy() = default;
    constexpr policy(errc& ec)
        requires std::same_as<E, detail::error_ref>
        : E(ec) {}

    static constexpr policy_flag Flags = F;

    static constexpr bool test(policy_flag flag) { return has_flag(F, flag); }

    static constexpr bool checks_range() {
        if consteval {
            return true;
        }
        return is_checked(F) && not test(ignore_range);
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
        if constexpr (std::is_same_v<E, detail::error_ref>)
            E::Code = E::Code != errc{} ? E::Code : code;
        else {
            if consteval {
                detail::raise(code, detail::store_failure_message(code));
            }
            detail::raise(code);
        }
    }
};

policy(errc&) -> policy<none, detail::error_ref>;

//---------------------------------------------------------------------------
// is_policy_v — true for policy<F,E> specializations, false otherwise.
// Used to gate free-fn overloads so they don't accidentally bind P = action tag.
//---------------------------------------------------------------------------
namespace detail {
template <typename T>
inline constexpr bool is_policy_v = false;
template <policy_flag F, typename E>
inline constexpr bool is_policy_v<policy<F, E>> = true;

// Concept form of is_policy_v — pulls cvref off so the constraint matches
// forwarded `policy<F,E>` references in template parameters.
template <typename T>
concept policy_like = is_policy_v<std::remove_cvref_t<T>>;

// flags_of<policy<F, E>> is F. Lets the value+policy constructor and
// policy_ref's conversion fold the per-call flags into their
// `inside_assignable` check, so a one-shot clamp/round actually relaxes the
// constraint it enables.
template <policy_flag F, typename E>
inline constexpr policy_flag flags_of_impl<policy<F, E>> = F;

// True for policy specializations that carry a beman::inside::errc& reference.
// Free-fn arithmetic uses this to decide whether to call policy.report on
// failure (which sets ec) vs. returning a silent std::unexpected (no-arg form).
template <typename T>
inline constexpr bool uses_error_ref = false;
template <policy_flag F>
inline constexpr bool uses_error_ref<policy<F, error_ref>> = true;
} // namespace detail

//---------------------------------------------------------------------------
// make_policy
//---------------------------------------------------------------------------
template <policy_flag F = none>
[[nodiscard]] constexpr auto make_policy() {
    return policy<F, detail::empty_ref>{};
}

template <policy_flag F = none>
[[nodiscard]] constexpr auto make_policy(errc& ec) {
    return policy<F, detail::error_ref>{ec};
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
// Named convenience policies — let user code skip `make_policy<F>()` entirely
// for the per-call flag form on free arithmetic functions.
//---------------------------------------------------------------------------
// Each is named after its flag: rounded_* = round_*, clamped = clamp,
// wrapped = wrap.
inline constexpr auto rounded_toward_zero = make_policy<round_toward_zero>();
inline constexpr auto rounded_nearest     = make_policy<round_nearest>();
inline constexpr auto rounded_floor       = make_policy<round_floor>();
inline constexpr auto rounded_ceil        = make_policy<round_ceil>();
inline constexpr auto rounded_half_even   = make_policy<round_half_even>();
inline constexpr auto clamped             = make_policy<clamp>();
inline constexpr auto wrapped             = make_policy<wrap>();
} // namespace beman::inside

#endif // BEMAN_INSIDE_POLICY_HPP
