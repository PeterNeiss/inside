// slim::expected — minimal C++20 backport of std::expected
// Copyright (c) 2026 Peter Neiss
// SPDX-License-Identifier: MIT
//
// An independent utility namespace, no `bnd::` dependency. Mirrors the subset of
// C++23 std::expected the library consumes (value/error access, `unexpected`,
// deref, throwing value()), giving one error-channel code path even on C++20
// toolchains lacking <expected>. Scope is deliberately small: T and E are
// trivially-copyable here, so storage is a flag-guarded pair, not a union. No
// expected<void, E>, no monadic ops.

#pragma once

#include <version>

#if defined(__cpp_lib_expected)
// C++23 toolchains: slim::expected IS std::expected — user code composes
// with the standard vocabulary (monadic ops included); the backport below
// serves only toolchains without <expected> (GCC 12 / BOUND_CXX20).
#include <expected>

namespace slim {
template<class T, class E> using expected   = std::expected<T, E>;
// Import the class template by name (not an alias template): call sites use CTAD
// — `unexpected(errc::…)` — and CTAD through an alias template (P1814) is not
// implemented by every C++23 toolchain (e.g. AppleClang 15).
using std::unexpected;
// std::bad_expected_access<E> derives from the <void> base, so catching this
// alias catches every instantiation — same role as the backport's type.
using bad_expected_access = std::bad_expected_access<void>;
} // namespace slim

#else // ── C++20 backport ─────────────────────────────────────────────────

#include <exception>
#include <type_traits>
#include <utility>

namespace slim {

// Thrown by expected::value() when the object holds an error. Mirrors
// std::bad_expected_access closely enough for the library's needs (the bound
// code only relies on "value() throws when empty").
class bad_expected_access : public std::exception {
    const char* msg_;
public:
    explicit bad_expected_access(const char* msg = "bad expected access") noexcept
        : msg_(msg) {}
    const char* what() const noexcept override { return msg_; }
};

// Throws when exceptions are enabled; otherwise traps — keeps value() usable
// under -fno-exceptions / freestanding.
namespace detail {
[[noreturn]] inline void throw_bad_expected_access(const char* msg) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_expected_access(msg);
#else
    (void)msg;
#  if defined(__GNUC__) || defined(__clang__)
    __builtin_trap();
#  else
    std::abort();
#  endif
#endif
}
} // namespace detail

// ── unexpected<E>: the error wrapper used to construct the error state ──
template<class E>
class unexpected {
    E error_;
public:
    constexpr explicit unexpected(const E& e) : error_(e) {}
    constexpr explicit unexpected(E&& e) : error_(std::move(e)) {}

    constexpr const E&  error() const&  noexcept { return error_; }
    constexpr E&        error() &       noexcept { return error_; }
    constexpr const E&& error() const&& noexcept { return std::move(error_); }
    constexpr E&&       error() &&      noexcept { return std::move(error_); }
};

// Deduction guide so `slim::unexpected{errc::overflow}` deduces E.
template<class E>
unexpected(E) -> unexpected<E>;

// ── expected<T, E> ──
template<class T, class E>
class expected {
    T value_{};
    E error_{};
    bool has_value_{true};

public:
    using value_type      = T;
    using error_type      = E;
    using unexpected_type = unexpected<E>;

    // Value state — implicit so `return some_T;` works at call sites.
    constexpr expected() = default;
    constexpr expected(const T& v) : value_(v), has_value_(true) {}
    constexpr expected(T&& v) : value_(std::move(v)), has_value_(true) {}

    // Value state from a convertible source (e.g. `return static_cast<T>(x);`
    // already yields T, but integral promotions at call sites benefit).
    template<class U = T>
        requires (!std::is_same_v<std::remove_cvref_t<U>, expected> &&
                  !std::is_same_v<std::remove_cvref_t<U>, unexpected<E>> &&
                  std::is_constructible_v<T, U> &&
                  std::is_convertible_v<U, T>)
    constexpr expected(U&& v) : value_(std::forward<U>(v)), has_value_(true) {}

    // Error state — implicit from slim::unexpected so `return slim::unexpected{e};`
    // works at call sites.
    constexpr expected(const unexpected<E>& u) : error_(u.error()), has_value_(false) {}
    constexpr expected(unexpected<E>&& u) : error_(std::move(u).error()), has_value_(false) {}

    // Observers
    [[nodiscard]] constexpr bool has_value() const noexcept { return has_value_; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return has_value_; }

    [[nodiscard]] constexpr const T& value() const& {
        if (!has_value_) detail::throw_bad_expected_access("expected has no value");
        return value_;
    }
    [[nodiscard]] constexpr T& value() & {
        if (!has_value_) detail::throw_bad_expected_access("expected has no value");
        return value_;
    }
    [[nodiscard]] constexpr T&& value() && {
        if (!has_value_) detail::throw_bad_expected_access("expected has no value");
        return std::move(value_);
    }

    [[nodiscard]] constexpr const E& error() const& noexcept { return error_; }
    [[nodiscard]] constexpr E&       error() &      noexcept { return error_; }
    [[nodiscard]] constexpr E&&      error() &&     noexcept { return std::move(error_); }

    [[nodiscard]] constexpr const T& operator*() const& noexcept { return value_; }
    [[nodiscard]] constexpr T&       operator*() &      noexcept { return value_; }
    [[nodiscard]] constexpr T&&      operator*() &&     noexcept { return std::move(value_); }

    [[nodiscard]] constexpr const T* operator->() const noexcept { return std::addressof(value_); }
    [[nodiscard]] constexpr T*       operator->()       noexcept { return std::addressof(value_); }

    template<class U>
        requires std::is_convertible_v<U, T>
    [[nodiscard]] constexpr T value_or(U&& default_value) const& {
        return has_value_ ? value_ : static_cast<T>(std::forward<U>(default_value));
    }
    template<class U>
        requires std::is_convertible_v<U, T>
    [[nodiscard]] constexpr T value_or(U&& default_value) && {
        return has_value_ ? std::move(value_) : static_cast<T>(std::forward<U>(default_value));
    }
};

} // namespace slim

#endif // __cpp_lib_expected
