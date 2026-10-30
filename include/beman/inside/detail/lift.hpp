// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_LIFT_HPP
#define BEMAN_INSIDE_DETAIL_LIFT_HPP

#include <beman/inside/detail/config.hpp>

#include <beman/inside/detail/error.hpp> // errc

#include <concepts>
#include <expected>
#include <type_traits>
#include <utility>

//---------------------------------------------------------------------------
// lift — monadic composition for `std::expected<T, errc>`. `lift(op, args...)`
// unwraps each expected arg, calls `op`, and re-wraps; the first arg (left to
// right) holding an error short-circuits with that error. An `op` already
// returning expected<R> is forwarded as-is, so the inner operation reports
// its own cause. Internal: it backs grid division and the expected-lift
// operators; user code chains through those.
//
// std::expected is larger than its value (flag + errc), so it only ever
// travels as a parameter or return value — never as stored state.
//---------------------------------------------------------------------------
namespace beman::inside::detail {
template <class T>
struct is_expected : std::false_type {};
template <class T, class E>
struct is_expected<std::expected<T, E>> : std::true_type {};

template <class T>
inline constexpr bool is_expected_v = is_expected<std::remove_cvref_t<T>>::value;

// Any std::expected specialization (cv/ref-stripped).
template <typename T>
concept expected_like = is_expected_v<T>;

// strip expected<X, E> down to X, leave non-expected unchanged
template <class T>
struct unwrap {
    using type = T;
};
template <class T, class E>
struct unwrap<std::expected<T, E>> {
    using type = T;
};
template <class T>
using unwrap_t = typename unwrap<std::remove_cvref_t<T>>::type;

template <class T>
constexpr decltype(auto) lift_unwrap(T&& v) {
    if constexpr (expected_like<T>)
        return *std::forward<T>(v);
    else
        return std::forward<T>(v);
}

// Copy an arg's error into `e`; true if the arg holds one.
template <class T>
constexpr bool lift_take_error([[maybe_unused]] const T& v, [[maybe_unused]] errc& e) {
    if constexpr (expected_like<T>)
        if (!v.has_value()) {
            e = v.error();
            return true;
        }
    return false;
}

template <class Op, class... Args>
[[nodiscard]] constexpr auto lift(Op op, Args&&... args) {
    using R   = std::remove_cvref_t<decltype(op(lift_unwrap(std::forward<Args>(args))...))>;
    using Ret = std::conditional_t<is_expected_v<R>, R, std::expected<R, errc>>;

    errc e{};
    if ((lift_take_error(args, e) || ...))
        return Ret{std::unexpected{e}};

    if constexpr (expected_like<R>)
        return op(lift_unwrap(std::forward<Args>(args))...);
    else
        return Ret{op(lift_unwrap(std::forward<Args>(args))...)};
}
} // namespace beman::inside::detail

#endif // BEMAN_INSIDE_DETAIL_LIFT_HPP
