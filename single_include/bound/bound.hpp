//---------------------------------------------------------------------------
// bound 1.0.0 — single-header amalgamation
//
//   *** GENERATED FILE — DO NOT EDIT BY HAND ***
//
// Regenerate with:  cmake --build <build-dir> --target amalgamate
// Source of truth:  include/bound/*.hpp, include/slim/*.hpp
//
// Copyright (C) 2026 Peter Neiss
// slim/* components are MIT-licensed (SPDX-License-Identifier: MIT).
//---------------------------------------------------------------------------
#ifndef BND_SINGLE_HEADER_HPP
#define BND_SINGLE_HEADER_HPP

#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <version>

// ======================================================================
//  bound/bound.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Public umbrella header. Include this to get the full `bnd::bound` API:
// the core type (core.hpp) plus the free-function layers that depend on the
// complete type — casts, arithmetic operators, and bound_range. Those three
// must follow core.hpp because they need `bound<G, P>` fully defined.
//---------------------------------------------------------------------------


// ======================================================================
//  bound/core.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Internal — include "bound/bound.hpp" (the umbrella), not this directly.
// Defines the core `bnd::bound<G, P>` type; the umbrella adds the free-function
// casts/arithmetic/range layers that depend on this complete type.
//---------------------------------------------------------------------------


// ======================================================================
//  bound/generic.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

#define SLIM_OPTIONAL_LEAN_AND_MEAN

// ======================================================================
//  slim/optional.hpp
// ======================================================================
// slim::optional — sentinel-based optional for C++23
//
// The `slim::` namespace is an independent utility namespace; `slim::optional`
// is reusable in any C++23 project and has no `bnd::` dependency. The bound
// library consumes it by specialising `slim::sentinel_traits<bnd::bound<G,P>>`
// and `slim::sentinel_traits<bnd::rational>`, so `slim::optional<bound<...>>`
// has the same size as the underlying bound (the sentinel reserves one value
// from the representable range — see README "slim::optional sentinel").



#ifndef SLIM_OPTIONAL_LEAN_AND_MEAN
#include <any>
#include <chrono>
#include <complex>
#include <coroutine>
#include <functional>
#include <span>
#include <stop_token>
#include <string_view>
#include <thread>
#endif

namespace slim {

// Exception type — inherits from std::bad_optional_access so user code that
// catches the standard type also catches ours.
class bad_optional_access : public std::bad_optional_access {
    const char* msg_;
public:
    explicit bad_optional_access(const char* msg = "bad optional access") noexcept
        : msg_(msg) {}

    const char* what() const noexcept override {
        return msg_;
    }
};

// Failure funnel for the empty-access paths. Throws bad_optional_access when
// exceptions are enabled; otherwise traps — so slim::optional is usable under
// -fno-exceptions / freestanding without depending on the exception ABI.
namespace detail {
[[noreturn]] inline void throw_bad_optional_access(const char* msg) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    throw bad_optional_access(msg);
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

// Tag types — aliased to the std equivalents so a single set of constructor
// overloads handles both `slim::nullopt` and `std::nullopt`.
using nullopt_t = std::nullopt_t;
inline constexpr std::nullopt_t nullopt = std::nullopt;
using in_place_t = std::in_place_t;
inline constexpr std::in_place_t in_place{};

// ============================================================================
// sentinel_traits<T>: customization point for sentinel values.
//
// Users specialize this for their own types to enable slim::optional support.
// Each specialization must provide:
//   static [constexpr] T sentinel() noexcept;
//   static [constexpr] bool is_sentinel(const T& v) noexcept;
// ============================================================================

// Primary template — undefined (opt-in via specialization)
template<class T>
struct sentinel_traits;

// Concept: is sentinel_traits<T> a complete type? The primary template is never
// defined, so completeness tracks opt-in specializations.
template<class T>
concept has_sentinel_traits = requires { sizeof(sentinel_traits<T>); };

// Escape-hatch specialization: a "never-empty" traits. When used as the
// Traits parameter of slim::optional, is_sentinel is always false, so the
// optional always reports has_value()==true and collapses to sizeof(T).
template<typename T>
struct never_empty {
protected:
    static constexpr T sentinel() { detail::throw_bad_optional_access("never_empty"); }
    static constexpr bool is_sentinel(const T&) noexcept { return false; }
};

// Dual escape-hatch: "always-empty" traits — the optional has no T storage
// (sizeof == 1) and reports has_value()==false. For conditional return types
// whose no-value branch structurally never produces a value:
//
//     if constexpr (cacheable) return slim::optional<T>{...};
//     else                     return slim::optional<T, slim::always_empty<T>>{};
template<typename T>
struct always_empty {
protected:
    static T sentinel() = delete;  // never invoked — specialization has no value_
    static constexpr bool is_sentinel(const T&) noexcept { return true; }
};

// ── Scalar specializations ──
// Built-in specializations make sentinel()/is_sentinel() protected, accessed
// only by slim::optional<T, Traits> (which inherits publicly from Traits). User
// specializations should do the same.

// Signed integers (excluding bool)
template<class T>
    requires (std::signed_integral<T> && !std::same_as<T, bool>)
struct sentinel_traits<T>
{
protected:
    static constexpr T sentinel() noexcept { return std::numeric_limits<T>::min(); }
    static constexpr bool is_sentinel(const T& v) noexcept { return v == std::numeric_limits<T>::min(); }
};

// Unsigned integers (excluding bool, char8_t)
template<class T>
    requires (std::unsigned_integral<T> && !std::same_as<T, bool>
              && !std::same_as<T, char8_t>
              && !std::same_as<T, char16_t>
              && !std::same_as<T, char32_t>)
struct sentinel_traits<T>
{
protected:
    static constexpr T sentinel() noexcept { return std::numeric_limits<T>::max(); }
    static constexpr bool is_sentinel(const T& v) noexcept { return v == std::numeric_limits<T>::max(); }
};

// float/double/long double — NaN sentinel, all NaN values disallowed.
// v != v is true iff v is any NaN, and is constexpr everywhere.
template<>
struct sentinel_traits<float> {
protected:
    static constexpr float sentinel() noexcept { return std::numeric_limits<float>::quiet_NaN(); }
    static constexpr bool is_sentinel(const float& v) noexcept { return v != v; }
};

template<>
struct sentinel_traits<double> {
protected:
    static constexpr double sentinel() noexcept { return std::numeric_limits<double>::quiet_NaN(); }
    static constexpr bool is_sentinel(const double& v) noexcept { return v != v; }
};

template<>
struct sentinel_traits<long double> {
protected:
    static constexpr long double sentinel() noexcept { return std::numeric_limits<long double>::quiet_NaN(); }
    static constexpr bool is_sentinel(const long double& v) noexcept { return v != v; }
};

// Pointers → nullptr
template<class T>
struct sentinel_traits<T*> {
protected:
    static constexpr T* sentinel() noexcept { return nullptr; }
    static constexpr bool is_sentinel(T* const& v) noexcept { return v == nullptr; }
};

// char16_t → 0xFFFF (Unicode noncharacter)
template<>
struct sentinel_traits<char16_t> {
protected:
    static constexpr char16_t sentinel() noexcept { return 0xFFFF; }
    static constexpr bool is_sentinel(const char16_t& v) noexcept { return v == static_cast<char16_t>(0xFFFF); }
};

// char32_t → 0xFFFFFFFF (beyond Unicode range)
template<>
struct sentinel_traits<char32_t> {
protected:
    static constexpr char32_t sentinel() noexcept { return 0xFFFFFFFF; }
    static constexpr bool is_sentinel(const char32_t& v) noexcept { return v == static_cast<char32_t>(0xFFFFFFFF); }
};

// ── Standard library type specializations ──

#ifndef SLIM_OPTIONAL_LEAN_AND_MEAN

template<class T, class D>
struct sentinel_traits<std::unique_ptr<T, D>> {
protected:
    static std::unique_ptr<T, D> sentinel() noexcept { return nullptr; }
    static bool is_sentinel(const std::unique_ptr<T, D>& v) noexcept { return !v; }
};

template<class T>
struct sentinel_traits<std::shared_ptr<T>> {
protected:
    static std::shared_ptr<T> sentinel() noexcept { return nullptr; }
    static bool is_sentinel(const std::shared_ptr<T>& v) noexcept { return !v; }
};

template<class CharT, class Traits>
struct sentinel_traits<std::basic_string_view<CharT, Traits>> {
protected:
    static constexpr std::basic_string_view<CharT, Traits> sentinel() noexcept {
        return std::basic_string_view<CharT, Traits>{nullptr, 0};
    }
    static constexpr bool is_sentinel(const std::basic_string_view<CharT, Traits>& v) noexcept {
        return v.data() == nullptr;
    }
};

template<class T, std::size_t E>
struct sentinel_traits<std::span<T, E>> {
protected:
    static constexpr std::span<T, E> sentinel() noexcept { return std::span<T, E>{}; }
    static constexpr bool is_sentinel(const std::span<T, E>& v) noexcept { return v.data() == nullptr; }
};

template<class F>
struct sentinel_traits<std::function<F>> {
protected:
    static std::function<F> sentinel() noexcept { return nullptr; }
    static bool is_sentinel(const std::function<F>& v) noexcept { return !v; }
};

// std::move_only_function is C++23 (libstdc++ ships it from GCC 13); gate the
// specialization so C++20 / GCC 12 builds still compile this header.
#ifdef __cpp_lib_move_only_function
template<class F>
struct sentinel_traits<std::move_only_function<F>> {
protected:
    static std::move_only_function<F> sentinel() noexcept { return {}; }
    static bool is_sentinel(const std::move_only_function<F>& v) noexcept { return !v; }
};
#endif

template<class P>
struct sentinel_traits<std::coroutine_handle<P>> {
protected:
    static constexpr std::coroutine_handle<P> sentinel() noexcept { return std::coroutine_handle<P>{}; }
    static constexpr bool is_sentinel(const std::coroutine_handle<P>& v) noexcept { return !v; }
};

template<>
struct sentinel_traits<std::any> {
protected:
    static std::any sentinel() noexcept { return std::any{}; }
    static bool is_sentinel(const std::any& v) noexcept { return !v.has_value(); }
};

template<>
struct sentinel_traits<std::thread::id> {
protected:
    static constexpr std::thread::id sentinel() noexcept { return std::thread::id{}; }
    static constexpr bool is_sentinel(const std::thread::id& v) noexcept { return v == std::thread::id{}; }
};

template<>
struct sentinel_traits<std::stop_token> {
protected:
    static std::stop_token sentinel() noexcept { return std::stop_token{}; }
    static bool is_sentinel(const std::stop_token& v) noexcept { return !v.stop_possible(); }
};

// chrono::duration — uses the underlying Rep's sentinel (min() for integers, NaN for floats).
// Accesses the (protected) members of sentinel_traits<Rep> via inheritance.
template<class Rep, class Period>
    requires has_sentinel_traits<Rep>
struct sentinel_traits<std::chrono::duration<Rep, Period>> : private sentinel_traits<Rep> {
protected:
    static constexpr std::chrono::duration<Rep, Period> sentinel() noexcept {
        return std::chrono::duration<Rep, Period>{sentinel_traits<Rep>::sentinel()};
    }
    static constexpr bool is_sentinel(const std::chrono::duration<Rep, Period>& v) noexcept {
        return sentinel_traits<Rep>::is_sentinel(v.count());
    }
};

// chrono::time_point — uses the underlying Duration's sentinel
template<class Clock, class Duration>
    requires has_sentinel_traits<Duration>
struct sentinel_traits<std::chrono::time_point<Clock, Duration>> : private sentinel_traits<Duration> {
protected:
    static constexpr std::chrono::time_point<Clock, Duration> sentinel() noexcept {
        return std::chrono::time_point<Clock, Duration>{sentinel_traits<Duration>::sentinel()};
    }
    static constexpr bool is_sentinel(const std::chrono::time_point<Clock, Duration>& v) noexcept {
        return sentinel_traits<Duration>::is_sentinel(v.time_since_epoch());
    }
};

template<class T>
struct sentinel_traits<std::complex<T>> {
protected:
    static constexpr std::complex<T> sentinel() noexcept {
        return std::complex<T>{std::numeric_limits<T>::quiet_NaN(), std::numeric_limits<T>::quiet_NaN()};
    }
    static constexpr bool is_sentinel(const std::complex<T>& v) noexcept {
        return v.real() != v.real();
    }
};

#endif // !SLIM_OPTIONAL_LEAN_AND_MEAN

// exception_ptr — available in both modes (uses <exception> which is always included)
template<>
struct sentinel_traits<std::exception_ptr> {
protected:
    static std::exception_ptr sentinel() noexcept { return std::exception_ptr{}; }
    static bool is_sentinel(const std::exception_ptr& v) noexcept { return !v; }
};

// ── Types deliberately NOT supported ──
//
// std::string, std::u8string, etc.  — empty string is a valid value
// std::error_code                   — default (0) means "success", a valid value
// std::weak_ptr<T>                  — expired state is indistinguishable from sentinel
// std::variant<Ts...>               — cannot deliberately construct valueless state
// std::future<T>, std::shared_future<T> — move-only, rarely stored in containers
// std::filesystem::path             — empty path is a valid value
// std::regex                        — heavyweight, niche use case
// Containers (vector, map, etc.)    — empty is a valid state
// std::reference_wrapper<T>         — always holds a reference, no empty state
// Mutex/thread types                — not copyable or movable

// ============================================================================
// optional: sentinel-based optional with no bool flag
// ============================================================================

template<class T, class Traits = sentinel_traits<T>>
class optional;

// Helper trait to detect optional types (slim or std)
namespace detail {
template<class> inline constexpr bool is_optional_v = false;
template<class T, class Tr> inline constexpr bool is_optional_v<optional<T, Tr>> = true;
template<class T> inline constexpr bool is_optional_v<std::optional<T>> = true;

// Detects slim::optional<T, never_empty<T>> — used to forbid such results
// from monadic operations whose empty branch cannot construct one.
template<class> inline constexpr bool is_never_empty_optional_v = false;
template<class T>
inline constexpr bool is_never_empty_optional_v<optional<T, never_empty<T>>> = true;
}

// Publicly inherits from Traits so any public members users attach to
// their sentinel_traits specialization (constants, typedefs, helpers) are
// reachable through the optional. Empty-base optimization keeps
// sizeof(optional<T>) == sizeof(T) whenever Traits has no data members.
template<class T, class Traits>
class optional : public Traits {
    T value_;

    // Throws if v matches the trait's sentinel. The check (and the unwind
    // path it implies) is elided at compile time when the target trait has
    // no representable empty state — under never_empty<T>, is_sentinel
    // always returns false, so the function reduces to a no-op and the
    // exception handler vanishes from generated code.
    static constexpr void validate_not_sentinel(const T& v) {
        if constexpr (can_be_empty) {
            if (Traits::is_sentinel(v)) {
                detail::throw_bad_optional_access("Cannot construct optional with sentinel value");
            }
        }
    }

    static_assert(has_sentinel_traits<T> || !std::same_as<Traits, sentinel_traits<T>>,
        "slim::optional<T>: no sentinel_traits<T> specialization is visible. "
        "Either provide one, supply a custom Traits as the second template "
        "parameter, or use slim::optional<T, slim::never_empty<T>>.");

public:
    using value_type = T;
    using traits_type = Traits;

    // True iff this optional has a representable empty state. False only for
    // the never_empty escape-hatch traits.
    static constexpr bool can_be_empty = !std::same_as<Traits, never_empty<T>>;

    // Public accessor for the trait's sentinel value (the bit pattern that
    // means "empty"). Useful for interop with C APIs that need the sentinel.
    static constexpr T sentinel_value() noexcept(noexcept(Traits::sentinel()))
        requires (can_be_empty)
    {
        return Traits::sentinel();
    }

    // Default constructor — trivial whenever T is trivially default
    // constructible. In that case value_ is left in T's default-initialized
    // state (indeterminate for scalars, value-initialized for class types);
    // use `optional o = nullopt;` if you need a guaranteed-empty optional.
    constexpr optional() requires std::is_trivially_default_constructible_v<T> = default;

    // Fallback: sentinel-initializing default constructor for T that is not
    // trivially default constructible (only available under sentinel traits,
    // since the never-empty variant has no sentinel to fall back to).
    constexpr optional() noexcept(noexcept(T(Traits::sentinel())))
        requires (!std::is_trivially_default_constructible_v<T>)
        : value_(Traits::sentinel()) {}

    constexpr optional(nullopt_t) noexcept(noexcept(T(Traits::sentinel())))
      requires (can_be_empty)
        : value_(Traits::sentinel()) {}

    constexpr optional(const optional& other) = default;
    constexpr optional(optional&& other) noexcept(std::is_nothrow_move_constructible_v<T>) = default;

    template<class... Args>
        requires std::is_constructible_v<T, Args...>
    constexpr explicit optional(in_place_t, Args&&... args)
        : value_(std::forward<Args>(args)...)
    {
        validate_not_sentinel(value_);
    }

    template<class U = T>
        requires (!std::same_as<std::remove_cvref_t<U>, optional> &&
                  !std::same_as<std::remove_cvref_t<U>, in_place_t> &&
                  !std::same_as<std::remove_cvref_t<U>, nullopt_t> &&
                  std::is_constructible_v<T, U>)
    constexpr explicit(!std::is_convertible_v<U, T>)
    optional(U&& value)
        : value_(std::forward<U>(value))
    {
        validate_not_sentinel(value_);
    }

    // Non-throwing factory: collapses a sentinel-valued T to nullopt instead
    // of throwing. The throwing single-value ctor stays the default ("you
    // promised this wasn't a sentinel"); this factory is the public escape
    // hatch for "test if T already encodes empty".
    template<class U = T>
        requires (std::is_constructible_v<T, U>)
    static constexpr optional from_maybe_sentinel(U&& value)
        noexcept(std::is_nothrow_constructible_v<T, U>)
    {
        optional o{nullopt};
        if constexpr (can_be_empty)
        {
            T tmp(std::forward<U>(value));
            if (!Traits::is_sentinel(tmp))
                o.value_ = std::move(tmp);
        }
        else
        {
            o.value_ = T(std::forward<U>(value));
        }
        return o;
    }

    // Construct from another optional with different T and/or Traits.
    // The validate_not_sentinel call is retained because *other may be a
    // legitimate value under TrU's traits but happen to coincide with this
    // optional's sentinel under Traits.
    template<class U, class TrU>
        requires (!(std::same_as<U, T> && std::same_as<TrU, Traits>) &&
                  std::is_constructible_v<T, const U&>)
    constexpr explicit(!std::is_convertible_v<const U&, T>)
    optional(const optional<U, TrU>& other)
        : value_(other.has_value() ? *other : Traits::sentinel())
    {
        if (has_value()) {
            validate_not_sentinel(value_);
        }
    }

    template<class U, class TrU>
        requires (!(std::same_as<U, T> && std::same_as<TrU, Traits>) &&
                  std::is_constructible_v<T, U&&>)
    constexpr explicit(!std::is_convertible_v<U&&, T>)
    optional(optional<U, TrU>&& other)
        : value_(other.has_value() ? std::move(*other) : Traits::sentinel())
    {
        if (has_value()) {
            validate_not_sentinel(value_);
        }
    }

    // Construct from std::optional
    template<class U = T>
        requires (std::is_constructible_v<T, const U&>)
    constexpr explicit(!std::is_convertible_v<const U&, T>)
    optional(const std::optional<U>& other)
        : value_(other.has_value() ? *other : Traits::sentinel())
    {
        if (has_value()) {
            validate_not_sentinel(value_);
        }
    }

    template<class U = T>
        requires (std::is_constructible_v<T, U&&>)
    constexpr explicit(!std::is_convertible_v<U&&, T>)
    optional(std::optional<U>&& other)
        : value_(other.has_value() ? std::move(*other) : Traits::sentinel())
    {
        if (has_value()) {
            validate_not_sentinel(value_);
        }
    }

    constexpr ~optional() = default;

    // Assignment
    constexpr optional& operator=(nullopt_t) noexcept(noexcept(std::declval<T&>() = Traits::sentinel()))
      requires (can_be_empty)
    {
        value_ = Traits::sentinel();
        return *this;
    }

    constexpr optional& operator=(const optional& other) = default;
    constexpr optional& operator=(optional&& other) noexcept(std::is_nothrow_move_assignable_v<T>) = default;

    template<class U = T>
        requires (!std::same_as<std::remove_cvref_t<U>, optional> &&
                  std::is_constructible_v<T, U> &&
                  std::is_assignable_v<T&, U>)
    constexpr optional& operator=(U&& value) {
        value_ = std::forward<U>(value);
        validate_not_sentinel(value_);
        return *this;
    }

    template<class U, class TrU>
        requires (!(std::same_as<U, T> && std::same_as<TrU, Traits>) &&
                  std::is_constructible_v<T, const U&> &&
                  std::is_assignable_v<T&, const U&>)
    constexpr optional& operator=(const optional<U, TrU>& other) {
        if (other.has_value()) {
            value_ = *other;
            validate_not_sentinel(value_);
        } else {
            value_ = Traits::sentinel();
        }
        return *this;
    }

    template<class U, class TrU>
        requires (!(std::same_as<U, T> && std::same_as<TrU, Traits>) &&
                  std::is_constructible_v<T, U> &&
                  std::is_assignable_v<T&, U>)
    constexpr optional& operator=(optional<U, TrU>&& other) {
        if (other.has_value()) {
            value_ = std::move(*other);
            validate_not_sentinel(value_);
        } else {
            value_ = Traits::sentinel();
        }
        return *this;
    }

    // Assign from std::optional
    template<class U = T>
        requires (std::is_constructible_v<T, const U&> &&
                  std::is_assignable_v<T&, const U&>)
    constexpr optional& operator=(const std::optional<U>& other) {
        if (other.has_value()) {
            value_ = *other;
            validate_not_sentinel(value_);
        } else {
            value_ = Traits::sentinel();
        }
        return *this;
    }

    template<class U = T>
        requires (std::is_constructible_v<T, U> &&
                  std::is_assignable_v<T&, U>)
    constexpr optional& operator=(std::optional<U>&& other) {
        if (other.has_value()) {
            value_ = std::move(*other);
            validate_not_sentinel(value_);
        } else {
            value_ = Traits::sentinel();
        }
        return *this;
    }

    // Conversion to std::optional
    constexpr operator std::optional<T>() const {
        if (has_value()) {
            return std::optional<T>{value_};
        }
        return std::nullopt;
    }

    // Observers
    constexpr bool has_value() const noexcept {
        return !Traits::is_sentinel(value_);
    }

    constexpr explicit operator bool() const noexcept {
        return has_value();
    }

#if defined(__cpp_explicit_this_parameter) && __cpp_explicit_this_parameter >= 202110L
    // value() / operator* / operator-> use C++23 deducing-this so a single
    // function template covers all four cv/ref qualifications.
    template<class Self>
    constexpr auto&& value(this Self&& self) {
        if (!self.has_value()) {
            detail::throw_bad_optional_access("optional has no value");
        }
        return std::forward<Self>(self).value_;
    }

    template<class Self>
    constexpr auto&& operator*(this Self&& self) noexcept {
        return std::forward<Self>(self).value_;
    }

    template<class Self>
    constexpr auto operator->(this Self&& self) noexcept {
        return std::addressof(self.value_);
    }

    template<class Self, class U>
        requires std::is_convertible_v<U, T>
    constexpr T value_or(this Self&& self, U&& default_value) {
        return self.has_value()
            ? static_cast<T>(std::forward<Self>(self).value_)
            : static_cast<T>(std::forward<U>(default_value));
    }
#else
    constexpr T& value() & {
        if (!has_value()) detail::throw_bad_optional_access("optional has no value");
        return value_;
    }
    constexpr const T& value() const& {
        if (!has_value()) detail::throw_bad_optional_access("optional has no value");
        return value_;
    }
    constexpr T&& value() && {
        if (!has_value()) detail::throw_bad_optional_access("optional has no value");
        return std::move(value_);
    }
    constexpr const T&& value() const&& {
        if (!has_value()) detail::throw_bad_optional_access("optional has no value");
        return std::move(value_);
    }

    constexpr T& operator*() & noexcept { return value_; }
    constexpr const T& operator*() const& noexcept { return value_; }
    constexpr T&& operator*() && noexcept { return std::move(value_); }
    constexpr const T&& operator*() const&& noexcept { return std::move(value_); }

    constexpr T* operator->() noexcept { return std::addressof(value_); }
    constexpr const T* operator->() const noexcept { return std::addressof(value_); }

    template<class U>
        requires std::is_convertible_v<U, T>
    constexpr T value_or(U&& default_value) const& {
        return has_value()
            ? static_cast<T>(value_)
            : static_cast<T>(std::forward<U>(default_value));
    }
    template<class U>
        requires std::is_convertible_v<U, T>
    constexpr T value_or(U&& default_value) && {
        return has_value()
            ? static_cast<T>(std::move(value_))
            : static_cast<T>(std::forward<U>(default_value));
    }
#endif

    // Modifiers
    constexpr void reset() noexcept(noexcept(std::declval<T&>() = Traits::sentinel()))
        requires (can_be_empty)
    {
        value_ = Traits::sentinel();
    }

    template<class... Args>
        requires std::is_constructible_v<T, Args...>
    constexpr T& emplace(Args&&... args)
    {
        // Not noexcept: validate_not_sentinel can throw bad_optional_access
        // when args... happens to construct the sentinel. Construct into a
        // temporary first so that if construction or validation throws,
        // value_ is left untouched (strong exception guarantee).
        T tmp(std::forward<Args>(args)...);
        validate_not_sentinel(tmp);
        value_ = std::move(tmp);
        return value_;
    }

    // Move the value out and reset to empty. Unlike a plain move, this
    // guarantees has_value() == false afterwards — useful given that
    // sentinel-based moves do not normally disengage the source.
    constexpr T take()
        requires (can_be_empty && std::is_move_constructible_v<T>)
    {
        if (!has_value()) {
            detail::throw_bad_optional_access("optional has no value");
        }
        T out = std::move(value_);
        value_ = Traits::sentinel();
        return out;
    }

    // Swap
    constexpr void swap(optional& other)
        noexcept(std::is_nothrow_move_constructible_v<T> &&
                 std::is_nothrow_swappable_v<T>)
    {
        using std::swap;
        swap(value_, other.value_);
    }

    // Monadic operations (C++23)
#if defined(__cpp_explicit_this_parameter) && __cpp_explicit_this_parameter >= 202110L
    template<class Self, class F>
        requires detail::is_optional_v<std::remove_cvref_t<
                     std::invoke_result_t<F, decltype(std::declval<Self>().value_)>>> &&
                 (!detail::is_never_empty_optional_v<std::remove_cvref_t<
                     std::invoke_result_t<F, decltype(std::declval<Self>().value_)>>>)
    constexpr auto and_then(this Self&& self, F&& f) {
        using U = std::remove_cvref_t<
            std::invoke_result_t<F, decltype(std::forward<Self>(self).value_)>>;
        if (self.has_value()) {
            return std::forward<F>(f)(std::forward<Self>(self).value_);
        } else {
            return U(std::nullopt);
        }
    }

    template<class Self, class F>
    constexpr auto transform(this Self&& self, F&& f) {
        using U = std::remove_cvref_t<
            std::invoke_result_t<F, decltype(std::forward<Self>(self).value_)>>;
        if constexpr (std::same_as<U, T>) {
            // Propagate Traits when result type matches.
            if (self.has_value())
                return optional<U, Traits>{std::forward<F>(f)(std::forward<Self>(self).value_)};
            // never_empty has no nullopt ctor, so its empty branch is
            // constrained away; this keeps the return well-formed (it never
            // runs — has_value() is always true above).
            if constexpr (std::same_as<Traits, never_empty<T>>) {
                return optional<U, Traits>{std::forward<F>(f)(std::forward<Self>(self).value_)};
            } else {
                return optional<U, Traits>(nullopt);
            }
        } else if constexpr (has_sentinel_traits<U>) {
            if (self.has_value()) {
                return optional<U>{std::forward<F>(f)(std::forward<Self>(self).value_)};
            } else {
                return optional<U>(nullopt);
            }
        } else {
            if (self.has_value()) {
                return std::optional<U>{std::forward<F>(f)(std::forward<Self>(self).value_)};
            } else {
                return std::optional<U>(std::nullopt);
            }
        }
    }

    // or_else: invokes f with the Traits subobject so the recovery callable
    // can use trait-provided constants/helpers when fabricating a fallback.
    // f must be invocable as f(Traits const&) and return something
    // convertible to optional.
    template<class Self, class F>
        requires std::is_invocable_v<F, const Traits&> &&
                 std::is_convertible_v<std::invoke_result_t<F, const Traits&>, optional>
    constexpr optional or_else(this Self&& self, F&& f) {
        if (self.has_value()) {
            return std::forward<Self>(self);
        } else {
            return std::forward<F>(f)(static_cast<const Traits&>(self));
        }
    }
#else
    // and_then — const& and && overloads
    template<class F>
        requires detail::is_optional_v<std::remove_cvref_t<
                     std::invoke_result_t<F, const T&>>> &&
                 (!detail::is_never_empty_optional_v<std::remove_cvref_t<
                     std::invoke_result_t<F, const T&>>>)
    constexpr auto and_then(F&& f) const& {
        using U = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
        if (has_value()) return std::forward<F>(f)(value_);
        return U(std::nullopt);
    }
    template<class F>
        requires detail::is_optional_v<std::remove_cvref_t<
                     std::invoke_result_t<F, T&&>>> &&
                 (!detail::is_never_empty_optional_v<std::remove_cvref_t<
                     std::invoke_result_t<F, T&&>>>)
    constexpr auto and_then(F&& f) && {
        using U = std::remove_cvref_t<std::invoke_result_t<F, T&&>>;
        if (has_value()) return std::forward<F>(f)(std::move(value_));
        return U(std::nullopt);
    }

    // transform — const& and && overloads
    template<class F>
    constexpr auto transform(F&& f) const& {
        using U = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
        if constexpr (std::same_as<U, T>) {
            if (has_value())
                return optional<U, Traits>{std::forward<F>(f)(value_)};
            // never_empty's empty branch is constrained away; keep a
            // well-formed return (unreachable — has_value() is always true).
            if constexpr (std::same_as<Traits, never_empty<T>>)
                return optional<U, Traits>{std::forward<F>(f)(value_)};
            else
                return optional<U, Traits>(nullopt);
        } else if constexpr (has_sentinel_traits<U>) {
            if (has_value()) return optional<U>{std::forward<F>(f)(value_)};
            return optional<U>(nullopt);
        } else {
            if (has_value()) return std::optional<U>{std::forward<F>(f)(value_)};
            return std::optional<U>(std::nullopt);
        }
    }
    template<class F>
    constexpr auto transform(F&& f) && {
        using U = std::remove_cvref_t<std::invoke_result_t<F, T&&>>;
        if constexpr (std::same_as<U, T>) {
            if (has_value())
                return optional<U, Traits>{std::forward<F>(f)(std::move(value_))};
            // never_empty's empty branch is constrained away; keep a
            // well-formed return (unreachable — has_value() is always true).
            if constexpr (std::same_as<Traits, never_empty<T>>)
                return optional<U, Traits>{std::forward<F>(f)(std::move(value_))};
            else
                return optional<U, Traits>(nullopt);
        } else if constexpr (has_sentinel_traits<U>) {
            if (has_value()) return optional<U>{std::forward<F>(f)(std::move(value_))};
            return optional<U>(nullopt);
        } else {
            if (has_value()) return std::optional<U>{std::forward<F>(f)(std::move(value_))};
            return std::optional<U>(std::nullopt);
        }
    }

    // or_else — const& and && overloads
    template<class F>
        requires std::is_invocable_v<F, const Traits&> &&
                 std::is_convertible_v<std::invoke_result_t<F, const Traits&>, optional>
    constexpr optional or_else(F&& f) const& {
        if (has_value()) return *this;
        return std::forward<F>(f)(static_cast<const Traits&>(*this));
    }
    template<class F>
        requires std::is_invocable_v<F, const Traits&> &&
                 std::is_convertible_v<std::invoke_result_t<F, const Traits&>, optional>
    constexpr optional or_else(F&& f) && {
        if (has_value()) return std::move(*this);
        return std::forward<F>(f)(static_cast<const Traits&>(*this));
    }
#endif
};

// ============================================================================
// optional<T, always_empty<T>> — partial specialization with no T storage
// ============================================================================
//
// Carries no value_ member (sizeof 1, gone under [[no_unique_address]]);
// has_value() is always false. Members needing a stored T (operator*/->, value
// constructors, emplace, …) are deliberately not provided, so using them is a
// compile error. Pick via `if constexpr` in templated return types:
//   if constexpr (cond) return slim::optional<T>{...};
//   else                return slim::optional<T, slim::always_empty<T>>{};
template<class T>
class optional<T, always_empty<T>> : public always_empty<T> {
public:
    using value_type = T;
    using traits_type = always_empty<T>;
    static constexpr bool can_be_empty = true;

    constexpr optional() noexcept = default;
    constexpr optional(nullopt_t) noexcept {}
    constexpr optional(const optional&) noexcept = default;
    constexpr optional(optional&&) noexcept = default;

    // Convert from any other optional (slim or std). The source value is
    // discarded — this branch structurally never holds a value.
    template<class U, class TrU>
    constexpr optional(const optional<U, TrU>&) noexcept {}
    template<class U, class TrU>
    constexpr optional(optional<U, TrU>&&) noexcept {}
    template<class U>
    constexpr optional(const std::optional<U>&) noexcept {}
    template<class U>
    constexpr optional(std::optional<U>&&) noexcept {}

    constexpr ~optional() = default;

    constexpr optional& operator=(nullopt_t) noexcept { return *this; }
    constexpr optional& operator=(const optional&) noexcept = default;
    constexpr optional& operator=(optional&&) noexcept = default;
    template<class U, class TrU>
    constexpr optional& operator=(const optional<U, TrU>&) noexcept { return *this; }
    template<class U, class TrU>
    constexpr optional& operator=(optional<U, TrU>&&) noexcept { return *this; }

    // Convert to std::optional — always nullopt.
    constexpr operator std::optional<T>() const noexcept { return std::nullopt; }

    constexpr bool has_value() const noexcept { return false; }
    constexpr explicit operator bool() const noexcept { return false; }

    [[noreturn]] constexpr T value() const {
        detail::throw_bad_optional_access("always_empty optional has no value");
    }

    template<class U>
        requires std::is_convertible_v<U, T>
    constexpr T value_or(U&& default_value) const {
        return static_cast<T>(std::forward<U>(default_value));
    }

    constexpr void reset() noexcept {}

    constexpr void swap(optional&) noexcept {}

    // Monadic operations: trivially short-circuit. The lambdas are never
    // invoked — only their return type is used to compute the result type.
    template<class F>
        requires detail::is_optional_v<std::remove_cvref_t<std::invoke_result_t<F, const T&>>> &&
                 (!detail::is_never_empty_optional_v<std::remove_cvref_t<std::invoke_result_t<F, const T&>>>)
    constexpr auto and_then(F&&) const {
        using U = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
        return U(std::nullopt);
    }

    template<class F>
    constexpr auto transform(F&&) const {
        using U = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
        if constexpr (has_sentinel_traits<U>) {
            return optional<U>(nullopt);
        } else {
            return std::optional<U>(std::nullopt);
        }
    }

    // or_else: the lambda may run for side effects, but its return value is
    // structurally another always_empty optional, so we always return *this.
    template<class F>
        requires std::is_invocable_v<F, const always_empty<T>&> &&
                 std::is_convertible_v<std::invoke_result_t<F, const always_empty<T>&>, optional>
    constexpr optional or_else(F&& f) const {
        (void)std::forward<F>(f)(static_cast<const always_empty<T>&>(*this));
        return *this;
    }
};

// Comparisons for the always_empty specialization. The generic templates
// below would short-circuit on has_value() at runtime, but they call
// `*lhs == *rhs` in unreachable branches — and operator* is deliberately
// not provided on the always_empty specialization, so those templates fail
// to instantiate. These overloads are picked first by partial-ordering.
template<class T>
constexpr bool operator==(const optional<T, always_empty<T>>&,
                          const optional<T, always_empty<T>>&) noexcept {
    return true;
}

template<class T>
constexpr std::strong_ordering operator<=>(const optional<T, always_empty<T>>&,
                                           const optional<T, always_empty<T>>&) noexcept {
    return std::strong_ordering::equal;
}

// always_empty vs any other optional (slim or std): equal iff the other is
// also empty.
template<class T, class Tr>
constexpr bool operator==(const optional<T, always_empty<T>>&,
                          const optional<T, Tr>& other) noexcept {
    return !other.has_value();
}
template<class T, class Tr>
constexpr bool operator==(const optional<T, Tr>& other,
                          const optional<T, always_empty<T>>&) noexcept {
    return !other.has_value();
}
template<class T, class U>
constexpr bool operator==(const optional<T, always_empty<T>>&,
                          const std::optional<U>& other) noexcept {
    return !other.has_value();
}
template<class T, class U>
constexpr bool operator==(const std::optional<U>& other,
                          const optional<T, always_empty<T>>&) noexcept {
    return !other.has_value();
}

// always_empty vs nullopt: trivially equal.
template<class T>
constexpr bool operator==(const optional<T, always_empty<T>>&, nullopt_t) noexcept {
    return true;
}

// ============================================================================
// Comparison operators — optional vs optional
// ============================================================================

template<class T, class Tr>
constexpr bool operator==(const optional<T, Tr>& lhs, const optional<T, Tr>& rhs) {
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }
    if (!lhs.has_value()) {
        return true;
    }
    return *lhs == *rhs;
}

template<class T, class Tr>
constexpr std::compare_three_way_result_t<T> operator<=>(const optional<T, Tr>& lhs, const optional<T, Tr>& rhs)
    requires std::three_way_comparable<T>
{
    if (lhs.has_value() && rhs.has_value()) {
        return *lhs <=> *rhs;
    }
    return lhs.has_value() <=> rhs.has_value();
}

// ============================================================================
// Comparison operators — optional vs std::optional
// ============================================================================

template<class T, class Tr>
    requires (!std::is_same_v<Tr, always_empty<T>>)
constexpr bool operator==(const optional<T, Tr>& lhs, const std::optional<T>& rhs) {
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }
    if (!lhs.has_value()) {
        return true;
    }
    return *lhs == *rhs;
}

template<class T, class Tr>
    requires (!std::is_same_v<Tr, always_empty<T>>)
constexpr bool operator==(const std::optional<T>& lhs, const optional<T, Tr>& rhs) {
    return rhs == lhs;
}

template<class T, class Tr>
constexpr std::compare_three_way_result_t<T> operator<=>(const optional<T, Tr>& lhs, const std::optional<T>& rhs)
    requires std::three_way_comparable<T>
{
    if (lhs.has_value() && rhs.has_value()) {
        return *lhs <=> *rhs;
    }
    return lhs.has_value() <=> rhs.has_value();
}

// ============================================================================
// Comparison with nullopt
// ============================================================================

template<class T, class Tr>
constexpr bool operator==(const optional<T, Tr>& opt, nullopt_t) noexcept {
    return !opt.has_value();
}

template<class T, class Tr>
constexpr std::strong_ordering operator<=>(const optional<T, Tr>& opt, nullopt_t) noexcept {
    return opt.has_value() <=> false;
}

// ============================================================================
// Comparison with T
// ============================================================================

template<class T, class Tr, class U>
    requires (!detail::is_optional_v<std::remove_cvref_t<U>> &&
              !std::same_as<std::remove_cvref_t<U>, nullopt_t>)
constexpr bool operator==(const optional<T, Tr>& opt, const U& value) {
    return opt.has_value() && (*opt == value);
}

template<class T, class Tr, class U>
    requires (!detail::is_optional_v<std::remove_cvref_t<U>> &&
              !std::same_as<std::remove_cvref_t<U>, nullopt_t> &&
              std::three_way_comparable_with<T, U>)
constexpr std::compare_three_way_result_t<T, U> operator<=>(const optional<T, Tr>& opt, const U& value) {
    return opt.has_value() ? *opt <=> value : std::strong_ordering::less;
}

// ============================================================================
// Specialized algorithms
// ============================================================================

template<class T, class Tr>
constexpr void swap(optional<T, Tr>& lhs, optional<T, Tr>& rhs)
    noexcept(noexcept(lhs.swap(rhs)))
{
    lhs.swap(rhs);
}

template<class T>
constexpr optional<std::decay_t<T>> make_optional(T&& value) {
    return optional<std::decay_t<T>>(std::forward<T>(value));
}

template<class T, class... Args>
constexpr optional<T> make_optional(Args&&... args) {
    return optional<T>(in_place, std::forward<Args>(args)...);
}

// Trait-aware overloads: explicitly select the Traits parameter.
template<class T, class Traits, class U>
    requires std::is_constructible_v<T, U&&>
constexpr optional<T, Traits> make_optional(U&& value) {
    return optional<T, Traits>(std::forward<U>(value));
}

template<class T, class Traits, class... Args>
    requires std::is_constructible_v<T, Args...>
constexpr optional<T, Traits> make_optional(Args&&... args) {
    return optional<T, Traits>(in_place, std::forward<Args>(args)...);
}

// Deduction guides
template<class T>
optional(T) -> optional<T>;

template<class T, class... Args>
optional(in_place_t, T, Args...) -> optional<T>;

// ============================================================================
// Hash support
// ============================================================================

} // namespace slim

// Extend std::hash for optional
namespace std {

template<class T, class Tr>
struct hash<slim::optional<T, Tr>> {
    constexpr size_t operator()(const slim::optional<T, Tr>& opt) const
        noexcept(noexcept(hash<T>{}(std::declval<T>())))
        requires requires { hash<T>{}(std::declval<T>()); }
    {
        if (!opt.has_value()) {
            // Distinct sentinel hash to reduce collision with hash<T>{}(T{}).
            return static_cast<size_t>(-1);
        }
        return hash<T>{}(*opt);
    }
};

// Hash specialization for always_empty optional — does not require hash<T>.
template<class T>
struct hash<slim::optional<T, slim::always_empty<T>>> {
    constexpr size_t operator()(const slim::optional<T, slim::always_empty<T>>&) const noexcept {
        return static_cast<size_t>(-1);
    }
};

// numeric_limits for slim::optional — reflects the reduced valid range when
// the default sentinel traits are used. For never_empty<T> (no value is
// reserved) the limits are inherited unchanged.
template<class T, class Tr>
    requires slim::has_sentinel_traits<T> && numeric_limits<T>::is_specialized
struct numeric_limits<slim::optional<T, Tr>> : numeric_limits<T> {
private:
    static constexpr bool reserves_value =
        std::same_as<Tr, slim::sentinel_traits<T>>;
public:
    static constexpr T min() noexcept {
        if constexpr (reserves_value && std::signed_integral<T>)
            return numeric_limits<T>::min() + 1;
        else
            return numeric_limits<T>::min();
    }

    static constexpr T lowest() noexcept {
        if constexpr (reserves_value && std::signed_integral<T>)
            return numeric_limits<T>::min() + 1;
        else
            return numeric_limits<T>::lowest();
    }

    static constexpr T max() noexcept {
        if constexpr (reserves_value && (std::unsigned_integral<T> || std::same_as<T, char16_t> || std::same_as<T, char32_t>))
            return numeric_limits<T>::max() - 1;
        else
            return numeric_limits<T>::max();
    }

    static constexpr bool has_quiet_NaN =
        (reserves_value && std::floating_point<T>) ? false : numeric_limits<T>::has_quiet_NaN;
    static constexpr bool has_signaling_NaN =
        (reserves_value && std::floating_point<T>) ? false : numeric_limits<T>::has_signaling_NaN;
};

} // namespace std


// ======================================================================
//  bound/detail/debug.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

// <stdexcept> backs the *default* throwing handler only; it is the single heavy
// error include and is pulled in solely when exceptions are available. A
// freestanding / -fno-exceptions build never sees it and traps instead. Error
// reporting otherwise stays on static const-char* messages — no <string>, no
// <string_view>, no <system_error>. (General-purpose stringification lives in
// the opt-in "bound/io.hpp".)
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    #define BND_HAS_EXCEPTIONS 1
    #include <stdexcept>
#else
    #define BND_HAS_EXCEPTIONS 0
#endif

//---------------------------------------------------------------------------
// Attribute shims. Error/throw paths are marked cold + non-inline so the
// optimiser keeps them out of the hot path (and out of the inlined body of
// otherwise-trivial assignment/arithmetic). Standard [[noreturn]] / [[unlikely]]
// are used directly at the throw sites and branches.
//---------------------------------------------------------------------------
#if defined(__GNUC__) || defined(__clang__)
#  define BND_COLD     [[gnu::cold]]
#  define BND_NOINLINE [[gnu::noinline]]
#  define BND_TRAP()   __builtin_trap()
#elif defined(_MSC_VER)
#  define BND_COLD
#  define BND_NOINLINE __declspec(noinline)
#  define BND_TRAP()   __debugbreak()
#else
#  define BND_COLD
#  define BND_NOINLINE
#  include <cstdlib>
#  define BND_TRAP()   ::abort()
#endif

//---------------------------------------------------------------------------
// debug — error codes, the replaceable failure handler, and diagnostic helpers.
// `errc` enumerates the failure modes. Every runtime failure funnels through
// `detail::raise`, which calls the installed `error_handler` — by default one
// that throws `bnd::bound_error` (hosted) or traps (freestanding / no
// exceptions). `set_error_handler` lets a bare-metal target redirect failures
// to a reset/log without depending on <system_error> or the C++ exception ABI.
// `print_types` is a static_assert debug helper for template instantiation.
//---------------------------------------------------------------------------
namespace bnd
{
  //---------------------------------------------------------------------------
  // error codes
  //---------------------------------------------------------------------------
  enum class errc
  {
    domain_error = 1,   // value outside interval
    division_by_zero,   // divisor is zero
    overflow,           // rational arithmetic overflow
    rounding_error,     // notch incompatibility
    not_a_value,        // operand is in its sentinel (NaN-like) state
    not_finite,         // non-finite double input (NaN/Inf)
  };

  // Static, allocation-free message per code. The single source of truth for
  // every error path; returns a null-terminated string literal so it doubles as
  // the default exception's what() and as an on_error message.
  constexpr const char* errc_message(errc e) noexcept
  {
    switch (e)
    {
      case errc::domain_error:     return "value outside interval";
      case errc::division_by_zero: return "division by zero";
      case errc::overflow:         return "rational arithmetic overflow";
      case errc::rounding_error:   return "notch incompatibility";
      case errc::not_a_value:      return "not a value (sentinel state)";
      case errc::not_finite:       return "non-finite floating-point value";
    }
    return "unknown bound error";
  }

#if BND_HAS_EXCEPTIONS
  //---------------------------------------------------------------------------
  // bound_error — the exception thrown by the default handler. Derives from
  // std::runtime_error (the library's only <stdexcept> use) and carries the
  // originating `errc` so `catch (bound_error& e) { e.code; }` replaces the old
  // `e.code() == make_error_code(...)` idiom.
  //---------------------------------------------------------------------------
  struct bound_error : std::runtime_error
  {
    errc code;
    explicit bound_error(errc c)
      : std::runtime_error(errc_message(c)), code(c) {}
    bound_error(errc c, const char* what)
      : std::runtime_error(what ? what : errc_message(c)), code(c) {}
  };
#endif

  //---------------------------------------------------------------------------
  // Replaceable failure handler. Contract: it must NOT return (throw / longjmp /
  // abort / reset). If it does return, `raise` traps to honour [[noreturn]].
  //---------------------------------------------------------------------------
  using error_handler_t = void (*)(errc code, const char* what);

  namespace detail
  {
    [[noreturn]] BND_COLD BND_NOINLINE
    inline void default_error_handler(errc code, const char* what)
    {
#if BND_HAS_EXCEPTIONS
      throw bound_error(code, what);
#else
      (void)code; (void)what;
      BND_TRAP();
#endif
    }

    // Header-only global: an inline variable, one per program. Mutating it is a
    // runtime-only act (constant evaluation never reads it), so constexpr paths
    // are unaffected.
    inline error_handler_t g_error_handler = &default_error_handler;
  } // namespace detail

  // Install a handler; returns the previous one. A null argument restores the
  // default. Never throws.
  inline error_handler_t set_error_handler(error_handler_t h) noexcept
  {
    error_handler_t prev = detail::g_error_handler;
    detail::g_error_handler = h ? h : &detail::default_error_handler;
    return prev;
  }

  inline error_handler_t get_error_handler() noexcept
  { return detail::g_error_handler; }

  namespace detail
  {
    //-----------------------------------------------------------------------
    // Outlined failure funnel. Cold + non-inline so the (rare) machinery is
    // emitted once, off the hot path. Not constexpr: calling it during constant
    // evaluation is ill-formed, which is exactly how the checked compile-time
    // paths hard-fail the build (the old `throw` did the same).
    //-----------------------------------------------------------------------
    [[noreturn]] BND_COLD BND_NOINLINE
    inline void raise(errc code, const char* what = nullptr)
    {
      g_error_handler(code, what ? what : errc_message(code));
      BND_TRAP();   // handler must not return; trap if it did
    }

    //-----------------------------------------------------------------------
    // Compile-time-only diagnostic. A fixed_string NTTP carries the message
    // into the type, so a malformed literal / overflow aborts constant
    // evaluation with the text in the instantiation — no `throw` token, so it
    // also compiles under -fno-exceptions. Never reached at runtime (all call
    // sites are guarded by std::is_constant_evaluated / are consteval).
    //-----------------------------------------------------------------------
    template <unsigned N>
    struct fixed_string
    {
      char data[N]{};
      constexpr fixed_string(const char (&s)[N])
      { for (unsigned i = 0; i < N; ++i) data[i] = s[i]; }
    };

    template <fixed_string Msg>
    [[noreturn]] BND_COLD BND_NOINLINE
    inline void constexpr_error()
    { raise(errc::overflow, Msg.data); }
  } // namespace detail

  //---------------------------------------------------------------------------
  // diagnostics
  //---------------------------------------------------------------------------
  template <typename... Ts>
  struct print_types
  {
      static_assert(!sizeof...(Ts), "=== PRINT_TYPES ===");
  };
} // namespace bnd


// ======================================================================
//  bound/grid.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


// ======================================================================
//  bound/lift.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


// ======================================================================
//  slim/expected.hpp
// ======================================================================
// slim::expected — minimal C++20 backport of std::expected
//
// An independent utility namespace, no `bnd::` dependency. Mirrors the subset of
// C++23 std::expected the library consumes (value/error access, `unexpected`,
// deref, throwing value()), giving one error-channel code path even on C++20
// toolchains lacking <expected>. Scope is deliberately small: T and E are
// trivially-copyable here, so storage is a flag-guarded pair, not a union. No
// expected<void, E>, no monadic ops.



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


//---------------------------------------------------------------------------
// lift — monadic composition for `slim::optional`. `lift(op, args...)` unwraps
// each optional arg, calls `op`, and re-wraps; a nullopt arg short-circuits to
// nullopt. An `op` already returning optional<R> is forwarded as-is. Used
// pervasively by interval/grid/bound arithmetic and rational's operators.
//---------------------------------------------------------------------------
namespace bnd
{
  namespace detail
  {
    template <class T> struct is_slim_optional : std::false_type {};
    template <class T> struct is_slim_optional<slim::optional<T>> : std::true_type {};

    template <class T>
    inline constexpr bool is_slim_optional_v =
        is_slim_optional<std::remove_cvref_t<T>>::value;

    // strip slim::optional<X> down to X, leave non-optional unchanged
    template <class T> struct unwrap { using type = T; };
    template <class T> struct unwrap<slim::optional<T>> { using type = T; };
    template <class T> using unwrap_t = typename unwrap<std::remove_cvref_t<T>>::type;

    template <class T>
    constexpr decltype(auto) lift_unwrap(T&& v)
    {
      if constexpr (is_slim_optional_v<T>)
        return *std::forward<T>(v);
      else
        return std::forward<T>(v);
    }

    template <class T>
    constexpr bool lift_engaged(T const& v)
    {
      if constexpr (is_slim_optional_v<T>)
        return v.has_value();
      else
        return true;
    }

    // expected-like: the std::expected access surface (slim backport or std).
    // The error_type requirement keeps bound and optional out.
    template <typename T>
    concept expected_like = requires(std::remove_cvref_t<T> const& e) {
      typename std::remove_cvref_t<T>::value_type;
      typename std::remove_cvref_t<T>::error_type;
      { e.has_value() } -> std::convertible_to<bool>;
      e.error();
      *e;
    };

    // strip expected<X, E> down to X, leave non-expected unchanged
    template <typename T>
    struct expected_value { using type = std::remove_cvref_t<T>; };
    template <typename T> requires expected_like<T>
    struct expected_value<T>
    { using type = typename std::remove_cvref_t<T>::value_type; };
    template <typename T>
    using expected_value_t = typename expected_value<T>::type;

    template <class T>
    constexpr decltype(auto) expected_unwrap(T const& v)
    {
      if constexpr (expected_like<T>) return *v;
      else                            return v;
    }
  }

  //---------------------------------------------------------------------------
  // ok(expected) — deliberately drop the error cause and enter the zero-cost
  // optional world (sentinel encoding, auto-chaining lift operators):
  //     ok(math::tan(x)) * gain + offset      // optional<bound> chain
  // Works for both the C++20 backport and std::expected.
  //---------------------------------------------------------------------------
  template <typename T, typename E>
  [[nodiscard]] constexpr slim::optional<T> ok(slim::expected<T, E> const& e)
  { return e.has_value() ? slim::optional<T>{*e} : slim::optional<T>{slim::nullopt}; }

  //---------------------------------------------------------------------------
  // lift_expected(op, map_nullopt, lhs, rhs) — binary lift over expected
  // operands: the left error short-circuits; an `op` that returns optional (a
  // division chain) maps its nullopt to `map_nullopt`.
  //---------------------------------------------------------------------------
  template <class Op, class E, class L, class R>
  constexpr auto lift_expected(Op op, E map_nullopt, L const& lhs, R const& rhs)
  {
    using Raw = std::remove_cvref_t<
        decltype(op(detail::expected_unwrap(lhs), detail::expected_unwrap(rhs)))>;
    using Out = detail::unwrap_t<Raw>;
    using Ret = slim::expected<Out, E>;

    if constexpr (detail::expected_like<L>)
      if (!lhs.has_value()) return Ret{slim::unexpected{lhs.error()}};
    if constexpr (detail::expected_like<R>)
      if (!rhs.has_value()) return Ret{slim::unexpected{rhs.error()}};

    auto r = op(detail::expected_unwrap(lhs), detail::expected_unwrap(rhs));
    if constexpr (detail::is_slim_optional_v<Raw>)
      return r.has_value() ? Ret{*r} : Ret{slim::unexpected{map_nullopt}};
    else
      return Ret{r};
  }

  //---------------------------------------------------------------------------
  // lift(op, args...) — call op on the unwrapped args → optional<result>; nullopt
  // if any optional arg is empty. An op already returning optional<R> passes through.
  //---------------------------------------------------------------------------
  template <class Op, class... Args>
  constexpr auto lift(Op op, Args&&... args)
  {
    using R = std::remove_cvref_t<
        decltype(op(detail::lift_unwrap(std::forward<Args>(args))...))>;
    using Ret = std::conditional_t<detail::is_slim_optional_v<R>, R, slim::optional<R>>;

    if (!(detail::lift_engaged(args) && ...))
      return Ret{slim::nullopt};

    if constexpr (detail::is_slim_optional_v<R>)
      return op(detail::lift_unwrap(std::forward<Args>(args))...);
    else
      return Ret{op(detail::lift_unwrap(std::forward<Args>(args))...)};
  }

} // namespace bnd


// ======================================================================
//  bound/detail/rational.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


// ======================================================================
//  bound/math.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// math — primitive numeric utilities: umax/imax, smallest_uint_for /
// smallest_int_for (grid storage selection), the arithmetic/fractional concepts,
// safe_abs, constexpr frexp/ldexp, and abs_fraction (the double → rational
// engine behind rational(double)).
//---------------------------------------------------------------------------
namespace bnd
{
  using umax = std::uint64_t;
  using imax = std::int64_t;

  namespace detail { struct rational; }

  template<typename T>
  concept arithmetic = std::integral<T> || std::floating_point<T> || std::same_as<bnd::detail::rational,T>;

  namespace detail
  {

  // Strict `<` reserves the type's max as the sentinel slot for
  // slim::optional<bound>, so a grid whose max_notch lands on a type's max
  // promotes to the next-wider type (e.g. bound<{0,255}> uses uint16_t, not
  // uint8_t). A valid grid value can thus never collide with the sentinel.
  template <std::uintmax_t N>
  using smallest_uint_for =
    std::conditional_t<(N == 0), rational,
    std::conditional_t<(N < UINT8_MAX),  std::uint8_t,
    std::conditional_t<(N < UINT16_MAX), std::uint16_t,
    std::conditional_t<(N < UINT32_MAX), std::uint32_t,
                                          std::uint64_t>>>>;

  // +1 on min accounts for sentinel value reserved by slim::optional
  template <std::intmax_t Low, std::intmax_t High>
  using smallest_int_for =
    std::conditional_t<(Low >= INT8_MIN+1 && High <= INT8_MAX),   std::int8_t,
    std::conditional_t<(Low >= INT16_MIN+1 && High <= INT16_MAX), std::int16_t,
    std::conditional_t<(Low >= INT32_MIN+1 && High <= INT32_MAX), std::int32_t,
                                                                   std::int64_t>>>;

  // (type_name<T>() — used only by the debug stringifier — lives in
  // "bound/io.hpp" so the core stays free of <string_view>.)

  // Subset of arithmetic excluding integrals — the rhs types that need the
  // rational-arithmetic assignment path. (Named to avoid clashing with `real`.)
  template<typename T>
  concept fractional = std::floating_point<T> || std::same_as<rational, T>;

  template <std::signed_integral V>
  constexpr umax safe_abs(V value)
  { return (value >= 0) ? static_cast<umax>(value) : umax{0} - static_cast<umax>(value); }

  inline constexpr double frexp(double value, int* exp) noexcept
  {
    if (value == 0.0)
    {
        *exp = 0;
        return value;
    }

    auto bits = std::bit_cast<std::uint64_t>(value);
    constexpr std::uint64_t mantissa_mask = 0x000F'FFFF'FFFF'FFFF;
    constexpr std::uint64_t sign_mask     = 0x8000'0000'0000'0000;
    auto e = static_cast<int>((bits >> 52) & 0x7FF);

    if (e == 0x7FF) {
        *exp = 0;
        return value;
    }

    if (e == 0) {
        // subnormal: scale up, recurse
        double scaled = value * 0x1p53;
        double result = frexp(scaled, exp);
        *exp -= 53;
        return result;
    }

    *exp = e - 0x3FE;
    bits = (bits & (sign_mask | mantissa_mask)) | (std::uint64_t{0x3FE} << 52);
    return std::bit_cast<double>(bits);
  }

  constexpr double ldexp(double value, int exp) noexcept {
      if (value == 0.0 || exp == 0)
          return value;

      auto bits = std::bit_cast<std::uint64_t>(value);
      constexpr std::uint64_t sign_mask     = 0x8000'0000'0000'0000;
      constexpr std::uint64_t mantissa_mask = 0x000F'FFFF'FFFF'FFFF;
      auto e = static_cast<int>((bits >> 52) & 0x7FF);

      if (e == 0x7FF)
          return value; // inf or NaN

      // Normalize subnormals
      int extra = 0;
      if (e == 0) {
          bits = std::bit_cast<std::uint64_t>(value * 0x1p53);
          e = static_cast<int>((bits >> 52) & 0x7FF);
          extra = -53;
      }

      int new_exp = e + exp + extra;

      if (new_exp >= 0x7FF) {
          // overflow → ±inf
          return (bits & sign_mask) ? -std::numeric_limits<double>::infinity()
                                   : std::numeric_limits<double>::infinity();
      }

      if (new_exp > 0) {
          // normal result
          bits = (bits & (sign_mask | mantissa_mask))
               | (static_cast<std::uint64_t>(new_exp) << 52);
          return std::bit_cast<double>(bits);
      }

      // Subnormal or underflow
      auto mantissa = (bits & mantissa_mask) | (std::uint64_t{1} << 52);
      int shift = 1 - new_exp;

      if (shift > 53)
          return std::bit_cast<double>(bits & sign_mask); // ±0

      // Round-to-nearest-even
      std::uint64_t dropped = mantissa & ((std::uint64_t{1} << shift) - 1);
      mantissa >>= shift;
      std::uint64_t halfway = std::uint64_t{1} << (shift - 1);
      if (dropped > halfway || (dropped == halfway && (mantissa & 1)))
          ++mantissa;

      bits = (bits & sign_mask) | mantissa;
      return std::bit_cast<double>(bits);
  }

  // Freestanding finite check: `v - v` is 0 for every finite v, NaN otherwise
  // (and inf - inf is NaN). Avoids <cmath>/std::isfinite so the core stays
  // bare-metal clean; the same idiom is used in the store paths.
  constexpr bool is_finite(double v) noexcept { return v - v == 0; }

  // Exact conversion of a finite double to num/den (den a power of two), the
  // engine behind rational(double). A finite double is exactly
  // `significand · 2^exp2` (a 53-bit significand), both read straight from the
  // IEEE-754 bits — no <cmath>, no FPU rounding, bit-identical across platforms.
  constexpr std::pair<umax, umax> abs_fraction(double value)
  {
    if (not is_finite(value))
      raise(errc::not_finite, "bnd::detail::abs_fraction: non-finite double");

    if (value == 0.0) return {0, 1};
    if (value < 0)    value = -value;        // |value|; sign is the caller's job

    const auto bits = std::bit_cast<std::uint64_t>(value);
    const int  e    = static_cast<int>((bits >> 52) & 0x7FF);
    umax significand = bits & 0x000F'FFFF'FFFF'FFFF;
    int  exp2;
    if (e == 0)                              // subnormal: no implicit leading 1
      exp2 = 1 - 1023 - 52;
    else                                     // normal: restore the implicit bit
    {
      significand |= (umax{1} << 52);
      exp2 = e - 1023 - 52;
    }

    // value == significand * 2^exp2. Re-express as num/den with den = 2^k.
    if (exp2 >= 0)                           // integer-valued: scale up, den = 1
      return {significand << exp2, 1};

    // exp2 < 0 → den = 2^(-exp2), capped at 2^62 (den is stored signed). Beyond
    // the cap the value is too small to keep: drop the significand's low bits
    // (shift ≥ 64 folds to zero). Reachable for every subnormal and normals
    // below ~2^-62, where the significand collapses to 0 (0/d canonicalises to 0/1).
    int den_pow = -exp2;
    constexpr int max_pow = 62;
    if (den_pow > max_pow)
    {
      const int drop = den_pow - max_pow;
      significand = (drop >= 64) ? 0 : (significand >> drop);
      // Return canonical {0,1}: callers take this verbatim, so a non-canonical
      // {0, 2^62} would break the structural rational::operator==.
      if (significand == 0) return {0, 1};
      den_pow = max_pow;
    }
    umax den = umax{1} << den_pow;

    // den is a power of two, so reduce by cancelling shared factors of two.
    while (significand && (significand & 1) == 0 && den != 1)
    {
      significand >>= 1;
      den >>= 1;
    }
    return {significand, den};
  }

  } // namespace detail
} // namespace bnd


// ======================================================================
//  bound/detail/overflow.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// This is derived from Peter Sommerlads odins.h, allowed by MIT license
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// overflow — overflow-detecting add/sub/mul for integers. Wraps
// __builtin_*_overflow when available, with portable non_builtin_* fallbacks.
// Used by rational::*_impl and every checked arithmetic path.
//---------------------------------------------------------------------------

#if defined(__has_builtin)
  #if __has_builtin(__builtin_add_overflow) \
   && __has_builtin(__builtin_sub_overflow) \
   && __has_builtin(__builtin_mul_overflow)
    #define BOUND_HAVE_BUILTIN 1
  #endif
#endif

// GCC 5.0+ ships these builtins even where __has_builtin is unavailable.
#if !defined(BOUND_HAVE_BUILTIN) && defined(__GNUC__) && __GNUC__ >= 5
  #define BOUND_HAVE_BUILTIN 1
#endif

namespace bnd
{
  template<std::integral T>
  [[nodiscard]] constexpr bool non_builtin_add_overflow(T l, T r, T* result) noexcept
  {
    if constexpr (std::numeric_limits<T>::is_signed)
    {
      if constexpr(sizeof(T) == sizeof(std::int64_t))
      {
        *result = static_cast<T>(static_cast<uint64_t>(l) + static_cast<uint64_t>(r));
        if (l < 0)
          return (r<0) && (*result > l);
        else
          return (r >= 0) && (*result < l);
      }
      else
      {
        std::int64_t res {l};
        res += r;
        *result = static_cast<T>(res);
        return res != *result;
      }
    }
    else
    { // unsigned
      if constexpr(sizeof(T) == sizeof(std::uint64_t))
      {
        *result = l + r;
        return *result < l; // wrapped when true
      }
      else
      {
        std::uint64_t res {l};
        res += r;
        *result = static_cast<T>(res);
        return res != *result;
      }
    }
  }

  template<std::integral T>
  [[nodiscard]] constexpr bool non_builtin_mul_overflow(T l, T r, T* result) noexcept
  {
    if constexpr (std::numeric_limits<T>::is_signed)
    {
      if constexpr(sizeof(T) == sizeof(std::int64_t))
      {
        bool resultnegative { (l < 0) != (r < 0) };
        uint64_t res{};
        auto abs64 { [](int64_t value) -> uint64_t { return value < 0? 1ULL + ~static_cast<uint64_t>(value):static_cast<uint64_t>(value);} };
        if (not non_builtin_mul_overflow(abs64(l), abs64(r), &res))
        {
          if (resultnegative)
          {
            if (res <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1ull)
            {
              *result = static_cast<T>(1ULL + ~res); // two's complement
              return false;
            }
          }
          else
          {
            if (res <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
            {
              *result = static_cast<T>(res);
              return false;
            }
          }
        }
        return true; // overflow
      }
      else
      {
        std::int64_t res {l};
        res *= r;
        *result = static_cast<T>(res);
        return res != *result; // detect overflow bits
      }
    }
    else
    { // unsigned
      if constexpr(sizeof(T) == sizeof(std::uint64_t))
      {
        // compute high-parts and low-parts
        uint64_t lhigh { l >> 32 };
        uint64_t llow { l & 0xffff'ffffULL} ;
        uint64_t rhigh { r >> 32 };
        uint64_t rlow { r & 0xffff'ffffULL} ;
        if (lhigh > 0 && rhigh > 0) return true;
        uint64_t high_low{ lhigh>0 ? lhigh*rlow : rhigh*llow };
        if (high_low >> 32) return true; // overflow
        uint64_t low_low { llow * rlow } ;
        *result = (high_low << 32) + low_low;

        return *result < low_low; // detect overflow
      }
      else
      {
        std::uint64_t res {l};
        res *= r;
        *result = static_cast<T>(res);
        return res != *result;
      }
    }
  }

  template<std::integral T>
  [[nodiscard]] constexpr bool non_builtin_sub_overflow(T l, T r, T* result) noexcept
  {
    if constexpr (std::numeric_limits<T>::is_signed)
    {
      // Negating T::min() is signed-overflow UB, so detect it before negating:
      // when r == T::min(), `l - r` fits only if l < 0.
      if (r == std::numeric_limits<T>::min())
      {
        if (l >= 0) return true;
        *result = static_cast<T>(l - r);
        return false;
      }
      return non_builtin_add_overflow(l, static_cast<T>(-r), result);
    }
    else
    {
      *result = static_cast<T>(l - r);
      return r > l;
    }
  }

#ifdef BOUND_HAVE_BUILTIN
  template<std::integral T>
  [[nodiscard]]
  constexpr bool add_overflow(T l, T r, T* result) noexcept
  { return __builtin_add_overflow(l,r,result); }

  template<std::integral T>
  [[nodiscard]]
  constexpr bool sub_overflow(T l, T r, T* result) noexcept
  { return __builtin_sub_overflow(l,r,result); }

  template<std::integral T>
  [[nodiscard]]
  constexpr bool mul_overflow(T l, T r, T* result) noexcept
  { return __builtin_mul_overflow(l,r,result); }
#else // DIY
  template<std::integral T>
  [[nodiscard]]
  constexpr bool add_overflow(T l, T r, T* result) noexcept
  { return non_builtin_add_overflow(l,r,result); }

  template<std::integral T>
  [[nodiscard]]
  constexpr bool sub_overflow(T l, T r, T* result) noexcept
  { return non_builtin_sub_overflow(l,r,result); }

  template<std::integral T>
  [[nodiscard]]
  constexpr bool mul_overflow(T l, T r, T* result) noexcept
  { return non_builtin_mul_overflow(l,r,result); }
#endif
} // namespace bnd




namespace bnd::detail { struct rational; }

namespace slim
{
  template<>
  struct sentinel_traits<bnd::detail::rational>
  {
    protected:
      static constexpr bnd::detail::rational sentinel() noexcept;
      static constexpr bool is_sentinel(const bnd::detail::rational& v) noexcept;
  };
} // namespace slim

namespace bnd::detail
{
  constexpr umax abs_den(imax d) { return (d >= 0) ? static_cast<umax>(d) : umax{0} - static_cast<umax>(d); }

  // Portable 64×64 → 128-bit unsigned product, as {hi, lo}. Used where a native
  // unsigned __int128 is unavailable (MSVC). Schoolbook 32-bit split — the same
  // construction trusted in cmath.hpp's fmul, so it stays bit-exact.
  struct u128 { umax hi; umax lo; };
  constexpr u128 umul(umax a, umax b)
  {
    umax al = a & 0xffffffffu, ah = a >> 32;
    umax bl = b & 0xffffffffu, bh = b >> 32;
    umax ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;
    umax mid = (ll >> 32) + (lh & 0xffffffffu) + (hl & 0xffffffffu);
    return { hh + (lh >> 32) + (hl >> 32) + (mid >> 32),
             (ll & 0xffffffffu) | (mid << 32) };
  }
  constexpr std::strong_ordering cmp128(u128 a, u128 b)
  { return (a.hi != b.hi) ? (a.hi <=> b.hi) : (a.lo <=> b.lo); }

  //---------------------------------------------------------------------------
  // trim
  //---------------------------------------------------------------------------
  inline constexpr void trim(umax& numerator, imax& denominator)
  {
    umax ad = abs_den(denominator);
    if (ad <= 1)
      return;
    auto g = std::gcd(numerator, ad);
    if (g <= 1)
      return;
    numerator /= g;
    ad /= g;
    denominator = (denominator < 0) ? -ad : ad;
  }

  inline constexpr void trim(umax& a, umax& b)
  {
    if (a <= 1 || b <= 1)
      return;
    auto g = std::gcd(a, b);
    if (g <= 1)
      return;
    a /= g;
    b /= g;
  }

  constexpr slim::optional<rational> operator+(rational const&, rational const&);
  constexpr slim::optional<rational> operator/(rational const&, rational const&);
  constexpr slim::optional<rational> operator-(rational const&, rational const&);

  constexpr slim::optional<rational> operator*(rational const&, rational const&);
  constexpr auto     operator<=>(rational, rational) -> std::strong_ordering;

  //---------------------------------------------------------------------------
  // Overflow / malformed-literal signalling
  //---------------------------------------------------------------------------
  // Failure aborts constant evaluation via `detail::constexpr_error<Msg>()` — a
  // non-constexpr [[noreturn]] helper carrying the message in an NTTP (literal
  // parsers and the checked paths under `if (std::is_constant_evaluated())`),
  // hard-failing the build with the text in the diagnostic; at runtime those
  // paths fall through to `nullopt`. No `throw`, so it is -fno-exceptions clean.

  //---------------------------------------------------------------------------
  // rational — structural type for NTTP (public members only). Sign is encoded
  // in the denominator (negative = negative rational); numerator is unsigned to
  // represent e.g. umax itself.
  //---------------------------------------------------------------------------
  struct rational
  {
    umax Numerator;
    imax Denominator;

    constexpr rational() = default;
    constexpr rational(std::floating_point  auto);
    constexpr rational(std::signed_integral auto, imax = 1);
    constexpr rational(std::unsigned_integral auto num, imax den = 1)
     :Numerator{num}, Denominator{den}
    { canonicalize(Numerator, Denominator); }

    // Two-unsigned overload: lets `rational{i, N}` accept two `size_t`
    // operands without forcing the caller to `static_cast<imax>` the
    // numerator. Numerator is unsigned anyway; the cast on `den` is
    // safe because the unsigned-integral concept exclude negative inputs.
    template <std::unsigned_integral N, std::unsigned_integral D>
    constexpr rational(N num, D den)
     :Numerator{num}, Denominator{static_cast<imax>(den)}
    { canonicalize(Numerator, Denominator); }

    // Implicit unwrap of a checked result, so coefficient expressions read as
    // plain arithmetic (`rational two_pi = 2 * pi;`); empty optional (overflow) is
    // a compile error in constant evaluation, a throw at runtime. same_as-constrained
    // (not a plain `rational(optional<rational>)`) because optional's own converting
    // ctor is gated on `is_constructible_v<rational, U>` — a non-template overload
    // would make that trait depend on itself.
    template <class O>
      requires std::same_as<std::remove_cvref_t<O>, slim::optional<rational>>
    constexpr rational(O&& o) : rational(o.value()) {}

    // operator== by default for structural type
    constexpr bool operator==(const rational&) const = default;
    template <arithmetic T>
    constexpr bool operator==(T value) const { return operator==(rational{value}); }

    constexpr rational operator-() const;

    template <std::unsigned_integral T>
    constexpr slim::expected<T, errc> to() const;

    template <std::unsigned_integral T>
    explicit constexpr operator T () const
    {
      if (Denominator < 0)
        raise(errc::domain_error, "cannot convert negative rational to unsigned");
      return Numerator / abs_den(Denominator);
    }

    template <std::signed_integral T>
    explicit constexpr operator T () const
    {
      umax q = Numerator / abs_den(Denominator);
      return (Denominator < 0) ? -q : q;
    }

    template <std::floating_point T>
    explicit constexpr operator T () const
    {
      T q = static_cast<T>(Numerator) / static_cast<T>(abs_den(Denominator));
      return (Denominator < 0) ? -q : q;
    }

    // allow unary+ for generic programming
    constexpr rational operator+() const { return *this; }

    // Compound-assign: forward to the checked binary op and unwrap via .value()
    // — overflow surfaces as slim::bad_optional_access (no error channel here).
    constexpr rational& operator+=(rational const& rhs);
    constexpr rational& operator-=(rational const& rhs);
    constexpr rational& operator*=(rational const& rhs);
    constexpr rational& operator/=(rational const& rhs);

    // The sentinel slot is {N, 0}; only make_sentinel() can produce one.
    [[nodiscard]] constexpr bool is_sentinel() const noexcept
    { return Denominator == 0; }

    [[nodiscard]] static constexpr rational make_sentinel() noexcept
    { rational r; r.Numerator = 1; r.Denominator = 0; return r; }

    // Unchecked arithmetic — caller takes responsibility for non-overflow
    // (and non-zero operand for div_unchecked / inv_unchecked).
    static constexpr rational add_unchecked(rational, rational);
    static constexpr rational mul_unchecked(rational, rational);
    static constexpr rational div_unchecked(rational, rational);
    static constexpr rational inv_unchecked(rational);

    static constexpr slim::optional<rational> add(rational a, rational b)
    { return a + b; }
    static constexpr slim::optional<rational> inv(rational);

    // Shared algorithm bodies. Checked=true returns optional<rational>, reporting
    // overflow (throw at compile time / nullopt at runtime); Checked=false
    // silently overflows — the caller must guarantee its absence.
    template <bool Checked> static constexpr auto add_impl(rational const&, rational const&);
    template <bool Checked> static constexpr auto mul_impl(rational const&, rational const&);
    template <bool Checked> static constexpr auto div_impl(rational const&, rational const&);
    template <bool Checked> static constexpr auto inv_impl(rational const&);

  private:
    // Domain check + canonical-zero + gcd reduction; used by the integral ctors.
    // Two domain errors: Denominator == 0 (undefined; also the reserved sentinel)
    // and Denominator == imax_min (cannot be negated without UB, which every
    // sign-flip in the file assumes is well-defined).
    static constexpr void canonicalize(umax& num, imax& den)
    {
      if (den == 0)
        raise(errc::domain_error, "Denominator of Zero is invalid");
      if (den == std::numeric_limits<imax>::min())
        raise(errc::domain_error, "Denominator imax_min is invalid (cannot be negated)");
      if (num == 0) den = 1;
      trim(num, den);
    }

    // Signed-encoded denominator for the signed ctor, validated BEFORE the
    // negation so `-den` is never UB (the ctor-body canonicalize() would catch
    // these too, but only after the mem-init already evaluated `-den`).
    static constexpr imax signed_den_from(std::signed_integral auto num, imax den)
    {
      if (den == 0)
        raise(errc::domain_error, "Denominator of Zero is invalid");
      if (den == std::numeric_limits<imax>::min())
        raise(errc::domain_error, "Denominator imax_min is invalid (cannot be negated)");
      return (num < 0) ? -den : den;
    }
  };


  [[nodiscard]] constexpr slim::optional<rational> gcd(rational const&, rational const&);
  [[nodiscard]] constexpr rational abs(rational);

  [[nodiscard]] constexpr bool divides_evenly(rational const&, rational const&);

  //---------------------------------------------------------------------------
  // sign / named integer reductions — free functions over the public
  // numerator/denominator (structural type), siblings of abs/gcd. Reductions
  // are explicit, lossy alternatives to `static_cast`:
  // trunc → 0; floor → -inf; ceil → +inf; round → half-away-from-zero.
  //---------------------------------------------------------------------------
  // -1 / 0 / +1 — single source of truth for the sign convention (sign lives in
  // Denominator; canonical zero is {0, 1}).
  [[nodiscard]] constexpr int sign(rational v) noexcept
  {
    if (v.Numerator == 0) return 0;
    return (v.Denominator < 0) ? -1 : 1;
  }

  [[nodiscard]] constexpr imax trunc(rational v)
  {
    umax q = v.Numerator / abs_den(v.Denominator);
    return (v.Denominator < 0) ? -q : q;
  }

  [[nodiscard]] constexpr imax floor(rational v)
  {
    umax ad = abs_den(v.Denominator);
    umax q = v.Numerator / ad;
    umax rem = v.Numerator % ad;
    // negative with non-zero remainder: step one further toward -inf
    if (v.Denominator < 0 && rem != 0)
      return -q - 1;
    return (v.Denominator < 0) ? -q : q;
  }

  [[nodiscard]] constexpr imax ceil(rational v)
  {
    umax ad  = abs_den(v.Denominator);
    umax q   = v.Numerator / ad;
    umax rem = v.Numerator % ad;
    // negative value: ceiling toward +inf coincides with truncation toward zero
    if (v.Denominator < 0)
      return -q;
    // positive with non-zero remainder: step one further toward +inf
    return q + (rem != 0 ? 1 : 0);
  }

  [[nodiscard]] constexpr imax round(rational v)
  {
    umax ad = abs_den(v.Denominator);
    umax q = v.Numerator / ad;
    umax rem = v.Numerator % ad;
    // half-away-from-zero: bump magnitude when 2*rem >= ad
    if (rem * 2 >= ad) ++q;
    return (v.Denominator < 0) ? -q : q;
  }

  //---------------------------------------------------------------------------
  // abs
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr rational abs(rational v)
  { if (v.Denominator < 0) v.Denominator = -v.Denominator; return v; }

  //---------------------------------------------------------------------------
  // gcd
  //---------------------------------------------------------------------------
  // Returns nullopt if the combined denominator lcm = (a/gcd)·b would exceed
  // imax_max (sign-bit reservation) — traps the mul_overflow then range-checks.
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr slim::optional<rational> gcd(rational const& lhs, rational const& rhs)
  {
    umax a = abs_den(lhs.Denominator);
    umax b = abs_den(rhs.Denominator);
    umax g = std::gcd(a, b);

    umax denominator;
    if (mul_overflow(a / g, b, &denominator))
      return slim::nullopt;
    if (denominator > static_cast<umax>(std::numeric_limits<imax>::max()))
      return slim::nullopt;

    auto numerator = std::gcd(lhs.Numerator, rhs.Numerator);
    return rational{numerator, denominator};
  }

  //---------------------------------------------------------------------------
  // rational::rational
  //---------------------------------------------------------------------------
  constexpr rational::rational(std::signed_integral auto num, imax den)
   :Numerator{safe_abs(num)},
    Denominator{ signed_den_from(num, den) }
  { canonicalize(Numerator, Denominator); }

  constexpr rational::rational(std::floating_point auto value)
  {
    if (not is_finite(value))
      raise(errc::not_finite, "non-finite double");

    if (value == 0.0)
    {
      Numerator = 0;
      Denominator = 1;
      return;
    }

    bool neg = (value < 0.0);
    if (neg) value = -value;

    auto [num, den] = abs_fraction(value);
    Numerator = num;
    Denominator = neg ? -den : den;
    // trim not needed, because abs_fraction already trims in its special case
  }

  //---------------------------------------------------------------------------
  // to
  //---------------------------------------------------------------------------
  template <std::unsigned_integral T>
  constexpr slim::expected<T, errc> rational::to() const
  {
    if (is_sentinel())   return slim::unexpected{errc::not_a_value};
    if (Denominator < 0) return slim::unexpected{errc::domain_error};
    return static_cast<T>(Numerator / abs_den(Denominator));
  }

  //---------------------------------------------------------------------------
  // _b / _r literal parser — shared between bound.hpp's `_b` and `_r` below.
  // Accepts:
  //   integer:           5, 1'000
  //   decimal:           1.25, .5
  //   decimal scientific 1.5e2, 2.5e-1
  //   hex integer:       0xff
  //   binary integer:    0b1010
  //   hex float (Q-fmt): 0x1p15, 0x1p-15, 0x1.8p3
  // Exact (no double round-trip). Overflow -> consteval throw.
  //---------------------------------------------------------------------------
  namespace _detail
  {
    consteval int parse_digit(char c, int base)
    {
      if (c >= '0' && c <= '9')
      {
        int d = c - '0';
        return d < base ? d : -1;
      }
      if (base == 16)
      {
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
      }
      return -1;
    }

    template<char... Chars>
    consteval rational parse_b_literal()
    {
      constexpr char src[] = { Chars..., '\0' };
      constexpr std::size_t N = sizeof...(Chars);

      // Detect radix prefix.
      int base = 10;
      std::size_t i = 0;
      if (N >= 2 && src[0] == '0')
      {
        if (src[1] == 'x' || src[1] == 'X') { base = 16; i = 2; }
        else if (src[1] == 'b' || src[1] == 'B') { base = 2; i = 2; }
      }

      umax num = 0;
      int frac_len = 0;
      bool in_frac = false;
      int exp = 0;
      bool exp_neg = false;
      bool has_p_exp = false;  // 2^exp (hex floats)
      bool has_e_exp = false;  // 10^exp (decimal scientific)
      bool in_exp = false;
      bool exp_seen_digit = false;

      for (; i < N; ++i)
      {
        char c = src[i];
        if (c == '\'') continue;

        if (in_exp)
        {
          if (!exp_seen_digit && (c == '+' || c == '-'))
          {
            exp_neg = (c == '-');
            continue;
          }
          if (c >= '0' && c <= '9')
          {
            exp = exp * 10 + (c - '0');
            exp_seen_digit = true;
            continue;
          }
          constexpr_error<"_b/_r literal: invalid char in exponent">();
        }

        if (c == '.')
        {
          if (in_frac) constexpr_error<"_b/_r literal: multiple '.'">();
          if (base == 2) constexpr_error<"_b/_r literal: '.' not allowed in binary">();
          in_frac = true;
          continue;
        }

        if ((c == 'p' || c == 'P') && base == 16)
        {
          has_p_exp = true;
          in_exp = true;
          continue;
        }

        if ((c == 'e' || c == 'E') && base == 10)
        {
          has_e_exp = true;
          in_exp = true;
          continue;
        }

        int d = parse_digit(c, base);
        if (d < 0) constexpr_error<"_b/_r literal: invalid digit for radix">();

        umax base_u = base;
        if (num > (~umax{0} - d) / base_u)
          constexpr_error<"_b/_r literal: numerator overflow">();
        num = num * base_u + d;
        if (in_frac) ++frac_len;
      }

      // Build denominator from fractional part.
      // For decimal: den = 10^frac_len. For hex: den = 2^(4*frac_len).
      umax den = 1;
      if (base == 10)
      {
        for (int k = 0; k < frac_len; ++k)
        {
          if (den > (~umax{0}) / 10u)
            constexpr_error<"_b/_r literal: denominator overflow">();
          den *= 10u;
        }
      }
      else if (base == 16)
      {
        int shift = 4 * frac_len;
        if (shift >= 64) constexpr_error<"_b/_r literal: hex fraction too long">();
        den <<= shift;
      }

      // Apply binary exponent (hex floats, `p`).
      if (has_p_exp)
      {
        if (exp >= 63) constexpr_error<"_b/_r literal: p exponent too large">();
        if (!exp_neg) num <<= exp;
        else
        {
          if (den > (~umax{0}) >> exp)
            constexpr_error<"_b/_r literal: p exponent denominator overflow">();
          den <<= exp;
        }
      }

      // Apply decimal exponent (decimal scientific, `e`).
      if (has_e_exp)
      {
        for (int k = 0; k < exp; ++k)
        {
          if (!exp_neg)
          {
            if (num > (~umax{0}) / 10u)
              constexpr_error<"_b/_r literal: e exponent numerator overflow">();
            num *= 10u;
          }
          else
          {
            if (den > (~umax{0}) / 10u)
              constexpr_error<"_b/_r literal: e exponent denominator overflow">();
            den *= 10u;
          }
        }
      }

      return rational{num, den};
    }
  }

  template<char... Chars>
  constexpr rational operator ""_r() { return _detail::parse_b_literal<Chars...>(); }

  // notch<N, D> is defined publicly in `namespace bnd` (see the re-export block
  // at the end of this header) so consumers spell it without naming the
  // internal representation type.

  //---------------------------------------------------------------------------
  // add_impl / mul_impl / div_impl — shared bodies (Checked toggles overflow)
  //---------------------------------------------------------------------------
  template <bool Checked>
  inline constexpr auto rational::add_impl(rational const& a, rational const& b)
  {
    using ret_t = std::conditional_t<Checked, slim::optional<rational>, rational>;

    if (a == -b) return ret_t{0_r};
    if (a.Numerator == 0) return ret_t{b};
    if (b.Numerator == 0) return ret_t{a};

    bool a_neg = a.Denominator < 0;
    bool b_neg = b.Denominator < 0;
    umax a_ad = abs_den(a.Denominator);
    umax b_ad = abs_den(b.Denominator);

    if (a_ad == b_ad)
    {
      if (a_neg == b_neg)
      {
        umax numerator;
        if constexpr (Checked)
        {
          if (add_overflow(a.Numerator, b.Numerator, &numerator))
          {
            if (std::is_constant_evaluated()) { constexpr_error<"rational +: numerator overflow (same denominator)">(); }
            return ret_t{slim::nullopt};
          }
        }
        else
          numerator = a.Numerator + b.Numerator;

        rational r;
        r.Numerator = numerator;
        r.Denominator = a.Denominator;
        trim(r.Numerator, r.Denominator);
        return ret_t{r};
      }

      umax num = (a.Numerator > b.Numerator) ? (a.Numerator - b.Numerator)
                                              : (b.Numerator - a.Numerator);
      bool r_neg = a_neg ? (a.Numerator > b.Numerator) : (b.Numerator > a.Numerator);
      if (num == 0) return ret_t{0_r};
      rational r;
      r.Numerator = num;
      r.Denominator = r_neg ? -a_ad : a_ad;
      trim(r.Numerator, r.Denominator);
      return ret_t{r};
    }

    // Common denominator = lcm(a_ad, b_ad) = a_ad·(b_ad/g), not a_ad·b_ad: the
    // reduced cofactors (g = gcd) overflow far less often than the raw product.
    umax g     = std::gcd(a_ad, b_ad);
    umax a_ad_r = a_ad / g;       // = a_ad / gcd; coprime with b_ad_r
    umax b_ad_r = b_ad / g;

    umax denominator;
    umax A;
    umax B;

    if constexpr (Checked)
    {
      if (mul_overflow(a_ad, b_ad_r, &denominator)    ||   // = lcm(a_ad, b_ad)
          mul_overflow(a.Numerator, b_ad_r, &A)       ||
          mul_overflow(b.Numerator, a_ad_r, &B)       ||
          denominator > static_cast<umax>(std::numeric_limits<imax>::max()))
      {
        if (std::is_constant_evaluated()) { constexpr_error<"rational +: cross-multiplication overflow">(); }
        return ret_t{slim::nullopt};
      }
    }
    else
    {
      denominator = a_ad * b_ad_r;
      A = a.Numerator * b_ad_r;
      B = b.Numerator * a_ad_r;
    }

    if (a_neg == b_neg)
    {
      umax numerator;
      if constexpr (Checked)
      {
        if (add_overflow(A, B, &numerator))
        {
          if (std::is_constant_evaluated()) { constexpr_error<"rational +: numerator sum overflow">(); }
          return ret_t{slim::nullopt};
        }
      }
      else
        numerator = A + B;

      // num, den both > 0 here, so the ctor's domain/zero checks are dead;
      // assemble directly and trim.
      rational r;
      r.Numerator   = numerator;
      r.Denominator = a_neg ? -denominator
                             :  denominator;
      trim(r.Numerator, r.Denominator);
      return ret_t{r};
    }

    // numerator == 0 (exact cancellation) is unreachable here: it would
    // require a == -b, which the `a == -b` early-return at the top of
    // add_impl already handles for canonical inputs.
    umax numerator = (A > B) ? (A - B) : (B - A);
    bool r_neg = a_neg ? (A > B) : (B > A);
    rational r;
    r.Numerator   = numerator;
    r.Denominator = r_neg ? -denominator
                           :  denominator;
    trim(r.Numerator, r.Denominator);
    return ret_t{r};
  }

  template <bool Checked>
  inline constexpr auto rational::mul_impl(rational const& a_in, rational const& b_in)
  {
    using ret_t = std::conditional_t<Checked, slim::optional<rational>, rational>;
    rational a = a_in, b = b_in;

    if (a.Numerator == 0 || b.Numerator == 0) return ret_t{0_r};

    bool r_neg = (a.Denominator < 0) != (b.Denominator < 0);
    umax a_ad = abs_den(a.Denominator);
    umax b_ad = abs_den(b.Denominator);

    if (a_ad == 1 && b_ad == 1)
    {
      umax numerator;
      if constexpr (Checked)
      {
        if (mul_overflow(a.Numerator, b.Numerator, &numerator))
        {
          if (std::is_constant_evaluated()) { constexpr_error<"rational *: numerator overflow">(); }
          return ret_t{slim::nullopt};
        }
      }
      else
        numerator = a.Numerator * b.Numerator;

      rational r;
      r.Numerator = numerator;
      r.Denominator = r_neg ? imax{-1} : imax{1};
      return ret_t{r};
    }

    trim(a.Numerator, b_ad);
    trim(b.Numerator, a_ad);

    umax numerator;
    umax denominator;
    if constexpr (Checked)
    {
      if (mul_overflow(a.Numerator, b.Numerator, &numerator) ||
          mul_overflow(a_ad, b_ad, &denominator)             ||
          denominator > static_cast<umax>(std::numeric_limits<imax>::max()))
      {
        if (std::is_constant_evaluated()) { constexpr_error<"rational *: numerator or denominator overflow">(); }
        return ret_t{slim::nullopt};
      }
    }
    else
    {
      numerator = a.Numerator * b.Numerator;
      denominator = a_ad * b_ad;
    }

    // The cross-trims above guarantee gcd(numerator, denominator) == 1, so
    // bypass rational(num, den) and skip its redundant trim.
    rational r;
    r.Numerator   = numerator;
    r.Denominator = r_neg ? -denominator
                           :  denominator;
    return ret_t{r};
  }

  //---------------------------------------------------------------------------
  // inv_impl — multiplicative inverse (1 / a)
  //---------------------------------------------------------------------------
  // a is already trimmed (Numerator and |Denominator| coprime), so the swapped
  // pair is also trimmed. Sign lives in the denominator and 1/(-x) has the same
  // sign as -x, so the sign bit moves with the (now) denominator unchanged.
  template <bool Checked>
  inline constexpr auto rational::inv_impl(rational const& a)
  {
    using ret_t = std::conditional_t<Checked, slim::optional<rational>, rational>;

    if constexpr (Checked)
    {
      // a.Numerator goes into the result's Denominator slot, so it must fit in
      // imax (else the umax→imax conversion wraps and a later -Denominator is UB).
      if (a.Numerator == 0 ||
          a.Numerator > static_cast<umax>(std::numeric_limits<imax>::max()))
      {
        if (std::is_constant_evaluated()) { constexpr_error<"rational inv: numerator zero or out of denominator range">(); }
        return ret_t{slim::nullopt};
      }
    }

    rational r;
    r.Numerator   = abs_den(a.Denominator);
    r.Denominator = (a.Denominator < 0) ? -a.Numerator : a.Numerator;
    return ret_t{r};
  }

  // div(a, b) = a * inv(b). The checked path goes through inv_impl<true> so
  // the b.Numerator-fits-in-imax check (added there) propagates here too;
  // the unchecked path skips it (caller's contract).
  template <bool Checked>
  inline constexpr auto rational::div_impl(rational const& a, rational const& b)
  {
    using ret_t = std::conditional_t<Checked, slim::optional<rational>, rational>;

    if constexpr (Checked)
    {
      auto inv_b = inv_impl<true>(b);
      if (!inv_b.has_value()) return ret_t{slim::nullopt};
      return mul_impl<true>(a, *inv_b);
    }
    else
      return mul_impl<false>(a, inv_impl<false>(b));
  }

  //---------------------------------------------------------------------------
  // unchecked rational arithmetic — caller guarantees: no umax overflow on the
  // products, no zero divisor/numerator, and the result Denominator fits in imax.
  // The checked variants enforce all three; unchecked skips them.
  //---------------------------------------------------------------------------
  inline constexpr rational rational::add_unchecked(rational a, rational b)
  { return add_impl<false>(a, b); }

  inline constexpr rational rational::mul_unchecked(rational a, rational b)
  { return mul_impl<false>(a, b); }

  inline constexpr rational rational::div_unchecked(rational a, rational b)
  { return div_impl<false>(a, b); }

  inline constexpr rational rational::inv_unchecked(rational a)
  { return inv_impl<false>(a); }

  inline constexpr slim::optional<rational> rational::inv(rational a)
  { return inv_impl<true>(a); }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  inline constexpr rational rational::operator-() const
  {
    if (Numerator == 0)
      return *this;

    // Already trimmed; flip the sign-encoding directly without re-running trim.
    rational r;
    r.Numerator = Numerator;
    r.Denominator = -Denominator;
    return r;
  }

  //---------------------------------------------------------------------------
  // operator<=>
  //---------------------------------------------------------------------------
  inline constexpr auto operator<=>(rational lhs, rational rhs) -> std::strong_ordering
  {
    int lhs_sign = sign(lhs);
    int rhs_sign = sign(rhs);

    if (lhs_sign != rhs_sign)
      return lhs_sign <=> rhs_sign;

    if (lhs_sign == 0)
      return std::strong_ordering::equal;

    // signs are equal here (the `lhs_sign != rhs_sign` branch returned above)
    bool lhs_neg = lhs_sign < 0;

    umax lhs_ad = abs_den(lhs.Denominator);
    umax rhs_ad = abs_den(rhs.Denominator);

    // integer comparison: skip cross-multiply entirely
    if (lhs_ad == 1 && rhs_ad == 1)
    {
      if (lhs_neg)
        return rhs.Numerator <=> lhs.Numerator;
      else
        return lhs.Numerator <=> rhs.Numerator;
    }

    // One side is an integer: compare via divmod instead of cross-multiply.
    // Cross-multiplying would otherwise overflow when the non-integer side
    // has a huge denominator (e.g. doubles like 19.99 stored as N/2^48).
    if (rhs_ad == 1)
    {
      umax q = lhs.Numerator / lhs_ad;
      umax r = lhs.Numerator % lhs_ad;
      auto cmp = (q == rhs.Numerator) ? (r == 0 ? std::strong_ordering::equal
                                                : std::strong_ordering::greater)
                                      : (q <=> rhs.Numerator);
      return lhs_neg ? (0 <=> cmp) : cmp;
    }
    if (lhs_ad == 1)
    {
      umax q = rhs.Numerator / rhs_ad;
      umax r = rhs.Numerator % rhs_ad;
      auto cmp = (q == lhs.Numerator) ? (r == 0 ? std::strong_ordering::equal
                                                : std::strong_ordering::less)
                                      : (lhs.Numerator <=> q);
      return lhs_neg ? (0 <=> cmp) : cmp;
    }

    // Cross-multiply in 128-bit: |numerator| and |denominator| are each ≤ 2^64−1,
    // so the products fit exactly in 128 bits — the comparison can never overflow,
    // so no trap is needed.
#if defined(__SIZEOF_INT128__)
    using u128n = unsigned __int128;
    u128n A = static_cast<u128n>(lhs.Numerator) * rhs_ad;
    u128n B = static_cast<u128n>(rhs.Numerator) * lhs_ad;
    return lhs_neg ? (B <=> A) : (A <=> B);
#else
    // Portable path (MSVC): form each product as {hi, lo} and compare lexically.
    const u128 A = umul(lhs.Numerator, rhs_ad);
    const u128 B = umul(rhs.Numerator, lhs_ad);
    return lhs_neg ? cmp128(B, A) : cmp128(A, B);
#endif
  }

  template <typename T>
  inline constexpr auto operator<=>(slim::optional<T> lhs, const rational& rhs)
  { return rational{lhs.value()} <=> rhs; }

  template <typename T>
  inline constexpr auto operator<=>(rational const& lhs, slim::optional<T> rhs)
  { return lhs <=> rational{rhs.value()}; }

  template <arithmetic T>
  inline constexpr auto operator<=>(T lhs, const rational& rhs)
  { return rational{lhs} <=> rhs; }

  template <arithmetic T>
  inline constexpr auto operator<=>(rational const& lhs, T rhs)
  { return lhs <=> rational{rhs}; }

  //---------------------------------------------------------------------------
  // Optional-lifting operators — one generic overload per arithmetic operator
  // that engages when an operand is a slim::optional, both unwrap to arithmetic,
  // and at least one to rational. Gating on `arithmetic` (not `boundable`, which
  // isn't visible this low) excludes bound operands, so bound-involving optional
  // expressions partition cleanly to arithmetic.hpp's generic instead.
  //---------------------------------------------------------------------------
  template <class L, class R>
  concept rational_lift_operands =
       (is_slim_optional_v<L> || is_slim_optional_v<R>)
    && arithmetic<unwrap_t<L>> && arithmetic<unwrap_t<R>>
    && (std::same_as<unwrap_t<L>, rational> || std::same_as<unwrap_t<R>, rational>);

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<rational> operator*(rational const& lhs, rational const& rhs)
  { return rational::mul_impl<true>(lhs, rhs); }

  // arithmetic operand — direct construction, no lift overhead
  template <arithmetic T>
  inline constexpr auto operator*(T lhs, rational const& rhs)
  { return rational{lhs} * rhs; }

  template <arithmetic T>
  inline constexpr auto operator*(rational const& lhs, T rhs)
  { return lhs * rational{rhs}; }

  // optional operand(s) — propagate via lift
  template <class L, class R> requires rational_lift_operands<L, R>
  inline constexpr auto operator*(L const& lhs, R const& rhs)
  { return lift([](auto const& a, auto const& b){ return a * b; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<rational> operator/(rational const& lhs, rational const& rhs)
  { return rational::div_impl<true>(lhs, rhs); }

  template <arithmetic T>
  inline constexpr auto operator/(T lhs, rational const& rhs)
  { return rational{lhs} / rhs; }

  template <arithmetic T>
  inline constexpr auto operator/(rational const& lhs, T rhs)
  { return lhs / rational{rhs}; }

  template <class L, class R> requires rational_lift_operands<L, R>
  inline constexpr auto operator/(L const& lhs, R const& rhs)
  { return lift([](auto const& a, auto const& b){ return a / b; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<rational> operator+(const rational& lhs, const rational& rhs)
  { return rational::add_impl<true>(lhs, rhs); }

  template <arithmetic T>
  inline constexpr auto operator+(T lhs, rational const& rhs)
  { return rational{lhs} + rhs; }

  template <arithmetic T>
  inline constexpr auto operator+(rational const& lhs, T rhs)
  { return lhs + rational{rhs}; }

  template <class L, class R> requires rational_lift_operands<L, R>
  inline constexpr auto operator+(L const& lhs, R const& rhs)
  { return lift([](auto const& a, auto const& b){ return a + b; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<rational> operator-(const rational& lhs, const rational& rhs)
  { return operator+(lhs, -rhs); }

  template <arithmetic T>
  inline constexpr auto operator-(T lhs, rational const& rhs)
  { return rational{lhs} - rhs; }

  template <arithmetic T>
  inline constexpr auto operator-(rational const& lhs, T rhs)
  { return lhs - rational{rhs}; }

  template <class L, class R> requires rational_lift_operands<L, R>
  inline constexpr auto operator-(L const& lhs, R const& rhs)
  { return lift([](auto const& a, auto const& b){ return a - b; }, lhs, rhs); }

  inline constexpr slim::optional<rational> operator-(slim::optional<rational> const& v)
  { return lift([](rational r){ return -r; }, v); }

  //---------------------------------------------------------------------------
  // Compound-assignment definitions — unwrap the checked binary op result.
  // .value() throws slim::bad_optional_access on overflow; callers that
  // need a non-throwing path must use the binary operators directly.
  //---------------------------------------------------------------------------
  inline constexpr rational& rational::operator+=(rational const& rhs)
  { *this = (*this + rhs).value(); return *this; }

  inline constexpr rational& rational::operator-=(rational const& rhs)
  { *this = (*this - rhs).value(); return *this; }

  inline constexpr rational& rational::operator*=(rational const& rhs)
  { *this = (*this * rhs).value(); return *this; }

  inline constexpr rational& rational::operator/=(rational const& rhs)
  { *this = (*this / rhs).value(); return *this; }

  // Forwarding overloads — accept arithmetic RHS (lifted via rational{}) and
  // slim::optional<rational> RHS (unwrapped via .value()) so callers can
  // chain `r += rational * rational` without a manual unwrap.
  template <arithmetic T>
  inline constexpr rational& operator+=(rational& lhs, T rhs)
  { return lhs += rational{rhs}; }

  template <arithmetic T>
  inline constexpr rational& operator-=(rational& lhs, T rhs)
  { return lhs -= rational{rhs}; }

  template <arithmetic T>
  inline constexpr rational& operator*=(rational& lhs, T rhs)
  { return lhs *= rational{rhs}; }

  template <arithmetic T>
  inline constexpr rational& operator/=(rational& lhs, T rhs)
  { return lhs /= rational{rhs}; }

  inline constexpr rational& operator+=(rational& lhs, slim::optional<rational> const& rhs)
  { return lhs += rhs.value(); }

  inline constexpr rational& operator-=(rational& lhs, slim::optional<rational> const& rhs)
  { return lhs -= rhs.value(); }

  inline constexpr rational& operator*=(rational& lhs, slim::optional<rational> const& rhs)
  { return lhs *= rhs.value(); }

  inline constexpr rational& operator/=(rational& lhs, slim::optional<rational> const& rhs)
  { return lhs /= rhs.value(); }

  //---------------------------------------------------------------------------
  // divides_evenly
  //---------------------------------------------------------------------------
  [[nodiscard]] inline constexpr bool divides_evenly(rational const& dividend, rational const& divisor)
  {
    if (divisor == 0) return true;            // convention: everything divides 0 evenly
    if (dividend.Numerator == 0) return true; // 0 / anything is the integer 0

    // dividend/divisor ∈ ℤ without forming the (possibly umax-overflowing)
    // quotient numerator. In lowest terms dividend = p/q, divisor = r/s; the
    // quotient p·s/(q·r) is integral iff q | s and r | p (rationals are
    // canonicalized, so gcd(p,q)=gcd(r,s)=1). All checks are on single fields.
    const umax p = dividend.Numerator, q = abs_den(dividend.Denominator);
    const umax r = divisor.Numerator,  s = abs_den(divisor.Denominator);
    return (s % q == 0) && (p % r == 0);
  }

} // namespace bnd::detail

namespace bnd
{
  // `rational` is internal, but the grid-building `notch<N,D>` / `frac<N,D>`
  // literals are public — they never expose the type.
  template <umax N, imax D = 1>
  inline constexpr detail::rational notch = detail::rational{N, D};

  // frac<N, D> — exact fractional grid value (signed numerator), companion to
  // notch<N,D> for non-dyadic endpoints not writable as a float literal
  // (e.g. `frac<-6, 5>` for -1.2).
  template <imax N, imax D = 1>
  inline constexpr detail::rational frac = detail::rational{N, D};
} // namespace bnd

namespace slim
{
  constexpr bnd::detail::rational sentinel_traits<bnd::detail::rational>::sentinel() noexcept { return bnd::detail::rational::make_sentinel(); }
  constexpr bool sentinel_traits<bnd::detail::rational>::is_sentinel(const bnd::detail::rational& v) noexcept
  { return v.is_sentinel(); }
} // namespace slim



// ======================================================================
//  bound/interval.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------



namespace bnd { struct interval; }

namespace slim
{
  template<>
  struct sentinel_traits<bnd::interval>
  {
    protected:
      static constexpr bnd::interval sentinel() noexcept;
      static constexpr bool is_sentinel(const bnd::interval& v) noexcept;
  };
} // namespace slim

namespace bnd
{
  //---------------------------------------------------------------------------
  // interval — structural NTTP type (public members only) with inclusive Lower
  // and Upper bounds. Like `grid`, its operator+/-/*// computes result intervals
  // at compile time; division returns nullopt when the divisor straddles zero
  // (grid::operator/ re-runs on the two zero-free halves and unions them).
  //---------------------------------------------------------------------------
  struct interval
  {
    detail::rational Lower;
    detail::rational Upper;

    interval() = default;

    constexpr interval(detail::rational lower, detail::rational upper)
     :Lower{lower}, Upper{upper} { }
    constexpr interval(arithmetic auto lower, arithmetic auto upper)
     :Lower{lower}, Upper{upper} { }

    template <auto I>
    static constexpr bool validate()
    {
      static_assert(I.Lower <= I.Upper);
      return true;
    }

    constexpr bool operator==(const interval& rhs) const = default;
    constexpr interval operator-() const { return interval{-Upper, -Lower}; }

    constexpr bool divides_evenly(const detail::rational& notch) const
    { return bnd::detail::divides_evenly((Upper - Lower).value(), notch); }

    constexpr slim::optional<detail::rational> operator/(const detail::rational& notch) const
    { return (Upper - Lower) / notch; }
  };

  // Containment / disjointness — free functions over the public endpoints
  // (siblings of the binary interval operators below).
  [[nodiscard]] constexpr bool includes(interval const& iv, interval const& rhs)
  { return iv.Lower <= rhs.Lower && rhs.Upper <= iv.Upper; }

  [[nodiscard]] constexpr bool includes(interval const& iv, detail::rational const& r)
  { return iv.Lower <= r && r <= iv.Upper; }

  [[nodiscard]] constexpr bool includes(interval const& iv, arithmetic auto a)
  { return includes(iv, detail::rational{a}); }

  // `excludes` means *strictly disjoint* — the intervals share no value.
  // `!includes()` is weaker: it only rules out total containment, so two
  // overlapping intervals are `!includes` AND `!excludes`.
  [[nodiscard]] constexpr bool excludes(interval const& iv, interval const& rhs)
  { return rhs.Upper < iv.Lower || iv.Upper < rhs.Lower; }

  // The `includes(rhs, iv)` clause catches rhs wholly containing iv (where
  // neither rhs endpoint lands in iv, so the other checks would miss it).
  [[nodiscard]] constexpr bool overlaps(interval const& iv, interval const& rhs)
  { return includes(rhs, iv) || includes(iv, rhs.Lower) || includes(iv, rhs.Upper); }

  // The min/max hull of four endpoint combinations — the result interval of an
  // interval product or quotient (interval arithmetic's four-corner rule).
  namespace detail
  {
    constexpr interval corner_hull(rational a, rational b, rational c, rational d) noexcept
    {
      auto [lo, hi] = std::minmax({a, b, c, d});
      return interval{lo, hi};
    }
  }

  constexpr slim::optional<interval> operator+  (const interval&, const interval&);
  constexpr slim::optional<interval> operator-  (const interval&, const interval&);
  constexpr slim::optional<interval> operator*  (const interval&, const interval&);
  constexpr slim::optional<interval> operator/  (const interval&, const interval&);
  constexpr auto                     operator<=>(const interval&, const interval&) -> std::partial_ordering;

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<interval> operator+(const interval& lhs, const interval& rhs)
  {
    return lift(
      [](detail::rational l, detail::rational u){ return interval{l, u}; },
      lhs.Lower + rhs.Lower, lhs.Upper + rhs.Upper);
  }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<interval> operator-(const interval& lhs, const interval& rhs)
  {
    return operator+(lhs, -rhs);
  }

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<interval> operator*(const interval& lhs, const interval& rhs)
  {
    return lift(detail::corner_hull,
      lhs.Lower * rhs.Lower, lhs.Lower * rhs.Upper,
      lhs.Upper * rhs.Lower, lhs.Upper * rhs.Upper);
  }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<interval> operator/(const interval& lhs, const interval& rhs)
  {
    if (includes(rhs, 0))
      return slim::nullopt;

    return lift(detail::corner_hull,
      lhs.Lower / rhs.Lower, lhs.Lower / rhs.Upper,
      lhs.Upper / rhs.Lower, lhs.Upper / rhs.Upper);
  }

  //---------------------------------------------------------------------------
  // operator<=>
  //---------------------------------------------------------------------------
  inline constexpr auto operator<=>(const interval& lhs, const interval& rhs) -> std::partial_ordering
  {
    if (lhs.Upper < rhs.Lower)
      return std::partial_ordering::less;

    if (lhs.Lower > rhs.Upper)
      return std::partial_ordering::greater;

    if (lhs.Lower == rhs.Lower && lhs.Upper == rhs.Upper)
      return std::partial_ordering::equivalent;

    return std::partial_ordering::unordered;
  }

} // namespace bnd

namespace slim
{
  constexpr bnd::interval sentinel_traits<bnd::interval>::sentinel() noexcept
  { return bnd::interval{bnd::detail::rational::make_sentinel(), bnd::detail::rational::make_sentinel()}; }

  constexpr bool sentinel_traits<bnd::interval>::is_sentinel(const bnd::interval& v) noexcept
  { return v.Lower.Denominator == 0; }
} // namespace slim

//---------------------------------------------------------------------------
// Structured bindings: `auto [lo, hi] = interval{...};`
//---------------------------------------------------------------------------
template <> struct std::tuple_size<bnd::interval> : std::integral_constant<std::size_t, 2> {};
template <std::size_t I> struct std::tuple_element<I, bnd::interval> { using type = bnd::detail::rational; };

namespace bnd
{
  template <std::size_t I, class Iv>
    requires std::same_as<std::remove_cvref_t<Iv>, bnd::interval>
  constexpr auto&& get(Iv&& iv) noexcept
  {
    if constexpr (I == 0) return std::forward<Iv>(iv).Lower;
    else                  return std::forward<Iv>(iv).Upper;
  }
}


// ======================================================================
//  bound/policy_flag.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


namespace bnd
{
  //---------------------------------------------------------------------------
  // policy_flag
  //---------------------------------------------------------------------------
  using policy_flag = unsigned long long;

  // Check model: compile-time checks always run. When success can't be proven
  // statically, compilation fails unless the matching ignore flag is set; else a
  // runtime check is inserted that throws (or reports via an error_code param).
  // Binary operations OR the flags of both operands.
  inline static constexpr policy_flag none         {0ull};
  inline static constexpr policy_flag ignore_zero  {1ull << 1};
  inline static constexpr policy_flag ignore_domain{1ull << 2};
  // `snap` — an off-notch value is rounded to fit the grid instead of
  // rejected; on its own truncate-toward-zero. Without it, an off-notch value is
  // a compile/runtime error and div/mod fall through to exact-rational results.
  inline static constexpr policy_flag snap     {1ull << 4};
  inline static constexpr policy_flag round_nearest {(1ull << 5) | snap};
  // Rounding modes each pick a unique bit and OR in `snap`. Conceptually
  // exclusive; combining two is allowed but dispatch (assignment.hpp) picks the
  // first match: nearest → floor → ceil → half_even → trunc.
  inline static constexpr policy_flag round_floor     {(1ull << 6) | snap};
  inline static constexpr policy_flag round_ceil      {(1ull << 7) | snap};
  inline static constexpr policy_flag round_half_even {(1ull << 8) | snap};

  // runtime checking — opt-in
  inline static constexpr policy_flag checked{1ull << 34}; // enable runtime domain/overflow checks

  // unary — mutually exclusive
  inline static constexpr policy_flag clamp   {1ull << 32}; // saturate to boundary
  inline static constexpr policy_flag wrap    {1ull << 33}; // modular arithmetic
  inline static constexpr policy_flag sentinel{1ull << 35}; // overflow -> sentinel (nullopt)

  // Representation flags — select raw storage. Without one, storage is deduced
  // from the grid (notch-0 → rational; unit notch at/below 0 → integer value;
  // else 0-based index). Binary ops OR operand policies; storage resolves
  // widest-wins: exact > f64 > direct > indexed > deduced.
  //
  // `f64` — math operand, binary64-backed storage under the default engine (value
  // held as IEEE-754 double, notch nominal); an ordinary round_nearest integer
  // bound under BND_MATH_FIXED. Power-of-2 notch + dyadic Lower required so
  // on-grid values are exact in double (see `double_exact`).
  inline static constexpr policy_flag f64{(1ull << 37) | round_nearest};

  // `f32` — binary32-backed storage (raw held as IEEE-754 float, notch nominal);
  // the single-precision sibling of `f64`, for float-only FPUs (Cortex-M4F) and
  // the `flt` engine. Power-of-2 notch + dyadic Lower required AND every on-grid
  // value must fit float's 24-bit significand (see `float_exact`). Like `f64` it
  // is an ordinary round_nearest integer bound under BND_MATH_FIXED. Widest-wins
  // storage order: exact > f64 > f32 > direct > indexed > deduced.
  inline static constexpr policy_flag f32{(1ull << 41) | round_nearest};

  // `real` — deprecated spelling of `f64`, kept as an alias for one release. New
  // code should use `f64` (binary64 storage) or `f32` (binary32). The flag is
  // purely a storage choice — transcendentals gate on `snap`, not on this.
  inline static constexpr policy_flag real = f64;

  // `exact` — force rational raw storage on any grid. Values still obey the grid;
  // exact fractions, no notch-count limit, no double. Slowest; overflow-checked
  // rational math. Identical under both engines.
  inline static constexpr policy_flag exact{1ull << 38};

  // `direct` — force raw == value (plain integer) where deduction would pick a
  // 0-based index (bound<{5,100}> stores 5..100). Wire/debugger value for interop.
  // Requires Notch == 1.
  inline static constexpr policy_flag direct{1ull << 39};

  // `indexed` — force raw == 0-based notch index where deduction would pick
  // direct storage (bound<{-5,5}> stores 0..10). Dense unsigned layout. Requires
  // Notch != 0.
  inline static constexpr policy_flag indexed{1ull << 40};

  // opt-out of `checked`: no domain/round/overflow/div-by-zero checks (reading
  // out-of-range or dividing by zero is UB; `/= 0` no-ops, `a / 0` skips the
  // check). Includes `snap` so notch-incompatible assigns compile.
  inline static constexpr policy_flag unsafe
    {(1ull << 36) | ignore_domain | snap | ignore_zero};

  //---------------------------------------------------------------------------
  // Flag-set membership predicates. `has_flag(set, flag)` is true iff EVERY bit
  // of `flag` is present in `set` — reads better than the raw `(set & flag) ==
  // flag` and is correct for composite flags (e.g. `round_nearest` carries
  // `snap`, `real` carries `round_nearest`), where a bare `set & flag`
  // truthy test would misfire. `has_any_flag` tests for any overlap.
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr bool has_flag(policy_flag set, policy_flag flag) noexcept
  { return (set & flag) == flag; }

  [[nodiscard]] constexpr bool has_any_flag(policy_flag set, policy_flag flags) noexcept
  { return (set & flags) != none; }

  //---------------------------------------------------------------------------
  // no_action — zero-overhead default for overflow callbacks
  //---------------------------------------------------------------------------
  struct no_action {};

  //---------------------------------------------------------------------------
  // tagged actions — opt-in callbacks for each failure path.
  // The lambda receives the bound by mutable reference as its first argument,
  // so the handler can override the value the policy was about to store.
  //---------------------------------------------------------------------------
  template<typename F> struct on_clamp_t    { [[no_unique_address]] F fn; };
  template<typename F> struct on_wrap_t     { [[no_unique_address]] F fn; };
  template<typename F> struct on_error_t    { [[no_unique_address]] F fn; };
  template<typename F> struct on_sentinel_t { [[no_unique_address]] F fn; };
  template<typename F> struct on_overflow_t { [[no_unique_address]] F fn; };

  //---------------------------------------------------------------------------
  // CTAD-style factories — drop the on_overflow_t{lambda} brace-init.
  //---------------------------------------------------------------------------
  template<typename F> [[nodiscard]] constexpr auto on_clamp(F&& fn)
  { return on_clamp_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }
  template<typename F> [[nodiscard]] constexpr auto on_wrap(F&& fn)
  { return on_wrap_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }
  template<typename F> [[nodiscard]] constexpr auto on_error(F&& fn)
  { return on_error_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }
  template<typename F> [[nodiscard]] constexpr auto on_sentinel(F&& fn)
  { return on_sentinel_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }
  template<typename F> [[nodiscard]] constexpr auto on_overflow(F&& fn)
  { return on_overflow_t<std::remove_cvref_t<F>>{std::forward<F>(fn)}; }

  namespace detail
  {
  // Action detection: the `*Pred` struct is the primary detector; the concept
  // derives from it and strips cvref so the ref form matches the value form.
  template<typename T> struct IsClampActionPred    : std::false_type {};
  template<typename F> struct IsClampActionPred<on_clamp_t<F>>    : std::true_type {};
  template<typename T> struct IsWrapActionPred     : std::false_type {};
  template<typename F> struct IsWrapActionPred<on_wrap_t<F>>     : std::true_type {};
  template<typename T> struct IsErrorActionPred    : std::false_type {};
  template<typename F> struct IsErrorActionPred<on_error_t<F>>    : std::true_type {};
  template<typename T> struct IsSentinelActionPred : std::false_type {};
  template<typename F> struct IsSentinelActionPred<on_sentinel_t<F>> : std::true_type {};
  template<typename T> struct IsOverflowActionPred : std::false_type {};
  template<typename F> struct IsOverflowActionPred<on_overflow_t<F>> : std::true_type {};

  template<typename T> concept clamp_action    = IsClampActionPred   <std::remove_cvref_t<T>>::value;
  template<typename T> concept wrap_action     = IsWrapActionPred    <std::remove_cvref_t<T>>::value;
  template<typename T> concept error_action    = IsErrorActionPred   <std::remove_cvref_t<T>>::value;
  template<typename T> concept sentinel_action = IsSentinelActionPred<std::remove_cvref_t<T>>::value;
  template<typename T> concept overflow_action = IsOverflowActionPred<std::remove_cvref_t<T>>::value;

  //---------------------------------------------------------------------------
  // implied_flags<A> — single source of truth for "this action requires these
  // policy bits". Used by bound::on_* and the action-first free-fn overloads.
  //---------------------------------------------------------------------------
  template<typename T> inline constexpr policy_flag implied_flags = none;
  template<typename F> inline constexpr policy_flag implied_flags<on_clamp_t<F>>    = clamp;
  template<typename F> inline constexpr policy_flag implied_flags<on_wrap_t<F>>     = wrap;
  template<typename F> inline constexpr policy_flag implied_flags<on_error_t<F>>    = checked;
  template<typename F> inline constexpr policy_flag implied_flags<on_sentinel_t<F>> = sentinel;
  template<typename F> inline constexpr policy_flag implied_flags<on_overflow_t<F>> = checked;

  //---------------------------------------------------------------------------
  // Pack helpers — let policy_ref/assignment/arithmetic accept Actions... packs.
  // The `*Pred` structs are reused as template-template parameters (concepts
  // can't be passed as such in C++23).
  //---------------------------------------------------------------------------

  // True if any element of the pack matches the trait.
  template<template<typename> class Trait, typename... As>
  inline constexpr bool has_action = (Trait<std::remove_cvref_t<As>>::value || ... || false);

  // How many pack elements match.
  template<template<typename> class Trait, typename... As>
  inline constexpr unsigned count_action_matches =
    (0u + ... + (Trait<std::remove_cvref_t<As>>::value ? 1u : 0u));

  // OR of implied_flags<plain<A>> across the pack.
  template<typename... As>
  inline constexpr policy_flag merged_implied_flags =
    (none | ... | implied_flags<std::remove_cvref_t<As>>);

  // pick_action<Trait>(actions...) returns a reference to the first pack element
  // matching the trait, or a static `no_action` fallback if none does. Conflict
  // diagnostics elsewhere ensure at most one match.
  namespace _detail
  {
    template<template<typename> class Trait>
    inline no_action& pick_action_fallback()
    { static no_action n; return n; }

    template<template<typename> class Trait, typename A, typename... Rest>
    constexpr auto& pick_action_impl(A& a, Rest&... rest)
    {
      if constexpr (Trait<std::remove_cvref_t<A>>::value) return a;
      else if constexpr (sizeof...(Rest) > 0) return pick_action_impl<Trait>(rest...);
      else return pick_action_fallback<Trait>();
    }
  }

  template<template<typename> class Trait, typename... As>
  constexpr auto& pick_action(As&... as)
  {
    if constexpr (sizeof...(As) == 0) return _detail::pick_action_fallback<Trait>();
    else return _detail::pick_action_impl<Trait>(as...);
  }

  // Same, but operating on a tuple (lvalue or rvalue ref).
  template<template<typename> class Trait, typename Tuple>
  constexpr auto& pick_action_in(Tuple& t)
  {
    return std::apply(
      [](auto&... as) -> auto& { return pick_action<Trait>(as...); },
      t);
  }

  } // namespace detail
} // namespace bnd




namespace bnd { struct grid; }

namespace slim
{
  template<>
  struct sentinel_traits<bnd::grid>
  {
    protected:
      static constexpr bnd::grid sentinel() noexcept;
      static constexpr bool is_sentinel(const bnd::grid& v) noexcept;
  };
} // namespace slim

namespace bnd
{
  //---------------------------------------------------------------------------
  // grid — structural NTTP type (public members only). Discretizes its interval
  // into notch-sized steps (interval must divide evenly by notch; Notch == 0
  // allows every rational, raw not offset). Its operator+/-/*// is the engine of
  // compile-time result-grid inference: every bound arithmetic operator computes
  // its result grid here, so the result interval contains every reachable value.
  //---------------------------------------------------------------------------
  struct grid
  {
    interval Interval;
    detail::rational Notch;

    grid() = default;
    // Corner ctors accept any type convertible to `rational` — int/float/rational and
    // any `bound` / `just<>` (via its implicit `operator rational()`), so a bound can be
    // a grid corner. They stay *templates* (deducing the corner type) on purpose: a
    // braced `{lo, hi}` can't deduce to a template parameter, so the `grid{{lo,hi}, notch}`
    // spelling unambiguously picks `grid(interval, rational)` below. The conversion is
    // resolved at the call site, so grid.hpp needs no dependency on `bound`.
    constexpr grid(std::convertible_to<detail::rational> auto lower,
                   std::convertible_to<detail::rational> auto upper,
                   std::convertible_to<detail::rational> auto notch)
      :grid{interval{lower, upper}, notch} { }
    constexpr grid(std::convertible_to<detail::rational> auto lower,
                   std::convertible_to<detail::rational> auto upper)
      :grid{interval{lower, upper}, detail::rational{1}} { }
    constexpr grid(std::convertible_to<detail::rational> auto lower)
      :grid{interval{lower, lower}, detail::rational{0}} { }
    constexpr grid(interval val, detail::rational notch):Interval{val}, Notch{notch} { }

    template <auto G>
    static constexpr bool validate()
    {
      interval::validate<G.Interval>();
      static_assert(G.Interval.divides_evenly(G.Notch));
      // Lower must sit on the notch lattice. divides_evenly avoids forming the
      // (possibly umax-overflowing) Lower/Notch quotient, so a grid finer than
      // uint64 index space is still valid (it stores as rational).
      static_assert(G.Notch == 0 || detail::divides_evenly(G.Interval.Lower, G.Notch));

      return true;
    }

    // Runtime sibling of validate<G>(): same invariants, but returns a typed
    // error instead of failing a static_assert — for grids built from runtime
    // config. A value, so it can't be a bound<G,P> template argument.
    [[nodiscard]] static constexpr slim::expected<grid, errc>
    try_make(interval iv, detail::rational notch)
    {
      if (iv.Lower > iv.Upper)
        return slim::unexpected{errc::domain_error};
      if (!iv.divides_evenly(notch))
        return slim::unexpected{errc::rounding_error};
      if (notch != 0 && !detail::divides_evenly(iv.Lower, notch))
        return slim::unexpected{errc::rounding_error};
      return grid{iv, notch};
    }

    // Notch-slot count = (Upper-Lower)/Notch, computed WITHOUT the rational
    // division (which throws at constant-eval on overflow). A valid grid is
    // notch-aligned, so with span = p/q and Notch = r/s the count is exactly
    // (p/r)·(s/q); mul_overflow flags when it exceeds umax. Returns false (and
    // count is meaningless) on overflow — such a grid stores as rational, never
    // an index, so the count is never used.
    constexpr bool notch_count(umax& out) const
    {
      if (Notch == 0) { out = 0; return true; }
      const detail::rational span = (Interval.Upper - Interval.Lower).value();
      const umax p = span.Numerator,  q = detail::abs_den(span.Denominator);
      const umax r = Notch.Numerator, s = detail::abs_den(Notch.Denominator);
      if (r == 0 || p % r != 0 || s % q != 0) { out = 0; return false; }
      return !mul_overflow(p / r, s / q, &out);
    }

    // Index-storage slot count (0 on overflow; the over-flow branch of storage_min
    // is discarded for such grids, which pick rational storage instead).
    constexpr umax max_notch() const { umax c = 0; (void)notch_count(c); return c; }

    // True when the slot count fits umax (index storage is possible). False ⇒ the
    // grid is still valid but stores its value as a rational, never an index.
    constexpr bool notch_count_representable() const { umax c = 0; return notch_count(c); }

    // True when `v` is an *exact* slot: in the interval AND on a notch (notch-0
    // grids store verbatim, so any in-range value qualifies). Used to admit a
    // single representable value (e.g. `0_b`) regardless of whole-range mapping.
    constexpr bool representable(detail::rational v) const
    {
      if (!includes(Interval, v)) return false;
      if (Notch == 0) return true;
      auto diff = v - Interval.Lower;            // optional<rational>
      if (!diff) return false;
      auto off = diff.value() / Notch;           // optional<rational>
      return off.has_value() && detail::abs_den(off->Denominator) == 1;
    }

    // operator== be default for structural type
    constexpr bool operator==(const grid& rhs) const = default;
    constexpr grid operator-() const { return {-Interval, Notch}; }

    // (Raw → double decoding lives in `detail::as_double` (generic.hpp): the
    // decode depends on the storage KIND, not the raw type's signedness — a
    // `direct`-policy bound has an unsigned raw that IS the value.)

    // Snap a double to the nearest grid point `Lower + k·Notch`. `real` storage
    // is only selected for dyadic grids, so the snap is lossless. A continuous
    // grid (Notch == 0) passes `v` through. The round stays constexpr/<cmath>-free.
    constexpr double snap_double(double v) const
    {
      if (Notch == detail::rational{0}) return v;
      const double lo = static_cast<double>(Interval.Lower);
      const double nd = static_cast<double>(Notch);
      const double q  = (v - lo) / nd;
      // Round q to the nearest integer, half away from zero (matching the
      // integer engine's round_nearest). Narrow to imax only when provably safe;
      // for |q| >= 2^52 the double is already integral, so snap is a no-op.
      // This avoids the `floor(q+0.5)` double-rounding flaw and the unguarded
      // double->imax cast (UB for huge q).
      double r;
      const double aq = q < 0 ? -q : q;
      if (aq >= 4503599627370496.0)            // 2^52
        r = q;
      else
      {
        const imax   t    = static_cast<imax>(q);   // trunc toward zero; |q| < 2^52 < imax
        const double frac = q - static_cast<double>(t);
        if      (frac >=  0.5) r = static_cast<double>(t + 1);
        else if (frac <= -0.5) r = static_cast<double>(t - 1);
        else                   r = static_cast<double>(t);
      }
      return lo + r * nd;
    }

    static constexpr grid make_sentinel() noexcept
    { return grid{interval{detail::rational{0}, detail::rational{0}}, detail::rational::make_sentinel()}; }
  };

  // Smallest raw type holding every reachable index in G. Order: notch-zero →
  // rational (no integer index space); index count too large for any integer →
  // rational (store the value's fraction directly, no index); signed-direct fits
  // Lower < 0 with notch 1; unsigned-offset (max_notch slots) otherwise.
  namespace detail
  {
  template <grid G>
  using storage_min =
    std::conditional_t<(G.Notch == 0), detail::rational,
    std::conditional_t<(!G.notch_count_representable()), detail::rational,
    std::conditional_t<(G.Interval.Lower < 0 && G.Notch == 1),
      smallest_int_for<trunc(G.Interval.Lower), trunc(G.Interval.Upper)>,
      smallest_uint_for<G.max_notch()>>>>;

  // Dyadic grid: power-of-2 notch denominator and Lower denominator, so every
  // on-grid value is exactly representable in IEEE-754 `double`. Precondition
  // for double-backed (`real`) storage.
  constexpr bool is_pow2(umax n) { return n != 0 && (n & (n - 1)) == 0; }

  template <grid G>
  inline constexpr bool dyadic_grid =
       G.Notch.Numerator != 0
    && is_pow2(bnd::detail::abs_den(G.Notch.Denominator))
    && is_pow2(bnd::detail::abs_den(G.Interval.Lower.Denominator));

  // log2 of a power-of-two magnitude (>= 1); 0 for 1. (grid.hpp can't include
  // cmath.hpp — that depends on us — so this mirrors detail::log2_pow2.)
  constexpr int log2_pow2_mag(umax d) noexcept { int n = 0; while (d > 1) { d >>= 1; ++n; } return n; }

  // |r · 2^f| as an integer. On a dyadic grid every endpoint's denominator is a
  // power of two dividing 2^f, so r·2^f is integral. Writes |N| and returns true
  // when it fits in umax; returns false on overflow (which already means ≥ 2^53).
  constexpr bool scaled_numerator(const rational& r, int f, umax& out) noexcept
  {
    if (r.Numerator == 0) { out = 0; return true; }
    const int sh = f - log2_pow2_mag(abs_den(r.Denominator));   // 0 <= sh <= f
    if (sh >= 64) return false;
    if (r.Numerator > (~umax{0} >> sh)) return false;           // Numerator << sh overflows
    out = r.Numerator << sh;
    return true;
  }

  // `double`-exactness of a dyadic grid: the IEEE-754 double path equals the
  // exact grid arithmetic iff, at the coarsest-magnitude end, the value's ULP is
  // no coarser than the notch. Writing v = N·2^(−f) with f = log2(den(Notch)),
  // that is |N| < 2^53 (53-bit significand) AND f ≤ 1022 (notch ≥ smallest
  // normal, so no on-grid value is subnormal). The 2^1024 overflow ceiling is
  // unreachable once |N| < 2^53. Necessary precondition for `real` storage.
  template <grid G>
  constexpr bool compute_double_exact() noexcept
  {
    if constexpr (!dyadic_grid<G>) return false;
    else
    {
      constexpr int f = log2_pow2_mag(abs_den(G.Notch.Denominator));
      if (f > 1022) return false;
      umax nlo = 0, nhi = 0;
      if (!scaled_numerator(G.Interval.Lower, f, nlo)) return false;
      if (!scaled_numerator(G.Interval.Upper, f, nhi)) return false;
      constexpr umax lim = umax{1} << 53;
      return nlo < lim && nhi < lim;
    }
  }

  template <grid G>
  inline constexpr bool double_exact = compute_double_exact<G>();

  // `float`-exactness: the binary32 analogue of double_exact. Every on-grid value
  // v = N·2^(−f) must fit float's 24-bit significand (|N| < 2^24) with f ≤ 126
  // (notch ≥ float's smallest normal, so no on-grid value is subnormal).
  // Necessary precondition for `f32` (binary32-backed) storage.
  template <grid G>
  constexpr bool compute_float_exact() noexcept
  {
    if constexpr (!dyadic_grid<G>) return false;
    else
    {
      constexpr int f = log2_pow2_mag(abs_den(G.Notch.Denominator));
      if (f > 126) return false;
      umax nlo = 0, nhi = 0;
      if (!scaled_numerator(G.Interval.Lower, f, nlo)) return false;
      if (!scaled_numerator(G.Interval.Upper, f, nhi)) return false;
      constexpr umax lim = umax{1} << 24;
      return nlo < lim && nhi < lim;
    }
  }

  template <grid G>
  inline constexpr bool float_exact = compute_float_exact<G>();

  // Demote an fp STORAGE flag a result grid can't represent — for DEDUCED policies
  // (cmath auto-outputs, which inherit the operand's storage flag), so a deduced
  // f32 output whose grid overflows binary32 silently widens instead of hard-
  // erroring. (A grid a user spells `f32` on directly still static_asserts in
  // storage_pick — that's deliberate misuse, not deduction.) f32 needs float_exact,
  // f64 needs double_exact (Notch == 0 continuous fits either). When the flag
  // doesn't fit: widen f32→f64 if double holds the grid, else drop the fp flag so
  // storage is deduced. The snap/round bits are preserved.
  // Storage for a bound<G, P>: representation flags pick the raw type, widest-wins
  // (exact > real > direct > indexed > deduced).
  //   exact   → rational raw on any grid.
  //   real    → double-backed under the default engine, on a dyadic or notch-0
  //             grid; elided under BND_MATH_FIXED (falls through to deduced).
  //   direct  → raw == value, plain integer (Notch == 1).
  //   indexed → raw == 0-based notch index (Notch != 0).
  //   none    → storage_min deduction.
  template <grid G, policy_flag P>
  constexpr auto storage_pick()
  {
    if constexpr ((P & bnd::exact) == bnd::exact)
      return detail::rational{};
#ifndef BND_MATH_FIXED
    else if constexpr (((P & bnd::real) == bnd::real)
                    && (double_exact<G> || G.Notch == 0))
      return double{};
    else if constexpr (((P & bnd::real) == bnd::real) && dyadic_grid<G>)
    {
      // `real`/`f64` explicitly requested on a dyadic grid double can't represent
      // exactly (max |value·2^f| ≥ 2^53, or notch below the smallest normal).
      // Arithmetic drops the flag before reaching here, so this is direct misuse.
      static_assert(double_exact<G>,
        "f64 storage: grid exceeds double's 53-bit significand — coarsen the "
        "notch/range or use `exact`");
      return double{};   // unreachable; fixes the deduced return type
    }
    else if constexpr (((P & bnd::f32) == bnd::f32)
                    && (float_exact<G> || G.Notch == 0))
      return float{};
    else if constexpr (((P & bnd::f32) == bnd::f32) && double_exact<G>)
      // `f32` requested on a grid too fine for float but representable in double:
      // WIDEN the storage to binary64. This makes a deduced f32 output (a cmath
      // result inheriting the operand's flag) whose grid overflows float store its
      // value in double rather than hard-erroring — the value stays exact. The f32
      // POLICY bit remains (harmless; storage is raw-driven via fp_raw).
      return double{};
    else if constexpr (((P & bnd::f32) == bnd::f32) && dyadic_grid<G>)
    {
      // Too fine for double too → genuinely unrepresentable as fp storage.
      static_assert(double_exact<G>,
        "f32 storage: grid exceeds double's 53-bit significand — coarsen the "
        "notch/range or use `exact`");
      return float{};    // unreachable; fixes the deduced return type
    }
#endif
    else if constexpr ((P & bnd::direct) == bnd::direct && G.Notch == 1)
      return std::conditional_t<(G.Interval.Lower < 0),
          smallest_int_for<trunc(G.Interval.Lower), trunc(G.Interval.Upper)>,
          smallest_uint_for<static_cast<umax>(trunc(G.Interval.Upper))>>{};
    else if constexpr ((P & bnd::indexed) == bnd::indexed && G.Notch != 0)
      return smallest_uint_for<G.max_notch()>{};
    else
      return storage_min<G>{};
  }

  template <grid G, policy_flag P>
  using storage_for = decltype(storage_pick<G, P>());
  }

  constexpr slim::optional<grid> operator+(const grid&, const grid&);
  constexpr slim::optional<grid> operator-(const grid&, const grid&);
  constexpr slim::optional<grid> operator*(const grid&, const grid&);
  constexpr slim::optional<grid> operator/(const grid&, const grid&);

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<grid> operator+(const grid& lhs, const grid& rhs)
  {
    // gcd returns optional — lift it so a notch-denominator overflow produces
    // nullopt rather than a silently wrapped result grid.
    return lift(
      [](interval i, detail::rational n){ return grid{i, n}; },
      lhs.Interval + rhs.Interval, detail::gcd(lhs.Notch, rhs.Notch));
  }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<grid> operator-(const grid& lhs, const grid& rhs)
  {
    return operator+(lhs, -rhs);
  }

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<grid> operator*(const grid& lhs, const grid& rhs)
  {
    return lift(
      [](interval i, detail::rational n){ return grid{i, n}; },
      lhs.Interval * rhs.Interval, lhs.Notch * rhs.Notch);
  }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  inline constexpr slim::optional<grid> operator/(const grid& lhs, const grid& rhs)
  {
    auto d = lhs.Interval / rhs.Interval;
    if (d.has_value())
      return grid{*d, detail::rational{0}};

    // Divisor interval includes zero — exclude zero for result interval.
    if (rhs.Interval.Lower == 0 && rhs.Interval.Upper == 0)
      return slim::nullopt;

    // `step` = smallest non-zero divisor magnitude; splits the divisor interval
    // into positive [step, Upper] and negative [Lower, -step] (skipping zero).
    // Both sides present → the result is their union.
    detail::rational step = (rhs.Notch != 0) ? detail::abs(rhs.Notch) : detail::rational{1};
    bool has_pos = 0 < rhs.Interval.Upper;
    bool has_neg = 0 > rhs.Interval.Lower;

    if (has_pos && has_neg)
    {
      return lift(
        [](interval pos, interval neg){
          return grid{interval{std::min(neg.Lower, pos.Lower),
                               std::max(neg.Upper, pos.Upper)}, detail::rational{0}};
        },
        lhs.Interval / interval{step, rhs.Interval.Upper},
        lhs.Interval / interval{rhs.Interval.Lower, -step});
    }
    else if (has_pos)
    {
      return lift([](interval i){ return grid{i, detail::rational{0}}; },
                  lhs.Interval / interval{step, rhs.Interval.Upper});
    }
    else
    {
      return lift([](interval i){ return grid{i, detail::rational{0}}; },
                  lhs.Interval / interval{rhs.Interval.Lower, -step});
    }
  }
} // namespace bnd

namespace slim
{
  constexpr bnd::grid sentinel_traits<bnd::grid>::sentinel() noexcept
  { return bnd::grid::make_sentinel(); }

  constexpr bool sentinel_traits<bnd::grid>::is_sentinel(const bnd::grid& v) noexcept
  { return v.Notch.Denominator == 0; }
} // namespace slim

//---------------------------------------------------------------------------
// Structured bindings: `auto [iv, notch] = some_grid;`
//---------------------------------------------------------------------------
template <> struct std::tuple_size<bnd::grid> : std::integral_constant<std::size_t, 2> {};
template <> struct std::tuple_element<0, bnd::grid> { using type = bnd::interval; };
template <> struct std::tuple_element<1, bnd::grid> { using type = bnd::detail::rational; };

namespace bnd
{
  template <std::size_t I, class G>
    requires std::same_as<std::remove_cvref_t<G>, bnd::grid>
  constexpr auto&& get(G&& g) noexcept
  {
    if constexpr (I == 0) return std::forward<G>(g).Interval;
    else                  return std::forward<G>(g).Notch;
  }
}


//---------------------------------------------------------------------------
// generic — type-level traits and predicates used everywhere else. Public
// grid/policy introspection (`Grid<B>`, `BoundPolicy<B>`, `Lower/Upper/Notch<B>`,
// `Interval<B>`) plus the `boundable`/`numeric`/`bound_assignable` concepts; the
// storage-shape predicates and raw/value converters are internal (`bnd::detail`).
//---------------------------------------------------------------------------
namespace bnd
{
  template <grid G = grid{{0, 0}, 0}, policy_flag P = checked> struct bound;

  template <class>                 inline constexpr bool is_bound_v = false;
  template <grid G, policy_flag P> inline constexpr bool is_bound_v<bound<G, P>> = true;

  template <typename B>
  concept boundable = is_bound_v<std::remove_cvref_t<B>>;

  //---------------------------------------------------------------------------
  // Public grid/policy introspection — extract a bound's template parameters.
  // These mirror std::numeric_limits: they report what the grid is, used
  // opaquely (the rational return type is never named by callers).
  //---------------------------------------------------------------------------
  template <boundable B>
  inline constexpr grid Grid = []<grid G, policy_flag P>(bound<G, P>){ return G; } (B{});

  template <boundable B>
  inline constexpr policy_flag BoundPolicy = []<grid G, policy_flag P>(bound<G, P>){ return P; } (B{});

  template <typename T>
  inline constexpr interval Interval = {0,0};

  template <boundable B>
  inline constexpr interval Interval<B> = []<grid G, policy_flag P>(bound<G, P>){ return G.Interval; } (B{});

  template <std::integral I>
  inline constexpr interval Interval<I> =
      {std::numeric_limits<I>::lowest(), std::numeric_limits<I>::max()};

  template <boundable B>
  inline constexpr bnd::detail::rational Lower = []<grid G, policy_flag P>(bound<G, P>){ return G.Interval.Lower; } (B{});

  template <boundable B>
  inline constexpr bnd::detail::rational Upper = []<grid G, policy_flag P>(bound<G, P>){ return G.Interval.Upper; } (B{});

  template <boundable B>
  inline constexpr bnd::detail::rational Notch = []<grid G, policy_flag P>(bound<G, P>){ return G.Notch; } (B{});

  template <typename N>
  concept numeric = boundable<N> or arithmetic<N>;

  //---------------------------------------------------------------------------
  // Internal plumbing — storage shape, raw/value conversion, dispatch.
  //---------------------------------------------------------------------------
  namespace detail
  {
    template<typename T>
    using plain = std::remove_cvref_t<T>;

    // Always-false but template-dependent: lets a `static_assert` inside a
    // template body fire only when that template is actually instantiated
    // (e.g. the guidance overloads that make `bound + 1` ill-formed).
    template<typename...>
    inline constexpr bool dependent_false = false;

    //-------------------------------------------------------------------------
    // Conversion-helper legend — the value/raw plumbing reused across the
    // engine. "value space" = the number a bound denotes; "raw space" = how it
    // is stored (see §2 "Storage encoding" in docs/internals.md). Use this to
    // tell the similarly-named helpers apart:
    //
    //   as_rational(x)         value → rational    exact view of a scalar or bound
    //   as_double(b)           raw   → double       kind-aware decode; lossy off dyadic grids
    //   to_value(b)            raw   → imax         the bound's integer value (decodes an index)
    //   from_value(b, v)       imax  → raw          store integer value v into b (inverse of to_value)
    //   raw_cast<B>(x)         x     → raw_t<B>     TYPE cast only — no value arithmetic
    //   raw_imax(b)            raw   → imax         widen the raw bits (NOT the value for index storage)
    //   raw_from_offset<B>(o)  index → raw_t<B>     adds RawLo for direct storage; identity for index
    //-------------------------------------------------------------------------

    // Uniform rational view of a scalar or bound (rational{v} / operator rational()).
    template <numeric N>
    [[nodiscard]] constexpr rational as_rational(N v)
    {
      if constexpr (arithmetic<N>) return rational{v};
      else                         return v;
    }

    // Canonical-zero test for a divisor. rational stores zero as {0, 1}, so
    // Numerator == 0 catches it regardless of representation; other types compare
    // against their own zero.
    template <typename T>
    [[nodiscard]] constexpr bool is_canonical_zero(T const& v)
    {
      if constexpr (std::same_as<T, rational>) return v.Numerator == 0;
      else                                      return v == T{0};
    }

    template <boundable B>
    using raw_t = typename B::raw_type;

    // How a bound's value lives in its raw storage — four disjoint encodings
    // (selected by policy flags or deduced; see grid.hpp storage_pick):
    //   rational_raw — raw IS the value, as a rational.
    //   f64_raw      — raw IS the value, as an IEEE-754 double (dyadic grids only).
    //   f32_raw      — raw IS the value, as an IEEE-754 float  (dyadic grids only).
    //   value_raw    — raw IS the value, as a plain integer.
    //   index_raw    — raw is a 0-based notch index; value = Lower + raw*Notch.
    template <boundable B>
    inline constexpr bool f64_raw = std::is_same_v<raw_t<B>, double>;

    template <boundable B>
    inline constexpr bool f32_raw = std::is_same_v<raw_t<B>, float>;

    // fp_raw — value held directly in a floating-point raw (f64 or f32). These
    // share every value-path branch: read/store/compare/arithmetic compute in
    // double, narrowing to the raw type on store (lossless on an fp-exact grid).
    template <boundable B>
    inline constexpr bool fp_raw = f64_raw<B> || f32_raw<B>;

    template <boundable B>
    inline constexpr bool rational_raw = std::is_same_v<raw_t<B>, rational>;

    template <boundable B>
    inline constexpr bool value_raw =
         !fp_raw<B> && !rational_raw<B>
      && ((BoundPolicy<B> & bnd::direct) == bnd::direct
          || ((BoundPolicy<B> & bnd::indexed) != bnd::indexed
              && Notch<B> == 1
              && (Lower<B> == 0 || std::signed_integral<raw_t<B>>)));

    template <boundable B>
    inline constexpr bool index_raw =
         !fp_raw<B> && !rational_raw<B> && !value_raw<B>;

    // Ungated double view of any bound, for the `real` arithmetic arms (the
    // public operator double() is gated on a rounding flag; this is always
    // available). Everything but index storage holds the value verbatim; an
    // index decodes through the grid.
    template <boundable B>
    [[nodiscard]] constexpr double as_double(B const& b) noexcept
    {
      if constexpr (!index_raw<B>)
        return static_cast<double>(b.raw());
      else
        return static_cast<double>((*(b.raw() * Notch<B>) + Lower<B>).value());
    }

    template <boundable B>
    using negative = bound<-Grid<B>, BoundPolicy<B>>;

    // True when R's interval cannot contain zero — so `a / b` can return a plain
    // `bound` instead of `optional<bound>` (see detail/division.hpp). A point
    // grid at 0 is *not* excluded.
    template <boundable R>
    inline constexpr bool DivisorExcludesZero = (Lower<R> > 0) || (Upper<R> < 0);

    // Storage-agnostic int truncation of interval endpoints — intent-revealing
    // `static_cast<imax>(Lower<B>)`. Used by from_value, RawLo, the fast paths.
    template <boundable B>
    inline constexpr imax LowerImax = trunc(Lower<B>);

    template <boundable B>
    inline constexpr imax UpperImax = trunc(Upper<B>);

    // Slot count via grid::max_notch (overflow-safe: 0 when it doesn't fit umax,
    // for grids that store as rational and never use the index).
    template <boundable B>
    inline constexpr umax NotchCount = Grid<B>.max_notch();

    //-------------------------------------------------------------------------
    // grid_value_bounds / rational_mul_is_safe / rational_add_is_safe
    //
    // Conservative compile-time bound on the (numerator, denominator) of any
    // canonical value on a grid, and derived "can the rational op of two grid
    // values overflow imax" predicates — letting checked exact arithmetic drop
    // the optional wrapper when the grids prove no overflow is reachable.
    //
    // For a notched grid every value v = lo + k·notch over the common denominator
    // dC = |lo.den|·|hi.den|·|notch.den| is linear in k, so the max scaled
    // numerator is at an endpoint. A continuous grid (Notch == 0, non-point) has
    // unbounded denominators — nothing provable, so the helpers return false.
    //-------------------------------------------------------------------------
    constexpr bool grid_value_bounds(grid g, umax& max_num, umax& max_den) noexcept
    {
      if (g.Notch.Numerator == 0 && !(g.Interval.Lower == g.Interval.Upper))
        return false;                          // continuous: dens unbounded

      umax d_lo = abs_den(g.Interval.Lower.Denominator);
      umax d_hi = abs_den(g.Interval.Upper.Denominator);
      umax d_no = (g.Notch.Numerator == 0) ? umax{1} : abs_den(g.Notch.Denominator);

      umax d_common;
      if (mul_overflow(d_lo, d_hi, &d_common)) return false;
      if (mul_overflow(d_common, d_no, &d_common)) return false;

      umax lo_scaled, hi_scaled;
      if (mul_overflow(g.Interval.Lower.Numerator, d_common / d_lo, &lo_scaled)) return false;
      if (mul_overflow(g.Interval.Upper.Numerator, d_common / d_hi, &hi_scaled)) return false;

      max_num = lo_scaled > hi_scaled ? lo_scaled : hi_scaled;
      max_den = d_common;
      return true;
    }

    constexpr bool rational_mul_is_safe(grid g_l, grid g_r) noexcept
    {
      umax n_l, d_l, n_r, d_r;
      if (!grid_value_bounds(g_l, n_l, d_l)) return false;
      if (!grid_value_bounds(g_r, n_r, d_r)) return false;

      umax num_prod, den_prod;
      if (mul_overflow(n_l, n_r, &num_prod)) return false;
      if (mul_overflow(d_l, d_r, &den_prod)) return false;
      if (den_prod > static_cast<umax>(std::numeric_limits<imax>::max())) return false;
      return true;
    }

    // add_impl's worst case over the conservative common denominator
    // D = d_l*d_r: scaled numerators A <= n_l*d_r and B <= n_r*d_l, sum
    // A + B. (The same-denominator and lcm-reduced paths only shrink these;
    // mixed signs subtract magnitudes.)
    constexpr bool rational_add_is_safe(grid g_l, grid g_r) noexcept
    {
      umax n_l, d_l, n_r, d_r;
      if (!grid_value_bounds(g_l, n_l, d_l)) return false;
      if (!grid_value_bounds(g_r, n_r, d_r)) return false;

      umax den, a, b, sum;
      if (mul_overflow(d_l, d_r, &den)) return false;
      if (den > static_cast<umax>(std::numeric_limits<imax>::max())) return false;
      if (mul_overflow(n_l, d_r, &a)) return false;
      if (mul_overflow(n_r, d_l, &b)) return false;
      if (add_overflow(a, b, &sum)) return false;
      return true;
    }

    // Notch is a non-zero integer (denominator 1) — the grid is notch-aligned,
    // so values map 1:1 to integers. Gates the implicit imax/size_t conversions.
    template <grid G>
    inline constexpr bool notch_is_unit_integer =
      abs_den(G.Notch.Denominator) == 1 && G.Notch.Numerator != 0;

    // ONLY type conversion, NO value representation conversion calculation
    template <boundable B>
    constexpr raw_t<B> raw_cast(auto value)
    {
      return static_cast<raw_t<B>>(value);
    }

    template <boundable B>
    constexpr raw_t<B> raw_cast(rational value)
    {
      if constexpr (rational_raw<B>)
        return value;
      else
        return value.to<raw_t<B>>().value_or(0);
    }

    // Widen raw storage to imax. Distinct from `to_value(b)` for notch-stored
    // grids where raw is an index rather than a value — naming separates the
    // two intents that today both spell `static_cast<imax>`.
    template <boundable B>
    constexpr imax raw_imax(B b) noexcept { return static_cast<imax>(b.raw()); }

    //-------------------------------------------------------------------------
    // Q-format integer fast path: for grids with integer Lower, unit-numerator
    // Notch, and raw fitting imax, value↔raw is pure integer arithmetic. Shared
    // by operator rational(), from_value, and assignment::store.
    //-------------------------------------------------------------------------
    template <boundable B>
    inline constexpr bool HasQFormatFastPath =
        abs_den(Lower<B>.Denominator) == 1
        && Notch<B>.Numerator == 1
        && !rational_raw<B>
        && (std::signed_integral<raw_t<B>>
            || NotchCount<B> <= static_cast<umax>(std::numeric_limits<imax>::max()));

    // value → raw, integer math only. Pre: HasQFormatFastPath<B>.
    template <boundable B>
    constexpr raw_t<B> q_format_encode(imax value) noexcept
    {
      constexpr imax nd = abs_den(Notch<B>.Denominator);
      return raw_cast<B>((value - LowerImax<B>) * nd);
    }

    // raw → rational, integer math only. Pre: HasQFormatFastPath<B>.
    template <boundable B>
    constexpr rational q_format_decode(B b) noexcept
    {
      constexpr imax nd = abs_den(Notch<B>.Denominator);
      return rational{raw_imax(b) + LowerImax<B> * nd, nd};
    }

    // Library-internal extraction helper. Always succeeds (returns `imax`
    // unconditionally) but does not check the value fits in any narrower
    // target. User code should prefer `b.to<T>()`, which carries a typed
    // overflow error.
    template <boundable B>
    constexpr imax to_value(B b)
    {
      if constexpr (!index_raw<B>)
        return raw_imax(b);
      else // index storage
        return trunc(as_rational(b));
    }

    template <boundable B>
    constexpr void from_value(B& b, imax val)
    {
      if constexpr (!index_raw<B>)
        b = B::from_raw(raw_cast<B>(val));
      else if constexpr (HasQFormatFastPath<B>)
        b = B::from_raw(q_format_encode<B>(val));
      else // index storage, generic rational path
      {
        auto offset = (rational{val} - Lower<B>) / Notch<B>;
        b = B::from_raw(raw_cast<B>(offset.value().Numerator));
      }
    }

    //-------------------------------------------------------------------------
    // RawLo / RawHi / raw_from_offset — map interval endpoints to raw space. For
    // notch-offset storage the raw is a 0-based index (RawLo == 0); for direct
    // storage the raw IS the value (RawLo == LowerImax<B>), so an offset needs
    // RawLo<L> added back before storing.
    //-------------------------------------------------------------------------
    template <boundable B>
    inline constexpr imax RawLo = !index_raw<B> ? LowerImax<B> : 0;

    template <boundable B>
    inline constexpr imax RawHi = !index_raw<B> ? UpperImax<B> : static_cast<imax>(NotchCount<B>);

    template <boundable L>
    constexpr raw_t<L> raw_from_offset(umax offset) noexcept
    {
      if constexpr (!index_raw<L>)
        return raw_cast<L>(static_cast<imax>(offset) + RawLo<L>);
      else
        return raw_cast<L>(offset);
    }

    template <boundable L>
    constexpr raw_t<L> raw_from_offset(imax offset) noexcept
    {
      if constexpr (!index_raw<L>)
        return raw_cast<L>(offset + RawLo<L>);
      else
        return raw_cast<L>(static_cast<umax>(offset));
    }

    //-------------------------------------------------------------------------
    // IsIntegerInterval vs IsIntegerAligned — easy to confuse, both needed.
    //   IsIntegerInterval<B>: Lower and Upper integer (Notch may be fractional,
    //     e.g. bound<{0,100}, 1/10>). Lets Lower/Upper be used as imax constants.
    //   IsIntegerAligned<B>: Notch and Lower integer ⇒ IsIntegerInterval (not the
    //     converse). Precondition for native integer raw arithmetic (Raw == value).
    //-------------------------------------------------------------------------
    template <boundable B>
    inline constexpr bool IsIntegerInterval =
        abs_den(Lower<B>.Denominator) == 1 && abs_den(Upper<B>.Denominator) == 1;

    template <boundable B>
    inline constexpr bool IsIntegerAligned =
        abs_den(Notch<B>.Denominator) == 1 && abs_den(Lower<B>.Denominator) == 1;

    // Q-format: the canonical fixed-point shape (Q8.8, Q16.16, ...). Notch has
    // unit numerator with integer denominator > 1, Lower is an integer at 0.
    // Value = Raw / Notch.Denominator. Used to gate the integer fast path for
    // fixed-point division, which would otherwise fall into the slow rational
    // route because Notch.Denominator > 1 disqualifies IsIntegerAligned.
    template <boundable B>
    inline constexpr bool IsQFormat =
           !rational_raw<B>
        && Notch<B>.Numerator == 1
        && abs_den(Notch<B>.Denominator) > 1
        && abs_den(Lower<B>.Denominator) == 1
        && Lower<B> == 0;

    // Policy test: checks both type-level and per-operation policy.
    // Composite flags (e.g. round_nearest = bit5 | snap) require all
    // their bits set — having a subset like just `snap` does NOT match.
    template <boundable B, typename P, policy_flag F>
    inline constexpr bool HasPolicy = has_flag(BoundPolicy<B>, F) || plain<P>::test(F);

    // Round the non-negative offset quotient num/den (den >= 1) to an integer
    // notch index per L's rounding policy.
    //
    // Tie/sign rules are in VALUE space, not offset space, so assigning a value
    // rounds it the same way dividing down to it does (detail::div_rounded is the
    // reference). The offset num/den is >= 0 (sign lost by subtracting Lower), so
    // we rebuild the signed value-index NUM = m·den + num (m = Lower/Notch), round
    // it like div_rounded, and return the offset J - m. m is integral on every
    // dyadic/integer-aligned/Q-format grid; otherwise fall back to offset rounding.
    template <boundable L, typename P>
    [[nodiscard]] constexpr umax round_quotient(umax num, umax den) noexcept
    {
      constexpr rational zl =
          (Notch<L> == rational{0})
            ? rational{0}
            : (Lower<L> / Notch<L>).value_or(rational{0});
      constexpr bool vidx = (zl.Denominator == 1 || zl.Denominator == -1);
      constexpr imax m = vidx
          ? (zl.Denominator < 0 ? -static_cast<imax>(zl.Numerator)
                                :  static_cast<imax>(zl.Numerator))
          : imax{0};

      if constexpr (!vidx)
      {
        // Exotic Lower/Notch (no integral value index) — historical offset rule.
        if constexpr (HasPolicy<L, P, round_nearest>)        return (num + den / 2) / den;
        else if constexpr (HasPolicy<L, P, round_floor>)     return num / den;
        else if constexpr (HasPolicy<L, P, round_ceil>)      return (num + den - 1) / den;
        else if constexpr (HasPolicy<L, P, round_half_even>)
        {
          umax q = num / den, r = num % den;
          if (r * 2 < den) return q;
          if (r * 2 > den) return q + 1;
          return (q & 1) ? q + 1 : q;
        }
        else                                                 return num / den;
      }
      else
      {
        // Round the signed value-index NUM/di exactly like detail::div_rounded.
        const imax di  = static_cast<imax>(den);
        const imax NUM = m * di + static_cast<imax>(num);
        const imax t   = NUM / di;                 // C++ truncation toward zero
        const imax rr  = NUM % di;                 // sign of NUM, |rr| < di
        imax J;
        if (rr == 0)
          J = t;
        else
        {
          const bool neg = NUM < 0;
          const umax ar  = (rr < 0) ? ~static_cast<umax>(rr) + 1u
                                    :  static_cast<umax>(rr);
          const umax ab  = static_cast<umax>(di);  // ab - ar safe: 0 < ar < ab
          if constexpr (HasPolicy<L, P, round_nearest>)        // half away from zero
            J = (ar >= ab - ar) ? (neg ? t - 1 : t + 1) : t;
          else if constexpr (HasPolicy<L, P, round_floor>)     // toward -inf
            J = neg ? t - 1 : t;
          else if constexpr (HasPolicy<L, P, round_ceil>)      // toward +inf
            J = neg ? t : t + 1;
          else if constexpr (HasPolicy<L, P, round_half_even>) // tie -> even value
          {
            if      (ar < ab - ar) J = t;
            else if (ar > ab - ar) J = neg ? t - 1 : t + 1;
            else                   J = (t & 1) == 0 ? t : (neg ? t - 1 : t + 1);
          }
          else                                                 // snap: toward zero
            J = t;
        }
        return static_cast<umax>(J - m);           // offset index k = J - m (>= 0)
      }
    }

    // Forward decl — defined in assignment.hpp
    template <typename L, typename R> struct assignment;

    // A single-point source (Lower == Upper) carries one value, so the only
    // question is whether it lands on L's grid — admitting e.g. `3_b` into
    // `{{0,9},3}` while rejecting `1_b` and out-of-range points.
    template <typename L, typename R>
    inline constexpr bool point_exactly_assignable =
      (Lower<R> == Upper<R>) && Grid<L>.representable(Lower<R>);

    template <boundable B>
    [[nodiscard]] constexpr raw_t<B> sentinel_raw()
    {
      if constexpr (std::is_same_v<raw_t<B>, rational>)
        return rational::make_sentinel();
      else if constexpr (std::signed_integral<raw_t<B>>)
        return std::numeric_limits<raw_t<B>>::min();
      else
        // Unsigned: max(). Real (double): DBL_MAX — a finite, normal, comparable
        // slot, unreachable as an on-grid value (grids stay < 2^53), so the real
        // raw never holds NaN/inf/subnormal, only this sentinel, ±0, or a normal.
        return std::numeric_limits<raw_t<B>>::max();
    }

    // "Is this raw the reserved sentinel slot?" — rational counts any zero
    // denominator; everything else (incl. real's finite DBL_MAX) is `==`.
    template <boundable B>
    [[nodiscard]] constexpr bool raw_is_sentinel(raw_t<B> const& r)
    {
      if constexpr (std::is_same_v<raw_t<B>, rational>)
        return r.Denominator == 0;
      else
        return r == sentinel_raw<B>();
    }

    // Tail of the policy cascade: sentinel sets sentinel raw, checked reports.
    // Returns true if a policy handled the failure (caller should return).
    // Cheap default — reports through the static category message (no string).
    template <boundable B, typename P>
    constexpr bool domain_fail(B& b, P&& policy)
    {
      if constexpr (HasPolicy<B, P, sentinel>)
      {
        b = B::from_raw(sentinel_raw<B>());
        return true;
      }
      else if (policy.domain_check())
      {
        policy.report(errc::domain_error);
        return true;
      }
      return false;
    }

    // The two non-trivial clauses of `bound_assignable`, named so the concept and
    // its `bound_assignable_why` diagnostic share one definition. Concepts (not
    // bools) so `||` short-circuits *instantiation* (e.g. assignment<L,R>::Factor
    // is never formed when R isn't boundable).
    template <typename L, typename R, policy_flag P = checked>
    concept assign_intervals_ok =
      (!boundable<R> && !std::integral<R>)
      // wrap/clamp bring any value into range, so a disjoint rhs interval is fine
      // for them (the integral-rhs path already allows it — int's interval is unbounded).
      || ((BoundPolicy<L> | P) & (wrap | clamp)) != 0
      || not excludes(Interval<L>, Interval<R>);

    template <typename L, typename R, policy_flag P>
    concept assign_notch_ok =
      !boundable<R> || abs_den(assignment<L, R>::Factor.Denominator) == 1
      || ((BoundPolicy<L> | P) & snap) != 0
      || point_exactly_assignable<L, R>;
  } // namespace detail

  // Compile-time prerequisites for L = R, gating three failure modes at the call
  // site: (1) R is numeric; (2) intervals overlap (typed-interval R only —
  // skipped for float/rational, which have no static interval); (3) integer
  // notch ratio or snap set (else R's notch doesn't divide L's; opt into
  // rounding). Named `bound_assignable` to avoid shadowing std::assignable_from.
  template <typename L, typename R, policy_flag P = checked>
  concept bound_assignable =
    numeric<R>
    && detail::assign_intervals_ok<L, R, P>
    && detail::assign_notch_ok<L, R, P>;

  // Diagnostic helper: instantiating `bound_assignable_why<L,R,P>` fires a named
  // static_assert per failed clause, so a developer can see which tripped. Backs
  // both the default-build diagnostic fallbacks in `bound` (core.hpp, gated by
  // `BND_STRICT_SFINAE`) and the public `why_assignable` probe below.
  template <typename L, typename R, policy_flag P = checked>
  struct bound_assignable_why
  {
    // Collapse each clause to a plain bool *before* the static_assert. Asserting on
    // a concept-id makes GCC dump the whole satisfaction tree ("constraints not
    // satisfied / no operand of the disjunction…") on top of the message; a bool
    // condition prints just the message. Each clause is self-guarding (the inner
    // disjunctions gate `assignment<L,R>::Factor` on `boundable<R>`), so evaluating
    // all three unconditionally is safe even when R is not numeric.
    static constexpr bool is_numeric   = numeric<R>;
    static constexpr bool intervals_ok = detail::assign_intervals_ok<L, R, P>;
    static constexpr bool notch_ok     = detail::assign_notch_ok<L, R, P>;
    static_assert(is_numeric,
      "bound_assignable: rhs is not numeric (must be a bound or arithmetic type)");
    static_assert(intervals_ok,
      "bound_assignable: rhs interval lies entirely outside lhs interval and the policy "
      "(not wrap/clamp) cannot bring it into range — assignment can never succeed");
    static_assert(notch_ok,
      "bound_assignable: incompatible notches — use `with_snap()` or `policy<snap>()` to allow rounding");
    static constexpr bool value = bound_assignable<L, R, P>;
  };

  // Public manual probe: `static_assert(bnd::why_assignable<DstBound, decltype(src)>);`
  // emits the named per-clause reasons in any build — including a strict
  // (`BND_STRICT_SFINAE`) build where the automatic in-`bound` fallbacks are absent.
  template <typename Dst, typename Src, policy_flag P = BoundPolicy<Dst>>
  inline constexpr bool why_assignable =
    bound_assignable_why<Dst, std::remove_cvref_t<Src>, P>::value;
} // namespace bnd


// ======================================================================
//  bound/policy.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


// ======================================================================
//  bound/detail/assignment.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


namespace bnd::detail
{
  //---------------------------------------------------------------------------
  // assignment — narrowing/coercion between bounded and arithmetic types. Three
  // specialisations dispatch on the source (integral / fractional / boundable),
  // each routing through `store` (in-range) and `handle_out_of_range` /
  // `apply_clamp` / `apply_wrap` (policy). The boundable path also exposes
  // `is_integer_mapping` / `map_raw` — a pure-integer formula in the hot path.
  //
  // NOTE: every member is defined *inline* in its specialization body (rather
  // than out-of-line). MSVC cannot reliably match out-of-line definitions of
  // member function templates to constrained partial specializations
  // (C2244/C2995/C3855); inlining sidesteps that and keeps the code portable.
  //---------------------------------------------------------------------------
  // needs_runtime_domain_check<L, P, A>: true iff any out-of-range handler would
  // fire (an action, a clamp/wrap/sentinel bit, or default-throw under checked).
  // When false (typically `unsafe`, no action) the runtime range branch in
  // `assign` is dead code and skipped, letting the autovectorizer kick in.
  //---------------------------------------------------------------------------
  template <boundable L, typename P, typename A>
  inline constexpr bool needs_runtime_domain_check =
         clamp_action   <plain<A>>
      || wrap_action    <plain<A>>
      || sentinel_action<plain<A>>
      || error_action   <plain<A>>
      || HasPolicy<L, P, clamp>
      || HasPolicy<L, P, wrap>
      || HasPolicy<L, P, sentinel>
      || (HasPolicy<L, P, checked> && !HasPolicy<L, P, ignore_domain>);

  // Shared out-of-range policy cascade. Order: clamp/wrap/sentinel/error
  // *actions*, then clamp/wrap *policy* bits, then `domain_fail`. The four
  // callers-supplied callables cover how clamp/wrap store, the sentinel-action
  // value, and the error-message rhs view. `Wrappable` is false on the fractional
  // path (no wrap *action* branch). Returns true when a handler resolved the write.
  template <bool Wrappable, boundable L, typename P, typename A,
            typename DoClamp, typename DoWrap, typename SentinelVal, typename MsgView>
  constexpr bool dispatch_out_of_range(L& lhs, P&& policy, A&& action,
                                       DoClamp do_clamp, DoWrap do_wrap,
                                       SentinelVal sentinel_val,
                                       [[maybe_unused]] MsgView msg_view)
  {
    using PA = plain<A>;
    if constexpr (clamp_action<PA>)
    { do_clamp(); return true; }
    else if constexpr (Wrappable && wrap_action<PA>)
    { do_wrap(); return true; }
    else if constexpr (sentinel_action<PA>)
    {
      lhs = L::from_raw(sentinel_raw<L>());
      action.fn(lhs, sentinel_val());
      return true;
    }
    else if constexpr (error_action<PA>)
    {
      action.fn(lhs, errc::domain_error, errc_message(errc::domain_error));
      return true;
    }
    else if constexpr (HasPolicy<L, P, clamp>)
    { do_clamp(); return true; }
    else if constexpr (HasPolicy<L, P, wrap>)
    { do_wrap(); return true; }
    else
      return domain_fail(lhs, policy);
  }

  //---------------------------------------------------------------------------
  // assignment
  //---------------------------------------------------------------------------
  template <typename L, typename R>
  struct assignment;

  //---------------------------------------------------------------------------
  // assign(boundable, integral)
  //---------------------------------------------------------------------------
  template <boundable L, std::integral R>
  struct assignment<L,R>
  {
    private:
      template<typename A>
      static constexpr void apply_clamp(L& lhs, R rhs, imax lower, imax upper, A&& action)
      {
        // Pre: rhs is out of [lower, upper] (only called from handle_out_of_range),
        // so the two-way pick is the full clamp.
        imax clamped = static_cast<imax>(rhs) < lower ? lower : upper;
        imax overshoot = static_cast<imax>(rhs) - clamped;
        from_value(lhs, clamped);
        if constexpr (clamp_action<plain<A>>)
          action.fn(lhs, overshoot);
      }

      template<typename A>
      static constexpr void apply_wrap(L& lhs, R rhs, imax lower, imax upper, A&& action)
      {
        // Overflow-safe modular wrap: `upper-lower+1` and `rhs-lower` can exceed imax,
        // so the reduction runs in umax (both fit umax for any valid grid; the result
        // lands back in [lower, upper] ⊂ imax).
        const umax urange = static_cast<umax>(upper) - static_cast<umax>(lower) + 1u;
        const imax ri = static_cast<imax>(rhs);
        if (urange == 0)                              // span == 2^64−1: wrap is identity
        {
          from_value(lhs, ri);
          if constexpr (wrap_action<plain<A>>) action.fn(lhs, imax{0});
          return;
        }
        umax w;
        imax excess;
        if (ri >= lower)
        {
          const umax dist = static_cast<umax>(ri) - static_cast<umax>(lower);  // true, ≥ 0
          w = dist % urange;
          excess = static_cast<imax>(dist / urange);
        }
        else
        {
          const umax dist = static_cast<umax>(lower) - static_cast<umax>(ri);  // true, > 0
          const umax m = dist % urange;
          w = (m == 0) ? 0u : (urange - m);
          excess = -static_cast<imax>((dist + urange - 1u) / urange);          // −ceil(dist/range)
        }
        from_value(lhs, static_cast<imax>(static_cast<umax>(lower) + w));
        if constexpr (wrap_action<plain<A>>)
          action.fn(lhs, excess);
      }

      template<typename P, typename A>
      static constexpr bool handle_out_of_range(L& lhs, R rhs, imax lower, imax upper,
                                                P&& policy, A&& action)
      {
        return dispatch_out_of_range<true>(lhs, policy, action,
          [&]{ apply_clamp(lhs, rhs, lower, upper, action); },
          [&]{ apply_wrap (lhs, rhs, lower, upper, action); },
          [&]{ return static_cast<imax>(rhs); },
          [&]{ return rhs; });
      }

      static constexpr void store(L& lhs, R rhs)
      {
        if constexpr (!index_raw<L>)
          lhs = L::from_raw(raw_cast<L>(rhs));
        else if constexpr (Lower<L> == Upper<L>)
          lhs = L::from_raw(0);   // notch_storage point grid: 0 is the only offset
        else if constexpr (HasQFormatFastPath<L>)
          lhs = L::from_raw(q_format_encode<L>(static_cast<imax>(rhs)));
        else // index storage, generic rational path
        {
          rational raw = ((rhs - Interval<L>.Lower)/Notch<L>).value();
          lhs = L::from_raw(raw_cast<L>(raw.Numerator / static_cast<umax>(raw.Denominator)));
        }
      }

    public:
      template<typename P, typename A = no_action>
      static constexpr L& assign(L& lhs, R const& rhs, P&& policy, A&& action = {})
      {
        static_assert(not excludes(Interval<L>, Interval<R>));

        // The out-of-range check runs unconditionally — clamp/wrap/sentinel
        // policies handle it via apply_*, which is constexpr-clean. Only the
        // unhandled-checked path winds up calling `policy.report`, which
        // contains its own `std::is_constant_evaluated()` guard.
        if constexpr (not includes(Interval<L>, Interval<R>))
        {
          if constexpr (IsIntegerInterval<L>)
          {
            // Skip the runtime range branch entirely when every handler would
            // be dead anyway — the dead branch otherwise inhibits autovec.
            if constexpr (needs_runtime_domain_check<L, plain<P>, plain<A>>)
            {
              constexpr imax lower = LowerImax<L>;
              constexpr imax upper = UpperImax<L>;
              if (static_cast<imax>(rhs) < lower || static_cast<imax>(rhs) > upper) [[unlikely]]
                if (handle_out_of_range(lhs, rhs, lower, upper, policy, action)) return lhs;
            }
          }
          else if (not includes(Interval<L>, rhs))
          {
            // Non-integer L bounds: route through the rational path so fractional
            // Lower/Upper drive clamp/sentinel/error correctly.
            return assignment<L, rational>::assign(lhs, rational{rhs}, policy, action);
          }
        }

        store(lhs, rhs);
        return lhs;
      }
  };

  //---------------------------------------------------------------------------
  // assign(boundable, floating_point | rational)
  //---------------------------------------------------------------------------
  template <boundable L, typename R>
    requires fractional<R>
  struct assignment<L,R>
  {
    private:
      template<typename P, typename A>
      static constexpr void apply_clamp(L& lhs, R rhs, P&&, A&& action)
      {
        R clamped = (rhs < Lower<L>) ? static_cast<R>(Lower<L>) : static_cast<R>(Upper<L>);
        R overshoot;
        if constexpr (std::same_as<R, rational>)
          overshoot = (rhs - clamped).value_or(rational{0});
        else
          overshoot = rhs - clamped;

        // The clamp target is an interval endpoint — a grid point — so the slot is 0
        // or NotchCount, no rounding. real takes the endpoint as a double, rational
        // the exact constant (a double round-trip would lose non-dyadic endpoints);
        // raw_from_offset<L> adds Lower back for direct-encoded storage.
        if constexpr (fp_raw<L>)
          lhs = L::from_raw((rhs < Lower<L>) ? static_cast<double>(Lower<L>)
                                             : static_cast<double>(Upper<L>));
        else if constexpr (rational_raw<L>)
          lhs = L::from_raw((rhs < Lower<L>) ? Lower<L> : Upper<L>);
        else
          lhs = L::from_raw(raw_from_offset<L>(
              (rhs < Lower<L>) ? umax{0} : NotchCount<L>));

        if constexpr (clamp_action<plain<A>>)
          action.fn(lhs, overshoot);
      }

    public:
      // Exposed (not private) so the boundable-rhs wrap path can reuse the
      // rational specialization's modular wrap on fractional/notch grids, and
      // so the wrap path can reuse store_checked after computing the wrapped
      // value.
      //
      // apply_wrap for real R — modular reduction into [Lower, Lower + range)
      // followed by store_checked so the rounding policy still applies if rhs
      // doesn't land on a notch after wrapping. range = Upper - Lower + Notch.
      template<typename P, typename A>
      static constexpr void apply_wrap(L& lhs, R rhs, P&& policy, A&& action)
      {
        rational rhs_r{rhs};
        rational lower_r = Lower<L>;
        rational range   = ((Upper<L> - lower_r).value() + Notch<L>).value();
        // q = floor((rhs - lower) / range), wrapped = rhs - q * range
        rational shifted = (rhs_r - lower_r).value();
        imax q = floor((shifted / range).value());
        rational wrapped = (rhs_r - (rational{q} * range).value()).value();

        // Re-enter the rational-rhs specialization for the actual store so the
        // notch / rounding policy logic is exercised once.
        assignment<L, rational>::store_checked(lhs, wrapped, policy, action);

        if constexpr (wrap_action<plain<A>>)
          action.fn(lhs, q);
      }

      template<typename P, typename A = no_action>
      static constexpr bool store_checked(L& lhs, R rhs, P&& policy, A&& action = {})
      {
        if constexpr (rational_raw<L> && Notch<L> == 0)
        { lhs = L::from_raw(rhs); return true; }   // continuous: store verbatim
        else if constexpr (fp_raw<L>)
        {
          // real target: raw IS the value — snap to the dyadic grid (range handling
          // already ran in the assign cascade; finite guard mirrors store_f64's).
          const double v = static_cast<double>(rhs);
          if (!(v - v == 0))
            detail::raise(errc::not_finite, "non-finite double");
          lhs = L::from_raw(Grid<L>.snap_double(v));
          return true;
        }
        else if constexpr (Lower<L> == Upper<L>)
        {
          // Singleton grid: offset encoding → Raw=0; rational/direct → Raw = Lower.
          if constexpr (rational_raw<L>)
            lhs = L::from_raw(Lower<L>);
          else if constexpr (!index_raw<L>)
            lhs = L::from_raw(raw_cast<L>(RawLo<L>));
          else
            lhs = L::from_raw(0);
          return true;
        }
        else
        {
          // Store the k-th notch slot: rational storage holds the snapped value;
          // raw_from_offset<L> covers offset- and direct-encoded integers.
          auto store_slot = [&](auto k)
          {
            if constexpr (rational_raw<L>)
              lhs = L::from_raw((Lower<L> + (rational{k} * Notch<L>).value()).value());
            else
              lhs = L::from_raw(raw_from_offset<L>(k));
          };

          constexpr bool has_round_flag =
               HasPolicy<L, P, round_nearest> || HasPolicy<L, P, round_floor>
            || HasPolicy<L, P, round_ceil>    || HasPolicy<L, P, round_half_even>
            || HasPolicy<L, P, snap>;

          // Q-format integer shortcut: with integer Lower and notch 1/K the offset is
          // (num − Lo·aden)·(K/g) / (aden/g), g = gcd(aden, K) — one gcd + integer ops
          // instead of two rational ops. round_quotient is invariant under reduction,
          // so the slot is bit-identical to the rational path. Oversized denominators
          // fall through (the kMaxDen guard keeps every product inside imax).
          if constexpr (HasQFormatFastPath<L> && !fp_raw<L> && Notch<L> != 0)
          {
            constexpr imax K  = abs_den(Notch<L>.Denominator);
            constexpr imax Lo = LowerImax<L>;
            constexpr umax kKM = []{
              // 2 · K · M with saturation (M bounds |value| and the offset span)
              umax k = static_cast<umax>(K);
              umax m = static_cast<umax>(
                  ceil(((bnd::detail::abs(Lower<L>) > bnd::detail::abs(Upper<L>)
                      ? bnd::detail::abs(Lower<L>) : bnd::detail::abs(Upper<L>))
                   ))) * 2 + 2;
              if (k > std::numeric_limits<umax>::max() / m)
                return std::numeric_limits<umax>::max();
              umax km = k * m;
              return (km > std::numeric_limits<umax>::max() / 2)
                       ? std::numeric_limits<umax>::max() : km * 2;
            }();
            constexpr umax kMaxDen =
                static_cast<umax>(std::numeric_limits<imax>::max()) / kKM;

            const rational rv{rhs};                       // exact (copy for rational R)
            const umax aden = abs_den(rv.Denominator);
            if (kMaxDen != 0 && aden <= kMaxDen)
            {
              const umax g    = std::gcd(aden, static_cast<umax>(K));
              const umax den2 = aden / g;
              const imax k2   = K / static_cast<imax>(g);
              const imax num  = (rv.Denominator < 0) ? -rv.Numerator : rv.Numerator;
              const umax onum =                          // ≥ 0: rhs ≥ Lower (in range)
                  static_cast<umax>((num - Lo * static_cast<imax>(aden)) * k2);
              if (den2 == 1)
              { store_slot(onum); return true; }
              if constexpr (has_round_flag)
              { store_slot(round_quotient<L, P>(onum, den2)); return true; }
              // strict policy, off-notch: fall through to the rational path for
              // the error message / action plumbing (cold).
            }
          }

          rational raw = ((rhs - Lower<L>)/Notch<L>).value();
          umax den = static_cast<umax>(raw.Denominator);
          if (den == 1)
          { store_slot(raw.Numerator); return true; }

          if constexpr (has_round_flag)
            store_slot(round_quotient<L, P>(raw.Numerator, den));
          else if (policy.round_check()) [[unlikely]]
          {
            if constexpr (error_action<plain<A>>)
            { action.fn(lhs, errc::rounding_error, errc_message(errc::rounding_error)); return false; }
            policy.report(errc::rounding_error);
            return false;
          }
          else
            store_slot(round_quotient<L, P>(raw.Numerator, den));
          return true;
        }
      }

      template<typename P, typename A = no_action>
      static constexpr L& assign(L& lhs, R const& rhs, P&& policy, A&& action = {})
      {
        if (not includes(Interval<L>, rhs)) [[unlikely]]
        {
          // Fractional path has no wrap *action* branch (Wrappable = false).
          if (dispatch_out_of_range<false>(lhs, policy, action,
                [&]{ apply_clamp(lhs, rhs, policy, action); },
                [&]{ apply_wrap (lhs, rhs, policy, action); },
                [&]{ return rhs; },
                [&]{ return rhs; }))
            return lhs;
        }

        store_checked(lhs, rhs, policy, action);
        return lhs;
      }
  };

  //---------------------------------------------------------------------------
  // assign(boundable, boundable)
  //---------------------------------------------------------------------------
  template <boundable L, boundable R>
  struct assignment<L,R>
  {
    private:
      // Offset/Factor map rhs.Raw → lhs.Raw via `lhs.Raw = Factor·rhs.Raw + Offset`.
      // Branches: L rational (pass value through), R rational (pre-divide by
      // Notch<L>), both integer (the hot path, collapses to integer math).
      static constexpr rational calcOffset()
      {
        if constexpr (rational_raw<L>)
          return Lower<R>;
        else if constexpr (Notch<L> == 0)
          // Continuous fp_raw L: no grid to land on, mapping unused (store
          // routes through snap_double). 0 avoids the /Notch<L> divide-by-zero.
          return rational{0};
        else if constexpr (rational_raw<R>)
          return -(Lower<L>/Notch<L>).value();
        else
          return ((Lower<R> - Lower<L>)/Notch<L>).value();
      }

      static constexpr rational calcFactor()
      {
        if constexpr (rational_raw<L>)
          return Notch<R>;
        else if constexpr (Notch<L> == 0)
          // Continuous fp_raw L (see calcOffset). A denominator-1 Factor also
          // makes assign_notch_ok vacuously true (any value representable).
          return rational{0};
        else if constexpr (rational_raw<R>)
          return (rational{1}/Notch<L>).value();
        else
          return (Notch<R>/Notch<L>).value();
      }

    public:
      static constexpr rational Offset = calcOffset();
      static constexpr rational Factor = calcFactor();

      // Raw-space integer-only mapping — requires integer raw storage on both
      // sides (not rational, not real).
      static constexpr bool is_integer_mapping =
          !rational_raw<L> && !rational_raw<R>
          && !fp_raw<L> && !fp_raw<R>
          && abs_den(Factor.Denominator) == 1 && abs_den(Offset.Denominator) == 1;

      // Map rhs.Raw into L's raw space (requires is_integer_mapping). The
      // Offset/Factor formula assumes offset encoding both sides; for direct
      // storage, subtract Lower<R> first (R-value → R-offset) and add Lower<L>
      // after (raw_from_offset<L>). All integer (is_integer_mapping guarantees it).
      static constexpr imax map_raw(auto rhs_raw)
      {
        imax r_offset = rhs_raw;
        if constexpr (!index_raw<R>)
          r_offset -= RawLo<R>;

        // Offset is an exact integer here, so trunc(Offset) is a constexpr constant.
        imax l_offset = static_cast<imax>(Factor.Numerator) * r_offset + trunc(Offset);

        if constexpr (!index_raw<L>)
          return l_offset + RawLo<L>;
        else
          return l_offset;
      }

    private:
      // Grid of the wrap "excess"/carry handed to an on_wrap action:
      // floor((value − Lower) / range) for value ∈ R's interval (range = span + notch).
      // Both operands are bounds, so — like the clamp overshoot — the carry has a
      // known range and is delivered as a bound, not a raw imax.
      static constexpr grid wrap_excess_grid()
      {
        constexpr rational range = ((Upper<L> - Lower<L>).value() + Notch<L>).value();
        return grid{ floor(((Lower<R> - Lower<L>).value() / range).value()),
                     floor(((Upper<R> - Lower<L>).value() / range).value()) };
      }

      template<typename A>
      static constexpr void apply_clamp(L& lhs, R const& rhs, A&& action)
      {
        // RawLo/RawHi are already the correct Raw (no raw_from_offset). Real storage
        // takes the endpoint as a double (RawLo/Hi truncate fractional dyadic endpoints).
        if constexpr (fp_raw<L>)
          lhs = L::from_raw((as_rational(rhs) < Lower<L>)
            ? static_cast<double>(Lower<L>) : static_cast<double>(Upper<L>));
        else
          lhs = L::from_raw((as_rational(rhs) < Lower<L>)
            ? raw_cast<L>(RawLo<L>) : raw_cast<L>(RawHi<L>));
        // Overshoot (rhs − clamped) as a bound, via the result-grid inference of normal
        // bound arithmetic: both operands are bounds, so the overshoot is too. It is always
        // in-grid and on-notch for Grid<R> − Grid<L>, so the construction is exact.
        if constexpr (clamp_action<plain<A>>)
        {
          constexpr grid OG = (Grid<R> - Grid<L>).value();
          bnd::bound<OG> overshoot{ (as_rational(rhs) - as_rational(lhs)).value() };
          action.fn(lhs, overshoot);
        }
      }

      template<typename P, typename A>
      static constexpr void apply_wrap(L& lhs, R const& rhs, P&& policy, A&& action)
      {
        // The integer modular wrap (range = Upper - Lower + 1, integer values) is
        // only correct on a unit-integer grid — notch 1 with integer bounds, so
        // consecutive integers are adjacent grid points. Any other grid (fractional
        // notch, non-integer bounds) routes through the rational modular wrap.
        if constexpr (IsIntegerInterval<L> && abs_den(Notch<L>.Denominator) == 1
                      && Notch<L>.Numerator == 1)
        {
          // Unit-integer fast path: modular wrap on the integer value.
          imax rhs_imax = trunc(as_rational(rhs));
          constexpr imax lower = LowerImax<L>;
          constexpr imax upper = UpperImax<L>;
          imax range = upper - lower + 1;
          imax shifted = rhs_imax - lower;
          imax wrapped = ((shifted % range) + range) % range;
          imax excess  = (shifted < 0) ? ((shifted - range + 1) / range) : (shifted / range);
          from_value(lhs, wrapped + lower);
          if constexpr (wrap_action<plain<A>>)
            action.fn(lhs, bnd::bound<wrap_excess_grid()>{excess});   // carry as a bound
        }
        else if constexpr (wrap_action<plain<A>>)
        {
          // Fractional destination with a wrap action: reuse the rational modular-wrap
          // path for the store/rounding, but wrap its imax carry `q` into a bound before
          // handing it to the user action.
          assignment<L, rational>::apply_wrap(lhs, as_rational(rhs), policy,
            bnd::on_wrap([&](auto& self, imax q){
              action.fn(self, bnd::bound<wrap_excess_grid()>{q});
            }));
        }
        else
        {
          // Fractional destination, no wrap action: delegate unchanged.
          assignment<L, rational>::apply_wrap(lhs, as_rational(rhs), policy, action);
        }
      }

      template<typename P, typename A>
      static constexpr bool try_clamp_or_fail(L& lhs, R const& rhs, P&& policy, A&& action)
      {
        return dispatch_out_of_range<true>(lhs, policy, action,
          [&]{ apply_clamp(lhs, rhs, action); },
          [&]{ apply_wrap (lhs, rhs, policy, action); },
          [&]{ return as_rational(rhs); },
          [&]{ return as_rational(rhs); });
      }

      template<typename P>
      static constexpr void store(L& lhs, R const& rhs, P&&)
      {
        if constexpr (fp_raw<L>)
          // real target: raw IS the value — decode the source and snap to the dyadic
          // grid (the offset machinery below mis-encodes a double raw).
          lhs = L::from_raw(Grid<L>.snap_double(as_double(rhs)));
        else if constexpr (is_integer_mapping)
        {
          // exact: Factor and Offset have integer denominators, no rounding ambiguity
          if constexpr (Offset == 0 && Factor == 1)
            lhs = L::from_raw(raw_cast<L>(rhs.raw()));
          else
            lhs = L::from_raw(raw_cast<L>(map_raw(rhs.raw())));
        }
        else
        {
          rational rat = *(Offset + *(Factor * rhs.raw()));
          umax ad = static_cast<umax>(abs_den(rat.Denominator));
          // Round the L-offset to a notch index in VALUE space via round_quotient
          // (same as the scalar path), honouring every rounding mode.
          umax q = round_quotient<L, P>(rat.Numerator, ad);
          // rat is the L-offset; raw_from_offset<L> adds Lower<L> back for direct storage.
          lhs = L::from_raw((rat.Denominator < 0)
            ? raw_from_offset<L>(-static_cast<imax>(q))
            : raw_from_offset<L>(q));
        }
      }

    public:
      template<typename P, typename A = no_action>
      static constexpr L& assign(L& lhs, R const& rhs, P&& policy, A&& action = {})
      {
        // wrap/clamp bring any value into range, so a disjoint rhs interval is fine
        // for them (matches the integral-rhs path); only strict policies reject it.
        static_assert(HasPolicy<L, P, wrap> || HasPolicy<L, P, clamp>
                      || not excludes(Interval<L>, Interval<R>),
          "rhs interval lies entirely outside lhs interval and the policy cannot bring it into range");
        static_assert(abs_den(Factor.Denominator) == 1 || HasPolicy<L, P, snap>
                      || point_exactly_assignable<L, R>,
          "incompatible notches: use with_snap() or policy<snap>() to allow rounding");

        if constexpr (not includes(Interval<L>, Interval<R>))
        {
          if constexpr (needs_runtime_domain_check<L, plain<P>, plain<A>>)
          {
            if constexpr (is_integer_mapping)
            {
              if (imax mapped = map_raw(rhs.raw()); mapped < RawLo<L> || mapped > RawHi<L>)
                if (try_clamp_or_fail(lhs, rhs, policy, action)) return lhs;
            }
            else if (not includes(Interval<L>, as_rational(rhs)))
              if (try_clamp_or_fail(lhs, rhs, policy, action)) return lhs;
          }
        }

        store(lhs, rhs, policy);
        return lhs;
      }
  };
} // namespace bnd::detail



//---------------------------------------------------------------------------
// policy — runtime policy carrier, plus `policy_ref` for per-operation dispatch.
//   policy<F, E>  — compile-time flags F plus an optional error_ref E holding a
//                   bnd::errc& (EBO so the no-error-code form is zero-sized).
//   policy_ref    — wraps a bound& with a policy<...> and a tuple of on_* actions;
//                   compound ops flow through it, routing each action to the right
//                   callback at the right pipeline stage.
//---------------------------------------------------------------------------
namespace bnd
{
  //---------------------------------------------------------------------------
  // policy — derives from E for EBO: throwing form (E == empty_ref) is zero-sized;
  // `policy(ec)` makes E == error_ref carrying a `bnd::errc&`. No virtuals,
  // resolved at compile time. (The error-code channel reports `errc` directly —
  // there is no <system_error> dependency.)
  //---------------------------------------------------------------------------
  namespace detail
  {
    struct empty_ref{ };
    struct error_ref
    {
      constexpr error_ref(errc& ec):Code{ec} {}
      errc& Code;
    };
  }

  template<policy_flag W = none, typename E = detail::empty_ref>
  struct policy: E
  {
    constexpr policy() = default;
    constexpr policy(errc& ec) requires std::same_as<E, detail::error_ref>
    :E(ec) { }

    static constexpr bool test(policy_flag w)
    { return has_flag(W, w); }

    static constexpr bool domain_check()
    {
      if (std::is_constant_evaluated()) return true;
      return test(checked) && not test(ignore_domain);
    }

    static constexpr bool round_check()
    {
      if (std::is_constant_evaluated()) return true;
      return test(checked) && not test(snap);
    }

    // Cheap default report: no message construction. error_ref mode records the
    // code (sticky: keeps the first error); throw mode funnels through the
    // installed handler via an outlined cold helper. The constant-evaluation
    // guard names a fixed-string diagnostic for a clearer compile-time message
    // than "non-constexpr function called".
    constexpr void report(errc code)
    {
      if (std::is_constant_evaluated())
        detail::constexpr_error<
          "bound: value out of range during constant evaluation "
          "(checked policy hit; choose clamp/wrap/sentinel or widen the interval)">();
      if constexpr (std::is_same_v<E, detail::error_ref>)
        E::Code = E::Code != errc{} ? E::Code : code;
      else
        detail::raise(code);
    }
  };

  policy(errc&) -> policy<none, detail::error_ref>;

  //---------------------------------------------------------------------------
  // IsPolicy — true for policy<F,E> specializations, false otherwise.
  // Used to gate free-fn overloads so they don't accidentally bind P = action tag.
  //---------------------------------------------------------------------------
  namespace detail
  {
    template<typename T>             inline constexpr bool IsPolicy = false;
    template<policy_flag F, typename E> inline constexpr bool IsPolicy<policy<F,E>> = true;

    // Concept form of IsPolicy — pulls cvref off so the constraint matches
    // forwarded `policy<F,E>` references in template parameters.
    template<typename T>
    concept policy_like = IsPolicy<std::remove_cvref_t<T>>;

    // policy_flags_of<T> — the flag-set a one-shot `policy<F,E>` carries (else
    // `none`). Lets the value+policy constructor and policy_ref's conversion fold
    // the per-call flags into their `bound_assignable` check, so a one-shot
    // clamp/round actually relaxes the constraint it enables.
    template<typename T>                inline constexpr policy_flag policy_flags_of = none;
    template<policy_flag F, typename E> inline constexpr policy_flag policy_flags_of<policy<F,E>> = F;

    // True for policy specializations that carry a bnd::errc& reference.
    // Free-fn arithmetic uses this to decide whether to call policy.report on
    // failure (which sets ec) vs. returning silent nullopt (no-arg form).
    template<typename T>             inline constexpr bool UsesErrorRef = false;
    template<policy_flag F>          inline constexpr bool UsesErrorRef<policy<F, error_ref>> = true;
  }

  //---------------------------------------------------------------------------
  // make_policy
  //---------------------------------------------------------------------------
  template<policy_flag F = none>
  [[nodiscard]] constexpr auto make_policy()
  { return policy<F,detail::empty_ref>{}; }

  template<policy_flag F = none>
  [[nodiscard]] constexpr auto make_policy(errc& ec)
  { return policy<F,detail::error_ref>{ec}; }

  //---------------------------------------------------------------------------
  // report_or_nullopt — uniform "rational arithmetic failed" handler shared by
  // addition/multiplication/division/modulo. Three compile-time behaviors:
  // overflow_action<A> → fire it on a default Result; UsesErrorRef<P> →
  // policy.report then nullopt; plain throw-policy → nullopt.
  //---------------------------------------------------------------------------
  namespace detail
  {
  template <boundable Result, typename A, typename P>
  constexpr auto report_or_nullopt(A&& action, P&& policy, errc code,
                                   [[maybe_unused]] const char* what)
    -> std::conditional_t<overflow_action<A>, Result, slim::optional<Result>>
  {
    if constexpr (overflow_action<A>)
    {
      Result res;
      action.fn(res, code);
      return res;
    }
    else
    {
      if constexpr (UsesErrorRef<std::remove_cvref_t<P>>)
        policy.report(code);
      return slim::nullopt;
    }
  }
  } // namespace detail

  //---------------------------------------------------------------------------
  // Named convenience policies — let user code skip `make_policy<F>()` entirely
  // for the per-call flag form on free arithmetic functions.
  //---------------------------------------------------------------------------
  inline constexpr auto truncated        = make_policy<snap>();
  inline constexpr auto round_to_nearest = make_policy<round_nearest>();
  inline constexpr auto clamped          = make_policy<clamp>();
  inline constexpr auto wrapped          = make_policy<wrap>();

  //---------------------------------------------------------------------------
  // policy_ref — variadic in actions (stores std::tuple<As...>). policy_ref
  // pre-picks the matching action per call path, so assignment/arithmetic keep
  // their single-A signatures. Payoff: the imax-probe and narrowing stages of a
  // compound op can each fire a different action (e.g. on_overflow + on_clamp).
  //---------------------------------------------------------------------------
  namespace detail
  {
  // Shared assignment dispatch: store `src` into `dst` under `policy` + the single
  // matching action from `actions` (at most one assignment-time tag is present).
  // Backs both policy_ref (dst = the wrapped bound) and policy_buffer (dst = a fresh
  // target), so the conversion/assignment logic lives in exactly one place.
  template <boundable Dst, numeric C, typename P, typename... As>
  constexpr Dst& dispatch_assign(Dst& dst, C const& src, P& policy, std::tuple<As...>& actions)
  {
    if constexpr (has_action<IsClampActionPred, As...>)
      return assignment<Dst, C>::assign(dst, src, policy, pick_action_in<IsClampActionPred>(actions));
    else if constexpr (has_action<IsWrapActionPred, As...>)
      return assignment<Dst, C>::assign(dst, src, policy, pick_action_in<IsWrapActionPred>(actions));
    else if constexpr (has_action<IsSentinelActionPred, As...>)
      return assignment<Dst, C>::assign(dst, src, policy, pick_action_in<IsSentinelActionPred>(actions));
    else if constexpr (has_action<IsErrorActionPred, As...>)
      return assignment<Dst, C>::assign(dst, src, policy, pick_action_in<IsErrorActionPred>(actions));
    else
      return assignment<Dst, C>::assign(dst, src, policy);
  }

  // policy_buffer — the rvalue-receiver sibling of policy_ref. `with_snap()` etc.
  // on a *temporary* return this instead: it OWNS the bound by value (the temporary
  // is moved in), so the snapped value can be returned/stored without dangling —
  // `auto square(small n){ return (n*n).with_snap(); }` is safe. Value read-out only
  // (no operator=: assigning into a throwaway is meaningless). Same constrained
  // conversion as policy_ref, so it stays SFINAE-friendly.
  template<boundable B, typename P, typename... As>
  struct policy_buffer
  {
    B Owned;
    P Policy;
    [[no_unique_address]] std::tuple<As...> Actions;

    template <boundable Target>
      requires bound_assignable<Target, B, BoundPolicy<Target> | policy_flags_of<P>>
    constexpr operator Target()
    {
      Target r;
      dispatch_assign(r, Owned, Policy, Actions);
      return r;
    }
  };

  template<boundable B, typename P, typename... As>
  struct policy_ref
  {
    private:
    // Conflict diagnostics: at most one assignment-time tag (clamp / wrap /
    // sentinel / error), at most one of each kind, no clamp+wrap.
    static constexpr unsigned _clamp_count    = count_action_matches<IsClampActionPred,    As...>;
    static constexpr unsigned _wrap_count     = count_action_matches<IsWrapActionPred,     As...>;
    static constexpr unsigned _sentinel_count = count_action_matches<IsSentinelActionPred, As...>;
    static constexpr unsigned _error_count    = count_action_matches<IsErrorActionPred,    As...>;
    static constexpr unsigned _overflow_count = count_action_matches<IsOverflowActionPred, As...>;

    static_assert(_clamp_count + _wrap_count + _sentinel_count + _error_count <= 1,
      "on_clamp / on_wrap / on_sentinel / on_error are mutually exclusive in a single policy_ref");
    static_assert(_clamp_count    <= 1, "duplicate on_clamp");
    static_assert(_wrap_count     <= 1, "duplicate on_wrap");
    static_assert(_sentinel_count <= 1, "duplicate on_sentinel");
    static_assert(_error_count    <= 1, "duplicate on_error");
    static_assert(_overflow_count <= 1, "duplicate on_overflow");

    public:
    B& Ref;
    P Policy;
    // `[[no_unique_address]]` is load-bearing: each captureless action lambda
    // is an empty type, and without this attribute the tuple would pad each
    // one out to a byte. With it, `policy_ref<B, P>` carrying no actions has
    // the same size as `policy_ref<B, P, no_action>`.
    [[no_unique_address]] std::tuple<As...> Actions;

    private:
    // Pre-pick the assignment-time action that matches the policy_ref's pack,
    // and forward to the single-action assignment::assign. At most one of the
    // four assignment-time tags is in the pack (enforced by static_assert), so
    // exactly one branch fires; the rest fall through to no-action.
    // Generic assignment: store `src` into `dst` under this ref's Policy + picked
    // action. `operator=` uses it with dst = Ref (the bound this ref wraps); the
    // conversion operator below uses it with a fresh target, so a one-shot snap can
    // be read out as a value (`(a * b).with_snap()`), not only assigned.
    template <boundable Dst, numeric C>
    constexpr Dst& assign_into(Dst& dst, C const& src)
    { return dispatch_assign(dst, src, Policy, Actions); }

    template <numeric C>
    constexpr B& assign_with_picked(C const& other)
    { return assign_into(Ref, other); }

    public:
    template <numeric C>
    constexpr B& operator=(C const& other)
    { return assign_with_picked(other); }

    // Value read-out: a one-shot policy ref converts to any bound the assignment
    // could satisfy, applying the target's own policy (range) plus this ref's
    // carried flags (notch/rounding) via HasPolicy's merge. Makes
    // `Target t = (a * b).with_snap();` / `return (a * b).with_snap();` compile.
    // Constrained so the proxy stays SFINAE-friendly (no over-broad convertibility).
    template <boundable Target>
      requires bound_assignable<Target, B, BoundPolicy<Target> | policy_flags_of<P>>
    constexpr operator Target()
    {
      Target r;
      assign_into(r, Ref);
      return r;
    }

    // optional<C> sink — unwrap once at the proxy boundary so callers can chain
    // checked arithmetic into `.with_clamp() = ...` without per-step `.value()`.
    template <numeric C>
    constexpr B& operator=(slim::optional<C> const& other)
    { return assign_with_picked(other.value()); }

    private:
    constexpr void report_zero(errc code, const char* what)
    {
      if constexpr (has_action<IsErrorActionPred, As...>)
        pick_action_in<IsErrorActionPred>(Actions).fn(Ref, code, what);
      else if constexpr (!HasPolicy<B, P, ignore_zero>)
        Policy.report(code);
    }

    public:
    //-------------------------------------------------------------------------
    // boundable RHS overloads — route through the bound's arithmetic, then
    // assign via assign_with_picked so callbacks fire on the narrowing back to B.
    // An optional<bound> result (rational-raw overflow) surfaces errc::overflow
    // through on_overflow if registered, else report.
    //-------------------------------------------------------------------------
    private:
    template <typename R>
    constexpr B& finalise_arith(R&& result, [[maybe_unused]] const char* msg)
    {
      if constexpr (requires { typename plain<R>::value_type; })
      {
        if (!result.has_value()) [[unlikely]]
        {
          if constexpr (has_action<IsOverflowActionPred, As...>)
            pick_action_in<IsOverflowActionPred>(Actions).fn(Ref, errc::overflow);
          else
            Policy.report(errc::overflow);
          return Ref;
        }
        return assign_with_picked(result.value());
      }
      else
        return assign_with_picked(std::forward<R>(result));
    }

    // Shared body for the fractional `+=`/`-=`/`*=`/`/=` operators: the rational
    // RHS lifts Ref to rational and routes the checked result through
    // `finalise_arith`; any other fractional RHS lifts both sides to double.
    template <fractional C, typename RatOp, typename DblOp>
    constexpr B& fractional_assign(C const& rhs, RatOp rat_op, DblOp dbl_op,
                                   const char* msg)
    {
      if constexpr (std::same_as<C, rational>)
        return finalise_arith(rat_op(rational{Ref}, rhs), msg);
      else
        return assign_with_picked(dbl_op(static_cast<double>(Ref),
                                         static_cast<double>(rhs)));
    }
    public:

    template <boundable C>
    constexpr B& operator+=(C const& rhs)
    { return finalise_arith(Ref + rhs, "policy_ref::operator+= overflow"); }

    template <boundable C>
    constexpr B& operator-=(C const& rhs)
    { return finalise_arith(Ref - rhs, "policy_ref::operator-= overflow"); }

    template <boundable C>
    constexpr B& operator*=(C const& rhs)
    { return finalise_arith(Ref * rhs, "policy_ref::operator*= overflow"); }

    template <boundable C>
    constexpr B& operator/=(C const& rhs)
    { return finalise_arith(Ref / rhs, "policy_ref::operator/= division/overflow"); }

    template <boundable C>
    constexpr B& operator%=(C const& rhs)
    { return finalise_arith(mod(Ref, rhs, Policy), "policy_ref::operator%= division/overflow"); }

    //-------------------------------------------------------------------------
    // rational RHS overloads — the only non-bound operand a compound assign
    // accepts. Lets callers write `b += rational{1,3}`. Raw int/float/double are
    // ill-formed: give the scalar a grid (`1_b` / `just<1>` / `bound<{lo,hi}>{n}`).
    // Lift Ref to rational, checked op, finalise_arith.
    //-------------------------------------------------------------------------
    template <std::same_as<rational> C>
    constexpr B& operator+=(C const& rhs)
    {
      return fractional_assign(rhs, [](rational a, rational b){ return a + b; },
        [](double a, double b){ return a + b; }, "policy_ref::operator+= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator-=(C const& rhs)
    {
      return fractional_assign(rhs, [](rational a, rational b){ return a - b; },
        [](double a, double b){ return a - b; }, "policy_ref::operator-= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator*=(C const& rhs)
    {
      return fractional_assign(rhs, [](rational a, rational b){ return a * b; },
        [](double a, double b){ return a * b; }, "policy_ref::operator*= overflow");
    }

    template <std::same_as<rational> C>
    constexpr B& operator/=(C const& rhs)
    {
      if (is_canonical_zero(rhs))
      {
        report_zero(errc::division_by_zero, "policy_ref::operator/= division by zero");
        return Ref;
      }
      return fractional_assign(rhs, [](rational a, rational b){ return a / b; },
        [](double a, double b){ return a / b; }, "policy_ref::operator/= division/overflow");
    }
  };
  } // namespace detail

} // namespace bnd


// ======================================================================
//  bound/detail/addition.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// addition — `add(L, R, policy, action) -> bound<G>`, G = Grid<L> + Grid<R>.
// The grid arithmetic is sound by construction (the result interval contains
// every runtime sum), so overflow can only happen on rational-raw results.
// Specialises on the storage shapes: rational result, mixed rational/integer,
// direct integer-space add, or both notch-offset (scale via lhs/rhs_widen).
//---------------------------------------------------------------------------
namespace bnd::detail
{
  template <boundable L, boundable R = L>
  struct addition
  {
    static_assert((Grid<L> + Grid<R>).has_value(),
      "addition: result grid's notch/interval exceeds the representable rational "
      "range — coarsen the operand grids");
    static constexpr grid result_grid = (Grid<L> + Grid<R>).value();
    // Propagate fp storage only when the result grid stays exactly representable
    // in the chosen width; otherwise demote (f32→f64) or drop it so storage_pick
    // deduces an exact representation (the fp sum would diverge from the exact sum
    // — see grid::double_exact / float_exact). Widest-wins: prefer f32 only when
    // both operands are f32-only and the result fits float; an f64 operand or a
    // too-fine-for-float result widens to f64; too fine for double → exact.
    static constexpr bool any_f64 =
        (BoundPolicy<L> & bnd::real) == bnd::real || (BoundPolicy<R> & bnd::real) == bnd::real;
    static constexpr bool any_f32 =
        (BoundPolicy<L> & bnd::f32) == bnd::f32 || (BoundPolicy<R> & bnd::f32) == bnd::f32;
    static constexpr bool keep_f32 = any_f32 && !any_f64 && float_exact<result_grid>;
    static constexpr bool keep_f64 = !keep_f32 && (any_f64 || any_f32) && double_exact<result_grid>;
    // Carry both operands' representation flags (widest-wins at storage selection).
    static constexpr policy_flag rep =
        ((BoundPolicy<L> | BoundPolicy<R>) & (bnd::exact | bnd::direct | bnd::indexed))
        | (keep_f64 ? bnd::real : none) | (keep_f32 ? bnd::f32 : none);
    using result = bound<result_grid, rep != none ? rep : checked>;

    template <policy_flag F>
    static constexpr bool needs_overflow_check =
        rational_raw<result>
        && ((F | BoundPolicy<L> | BoundPolicy<R>) & checked)
        && !rational_add_is_safe(Grid<L>, Grid<R>);

    template <policy_flag F = none>
    using return_type_for = std::conditional_t<needs_overflow_check<F>,
                                               slim::optional<result>,
                                               result>;

    template <policy_flag F, typename A>
    using add_return_t = std::conditional_t<overflow_action<plain<A>>,
                                            result,
                                            return_type_for<F>>;

    // Result notch is gcd(NL, NR); scale each raw up to it before adding —
    // lhs_widen = NL/Nresult, rhs_widen = NR/Nresult (exact, Nresult divides both).
    // Guard the continuous-grid case (Notch<result> == 0): the rational divide-by-zero
    // path returns nullopt on GCC/Clang but MSVC's constexpr evaluator rejects it
    // (C2131). widen is unused on the continuous/rational result path, so 1 is fine.
    static constexpr imax lhs_widen = (Notch<result> == 0) ? imax{1}
        : (Notch<L> / Notch<result>).value_or(rational{1}).Numerator;
    static constexpr imax rhs_widen = (Notch<result> == 0) ? imax{1}
        : (Notch<R> / Notch<result>).value_or(rational{1}).Numerator;

    // Defined inline (not out-of-line): MSVC mishandles out-of-line member
    // templates of constrained partial specializations.
    template <policy_flag F = none, typename E = empty_ref, typename A = no_action>
    static constexpr auto add(L lhs, R rhs, policy<F, E> policy = {}, A&& action = {}) -> add_return_t<F, A>
  {
    result res;
    if constexpr (fp_raw<result>)
    {
      res = result::from_raw(raw_cast<result>(Grid<result>.snap_double(as_double(lhs) + as_double(rhs))));
    }
    else if constexpr (rational_raw<result>)
    {
      if constexpr (needs_overflow_check<F>)
      {
        auto sum = rational::add(lhs,rhs);
        if (!sum) [[unlikely]]
          return report_or_nullopt<result>(action, policy, errc::overflow,
                                           "rational overflow in add");
        res = result::from_raw(*sum);
      }
      else
        res = result::from_raw(rational::add_unchecked(lhs, rhs));
    }
    else if constexpr (rational_raw<L> || rational_raw<R>
                       || !((IsIntegerAligned<L> && IsIntegerAligned<R>)
                            || (index_raw<L> && index_raw<R>)))
    {
      // Rational store: a rational-raw operand, or a direct integer bound mixed
      // with a fractional notch-offset one (where neither to_value nor offset-widen
      // is exact — to_value would truncate the fractional operand). Compute the
      // exact rational sum and convert to result's raw via raw_from_offset.
      auto sum = rational::add_unchecked(lhs,rhs);
      res = result::from_raw(raw_from_offset<result>(
          ((sum - Lower<result>) / Notch<result>).value().Numerator));
    }
    else if constexpr (IsIntegerAligned<L> && IsIntegerAligned<R>)
    {
      // Both operands are integer-valued (Notch and Lower integers), so the
      // value-space add is exact.
      from_value(res, to_value(lhs) + to_value(rhs));
    }
    else
    {
      // Both notch-offset: scale each raw to the result notch and add in offset
      // space (offsets compose because result Lower = Lower<L> + Lower<R>).
      res = result::from_raw(raw_cast<result>(raw_imax(lhs) * lhs_widen + raw_imax(rhs) * rhs_widen));
    }
    return res;
  }
  };
} // namespace bnd::detail


// ======================================================================
//  bound/detail/multiplication.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// multiplication — `mul(L, R, policy, action) -> bound<Grid<L> * Grid<R>>`. The
// integer hot path branches on which corner of the four-quadrant product hits
// `Lower<result>`, doing the arithmetic as `umax * umax` (no signed overflow)
// plus integer offset corrections. Rational-result and all-integer-aligned
// cases come first.
//---------------------------------------------------------------------------
namespace bnd::detail
{
  template <boundable L, boundable R = L>
  struct multiplication
  {
    static_assert((Grid<L> * Grid<R>).has_value(),
      "multiplication: result grid's notch/interval exceeds the representable "
      "rational range — coarsen the operand grids");
    static constexpr grid result_grid = (Grid<L> * Grid<R>).value();
    // fp storage propagation (see addition.hpp): the product grid (notch = N_L·N_R)
    // is finer, so demote f32→f64 / drop f64 when it outgrows the width — the fp
    // product would round below the result notch. Widest-wins: f32 only if both
    // operands f32-only and product fits float; else widen to f64; else exact.
    static constexpr bool any_f64 =
        (BoundPolicy<L> & bnd::real) == bnd::real || (BoundPolicy<R> & bnd::real) == bnd::real;
    static constexpr bool any_f32 =
        (BoundPolicy<L> & bnd::f32) == bnd::f32 || (BoundPolicy<R> & bnd::f32) == bnd::f32;
    static constexpr bool keep_f32 = any_f32 && !any_f64 && float_exact<result_grid>;
    static constexpr bool keep_f64 = !keep_f32 && (any_f64 || any_f32) && double_exact<result_grid>;
    static constexpr bool dropped_fp = (any_f64 || any_f32) && !keep_f64 && !keep_f32;
    // Carry both operands' representation flags (widest-wins at storage selection).
    static constexpr policy_flag rep =
        ((BoundPolicy<L> | BoundPolicy<R>) & (bnd::exact | bnd::direct | bnd::indexed))
        | (keep_f64 ? bnd::real : none) | (keep_f32 ? bnd::f32 : none);
    using result = bound<result_grid, rep != none ? rep : checked>;

    // The dropped-fp case lands on a rational result when the product grid outgrows
    // uint index space; its product numerator can exceed `umax`, so check it (the
    // result carries `checked`) rather than wrap.
    template <typename P>
    static constexpr bool needs_overflow_check =
        rational_raw<result>
        && (((BoundPolicy<L> | BoundPolicy<R>) & checked) || plain<P>::test(checked)
            || dropped_fp)
        && !rational_mul_is_safe(Grid<L>, Grid<R>);

    template <typename P>
    using return_type_for = std::conditional_t<needs_overflow_check<P>,
                                               slim::optional<result>,
                                               result>;

    template <typename P, typename A>
    using mul_return_t = std::conditional_t<overflow_action<plain<A>>,
                                            result,
                                            return_type_for<P>>;

    // Defined inline (not out-of-line): MSVC mishandles out-of-line member
    // templates of constrained partial specializations.
    template <typename P, typename A = no_action>
    static constexpr auto mul(L lhs, R rhs, P&& policy, A&& action = {}) -> mul_return_t<P, A>
  {
    if constexpr (fp_raw<result>)
    {
      return result::from_raw(raw_cast<result>(Grid<result>.snap_double(as_double(lhs) * as_double(rhs))));
    }
    else if constexpr (rational_raw<result>)
    {
      if constexpr (needs_overflow_check<P>)
      {
        auto prod = as_rational(lhs) * as_rational(rhs);
        if (!prod) [[unlikely]]
          return report_or_nullopt<result>(action, policy, errc::overflow,
                                           "rational overflow in mul");
        return result::from_raw(raw_cast<result>(*prod));
      }
      else
        return result::from_raw(raw_cast<result>(rational::mul_unchecked(
            as_rational(lhs), as_rational(rhs))));
    }
    else if constexpr (IsIntegerAligned<L> && IsIntegerAligned<R> && IsIntegerAligned<result>)
    {
      result res;
      from_value(res, to_value(lhs) * to_value(rhs));
      return res;
    }
    else if constexpr (fp_raw<L> || fp_raw<R> || rational_raw<L> || rational_raw<R>)
    {
      // An operand whose raw is a double/rational can't feed the integer
      // four-quadrant formula below (it reads the raw as an integer offset).
      // Combine exactly as rationals and convert to the result's storage —
      // mirrors addition's rational-mixed branch. Reached when `real` was
      // dropped from the result (grid not double-exact) but operands stay real.
      auto prod = rational::mul_unchecked(as_rational(lhs), as_rational(rhs));
      return result::from_raw(raw_from_offset<result>(
          ((prod - Lower<result>) / Notch<result>).value().Numerator));
    }
    else
    {
      // Result writes go through raw_from_offset so direct-storage results
      // get Lower<result> added back to recover the value.
      auto to_result = [](auto raw_offset)
      { return result::from_raw(raw_from_offset<result>(static_cast<umax>(raw_offset))); };

      // Normalize lhs.raw() / rhs.raw() to *offsets* regardless of L's / R's
      // storage shape. The formulas below all assume offset arithmetic.
      umax lhs_offset = !index_raw<L>
          ? static_cast<umax>(raw_imax(lhs) - RawLo<L>)
          : static_cast<umax>(lhs.raw());
      umax rhs_offset = !index_raw<R>
          ? static_cast<umax>(raw_imax(rhs) - RawLo<R>)
          : static_cast<umax>(rhs.raw());

      // Absolute notch index of each operand endpoint (Lower/Notch, Upper/Notch).
      constexpr umax idxLoL = (Lower<L>/Notch<L>).value_or(rational{0}).Numerator;
      constexpr umax idxLoR = (Lower<R>/Notch<R>).value_or(rational{0}).Numerator;
      constexpr umax idxHiL = (Upper<L>/Notch<L>).value_or(rational{0}).Numerator;

      // Integral promotion would make `raw * raw` an `int * int` (UB above
      // INT_MAX), so cast to umax to multiply in 64-bit unsigned space. The four
      // branches cover the sign quadrants: Lower<result> is one of the four
      // corner products; sign-flipped helpers (negative<L>/<R>) reduce each to
      // the all-positive formula. The static_assert guards the case analysis.
      if constexpr (Lower<result> == (Lower<L> * Lower<R>).value())
      {
        return to_result(lhs_offset * rhs_offset
                         + lhs_offset * idxLoR
                         + rhs_offset * idxLoL);
      }

      if constexpr (Lower<result> == (Upper<L> * Upper<R>).value())
      { return multiplication<negative<L>, negative<R>>::mul(-lhs, -rhs, std::forward<P>(policy)); }

      if constexpr (Lower<result> == (Upper<L> * Lower<R>).value())
      {
        umax negLhs = NotchCount<L> - lhs_offset;
        return to_result(negLhs * idxLoR
                         + rhs_offset * idxHiL
                         - negLhs * rhs_offset);
      }

      if constexpr (Lower<result> == (Lower<L> * Upper<R>).value())
      { return -multiplication<L, negative<R>>::mul(lhs, -rhs, std::forward<P>(policy)); }

      static_assert(Lower<result> == (Lower<L> * Lower<R>).value()
                 || Lower<result> == (Upper<L> * Upper<R>).value()
                 || Lower<result> == (Upper<L> * Lower<R>).value()
                 || Lower<result> == (Lower<L> * Upper<R>).value(),
                 "multiplication: internal logic error");
    }
  }
  };
} // namespace bnd::detail


// ======================================================================
//  bound/detail/division.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// division / modulo. `division::div` returns optional<result> (division by zero
// is always runtime-possible). Two paths: native (integer-aligned grids +
// snap → native integer division) and rational (exact, can overflow under
// checked). `modulo::mod` is integer-only — non-integer remainders aren't
// well-defined on fractional notches.
//---------------------------------------------------------------------------
namespace bnd::detail
{
  // Both operands are plain integer grids and the caller accepted integer
  // truncation (snap) — the prerequisite for native integer div / mod.
  template <boundable L, boundable R, policy_flag F>
  inline constexpr bool integer_native_ops =
      ((F | BoundPolicy<L> | BoundPolicy<R>) & snap)
      && !rational_raw<L> && !rational_raw<R>
      && IsIntegerAligned<L> && IsIntegerAligned<R>;

  //---------------------------------------------------------------------------
  // Rounding mode for the native div & mod paths (fire when `snap` is set).
  // Decided from the combined flags with assignment.hpp's precedence (nearest →
  // floor → ceil → half_even → trunc); `snap` alone is truncate-toward-zero.
  // The runtime quotient and the compile-time grid endpoints MUST agree on the
  // mode (both read div_round_mode), or a result could escape its own grid.
  //---------------------------------------------------------------------------
  enum class round_mode { trunc, nearest, floor, ceil, half_even };

  constexpr round_mode div_round_mode(policy_flag eff) noexcept
  {
    if ((eff & round_nearest)   == round_nearest)   return round_mode::nearest;
    if ((eff & round_floor)     == round_floor)     return round_mode::floor;
    if ((eff & round_ceil)      == round_ceil)      return round_mode::ceil;
    if (has_flag(eff, round_half_even)) return round_mode::half_even;
    return round_mode::trunc;
  }

  // |v| as umax, safe for imax_min (negating it would be UB).
  constexpr umax uabs(imax v) noexcept
  { return v < 0 ? ~static_cast<umax>(v) + 1u : static_cast<umax>(v); }

  // Round the signed exact quotient a/b (b != 0) to an integer per `m`.
  constexpr imax div_rounded(imax a, imax b, round_mode m) noexcept
  {
    const imax t = a / b;                     // C++ truncation toward zero
    const imax r = a % b;                     // sign of a, |r| < |b|
    if (r == 0 || m == round_mode::trunc) return t;
    const bool neg = (a < 0) != (b < 0);      // exact quotient is negative
    const umax ar = uabs(r), ab = uabs(b);    // ab - ar is safe: 0 < ar < ab
    switch (m)
    {
      case round_mode::floor:   return neg ? t - 1 : t;
      case round_mode::ceil:    return neg ? t : t + 1;
      case round_mode::nearest:                       // half away from zero
        return (ar >= ab - ar) ? (neg ? t - 1 : t + 1) : t;
      case round_mode::half_even:
        if (ar < ab - ar) return t;
        if (ar > ab - ar) return neg ? t - 1 : t + 1;
        return (t & 1) == 0 ? t : (neg ? t - 1 : t + 1);   // tie → even
      default:                  return t;
    }
  }

  // Round a non-negative quotient num/den (den != 0) per `m`. Used by the
  // Q-format path, whose raws are non-negative (Lower == 0).
  constexpr umax round_uquotient(umax num, umax den, round_mode m) noexcept
  {
    const umax t = num / den, r = num % den;
    if (r == 0 || m == round_mode::trunc) return t;
    switch (m)
    {
      case round_mode::floor:   return t;             // non-negative: floor == trunc
      case round_mode::ceil:    return t + 1;
      case round_mode::nearest: return (r >= den - r) ? t + 1 : t;
      case round_mode::half_even:
        if (r < den - r) return t;
        if (r > den - r) return t + 1;
        return (t & 1) == 0 ? t : t + 1;
      default:                  return t;
    }
  }

  // Compile-time rounding of a quotient-interval endpoint to an integer index.
  // lo/hi differ only for half_even, where the endpoint is bracketed by
  // [floor, ceil] rather than reproducing the parity rule at compile time.
  constexpr imax round_rat_lo(rational q, round_mode m) noexcept
  {
    switch (m)
    {
      case round_mode::nearest:   return round(q);
      case round_mode::floor:     return floor(q);
      case round_mode::ceil:      return ceil(q);
      case round_mode::half_even: return floor(q);
      default:                    return trunc(q);
    }
  }
  constexpr imax round_rat_hi(rational q, round_mode m) noexcept
  {
    switch (m)
    {
      case round_mode::nearest:   return round(q);
      case round_mode::floor:     return floor(q);
      case round_mode::ceil:      return ceil(q);
      case round_mode::half_even: return ceil(q);
      default:                    return trunc(q);
    }
  }

  template <boundable L, boundable R = L, policy_flag F = none>
  struct division
  {
    // Native integer division, two flavours gated on `snap`:
    //   native_div_integer — both operands integer-aligned; formula `a / b`.
    //   native_div_qformat — both same Q-format (Notch = 1/N, Lower = 0); formula
    //                        `(a·N)/b` (the native `(a << log2 N)/b` idiom).
    // Otherwise the exact-rational path returns bound<rational>.
    static constexpr bool native_div_integer = integer_native_ops<L, R, F>;

    static constexpr bool native_div_qformat =
        ((F | BoundPolicy<L> | BoundPolicy<R>) & snap)
        && IsQFormat<L> && IsQFormat<R>
        && Notch<L> == Notch<R>;

    static constexpr bool native_div = native_div_integer || native_div_qformat;

    // The rounding mode for the native paths (shared by the grid and runtime).
    static constexpr round_mode rmode =
        div_round_mode(F | BoundPolicy<L> | BoundPolicy<R>);

    // A clear diagnostic when the result grid is unrepresentable, instead of the
    // raw optional-deref / .value() below failing cryptically (mirrors add/mul).
    static_assert(native_div_qformat || (Grid<L> / Grid<R>).has_value(),
      "division: result grid not representable (notch/interval exceeds the "
      "representable rational range) — coarsen the operand grids");
    static_assert(!native_div_qformat || (Upper<L> / Notch<R>).has_value(),
      "division: Q-format result grid not representable — coarsen the operand grids");

    // Native-integer endpoints rounded with the same mode as the runtime
    // quotient, so e.g. round_ceil can't escape the grid. (The Q-format extreme
    // is always exact, so its grid is unchanged.)
    static constexpr grid result_grid =
        native_div_integer
            ? grid{round_rat_lo((*(Grid<L> / Grid<R>)).Interval.Lower, rmode),
                   round_rat_hi((*(Grid<L> / Grid<R>)).Interval.Upper, rmode)}
      : native_div_qformat
            ? grid{interval{rational{0}, (Upper<L> / Notch<R>).value()}, Notch<L>}
            : *(Grid<L> / Grid<R>);

    static constexpr bool any_f64 =
        (BoundPolicy<L> & bnd::real) == bnd::real || (BoundPolicy<R> & bnd::real) == bnd::real;
    static constexpr bool any_f32 =
        (BoundPolicy<L> & bnd::f32) == bnd::f32 || (BoundPolicy<R> & bnd::f32) == bnd::f32;
    // Keep fp for a continuous result (Notch 0: the raw stores the quotient
    // verbatim) or an fp-exact dyadic result; otherwise demote f32→f64 / drop f64
    // (the fp quotient would not land on the result grid). Widest-wins as in mul.
    static constexpr bool keep_f32 =
        any_f32 && !any_f64 && (result_grid.Notch == 0 || float_exact<result_grid>);
    static constexpr bool keep_f64 =
        !keep_f32 && (any_f64 || any_f32) && (result_grid.Notch == 0 || double_exact<result_grid>);
    // Carry both operands' representation flags (widest-wins at storage selection).
    static constexpr policy_flag rep =
        ((BoundPolicy<L> | BoundPolicy<R>) & (bnd::exact | bnd::direct | bnd::indexed))
        | (keep_f64 ? bnd::real : none) | (keep_f32 ? bnd::f32 : none);
    using result = bound<result_grid, rep != none ? rep : checked>;

    template <policy_flag G = F>
    static constexpr bool needs_overflow_check =
        ((G | F | BoundPolicy<L> | BoundPolicy<R>) & checked);

    // For a nonzero divisor the op fails only on the checked rational path
    // (overflow). So when the divisor excludes zero AND this is false, `div`
    // returns a plain `result` rather than optional<result>.
    static constexpr bool may_overflow_nonzero =
        !native_div && !fp_raw<result> && (needs_overflow_check<F> != 0);

    // Real division can still fail on a zero divisor, so it uses the same
    // return-type rule as the rest: plain `result` when the op cannot fail
    // (overflow-action, or the divisor grid excludes zero with no rational
    // overflow), else optional<result>. Real has no rational overflow, so
    // may_overflow_nonzero is false for it (above).
    template <typename A>
    using div_return_t = std::conditional_t<
        overflow_action<plain<A>> || (DivisorExcludesZero<R> && !may_overflow_nonzero),
        result,
        slim::optional<result>>;

    template <policy_flag G = F, typename E = empty_ref, typename A = no_action>
    static constexpr div_return_t<A> div(L, R, policy<G, E> = {}, A&& = {});
  };

  //---------------------------------------------------------------------------
  // div
  //---------------------------------------------------------------------------
  template<boundable L, boundable R, policy_flag F>
  template<policy_flag G, typename E, typename A>
  constexpr auto division<L,R,F>::div(L lhs, R rhs, policy<G, E> policy, A&& action) -> div_return_t<A>
  {
    // `fail` must stay well-formed even when div_return_t narrowed to plain
    // `result` (divisor excludes zero, no overflow); there every call to it is
    // removed by the guards below, so the final arm is dead (return-type only).
    // Shared by the real and non-real paths (real fails only on a zero divisor).
    [[maybe_unused]] auto fail = [&](errc code, const char* what) -> div_return_t<A> {
      if constexpr (overflow_action<plain<A>>)
        return report_or_nullopt<result>(action, policy, code, what);   // -> result
      else if constexpr (!DivisorExcludesZero<R> || may_overflow_nonzero)
        return report_or_nullopt<result>(action, policy, code, what);   // -> optional<result>
      else
        return result{};   // unreachable: divisor excludes zero, op cannot fail
    };

    // Div-by-zero check elided when R's grid excludes zero, or `ignore_zero` is
    // set (zero divisor is then UB, matching the `/= 0` no-op). The fail arms stay
    // keyed on DivisorExcludesZero (which narrows the return type; ignore_zero doesn't).
    [[maybe_unused]] constexpr bool zero_unchecked = DivisorExcludesZero<R>
        || (((G | F | BoundPolicy<L> | BoundPolicy<R>) & ignore_zero) != 0);

    if constexpr (fp_raw<result>)
    {
      // Real division reports zero like every other path (throw / report /
      // action / nullopt). Finite operands keep the quotient finite, so no
      // non-finite ever reaches storage.
      if constexpr (!zero_unchecked)
        if (as_double(rhs) == 0.0) return fail(errc::division_by_zero, "division by zero in div");
      return result::from_raw(raw_cast<result>(Grid<result>.snap_double(as_double(lhs) / as_double(rhs))));
    }
    else if constexpr (native_div_qformat)
    {
      // rhs.Raw == 0 iff rhs.value == 0 (Lower<R> == 0). Formula folds to
      // `(a << log2 N)/b` for power-of-two N — the native Q-format idiom.
      if constexpr (!zero_unchecked)
        if (rhs.raw() == 0) return fail(errc::division_by_zero, "division by zero in div");
      constexpr umax N = abs_den(Notch<L>.Denominator);
      return result::from_raw(raw_cast<result>(round_uquotient(
          static_cast<umax>(lhs.raw()) * N, static_cast<umax>(rhs.raw()), rmode)));
    }
    else if constexpr (native_div_integer)
    {
      imax rhs_val = to_value(rhs);
      if constexpr (!zero_unchecked)
        if (rhs_val == 0) return fail(errc::division_by_zero, "division by zero in div");
      result res;
      from_value(res, div_rounded(to_value(lhs), rhs_val, rmode));
      return res;
    }
    else if constexpr (needs_overflow_check<G>)
    {
      rational rhs_r = rhs;
      if constexpr (!zero_unchecked)
        if (rhs_r.Numerator == 0) return fail(errc::division_by_zero, "division by zero in div");
      auto q = as_rational(lhs) / rhs_r;
      if (!q) [[unlikely]] return fail(errc::overflow, "rational overflow in div");
      return result::from_raw(*q);
    }
    else
    {
      rational rhs_r = rhs;
      if constexpr (!zero_unchecked)
        if (rhs_r.Numerator == 0) return fail(errc::division_by_zero, "division by zero in div");
      return result::from_raw(rational::div_unchecked(as_rational(lhs), rhs_r));
    }
  }
  //---------------------------------------------------------------------------
  // modulo (requires integer-valued grids + snap)
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_flag F = none>
  struct modulo
  {
    static constexpr bool native_mod = integer_native_ops<L, R, F>;

    // Hard requirement, not a fallback: `a mod b` is only defined for integer
    // operands, so the grid must be integer-aligned with `snap` set.
    static_assert(native_mod, "modulo requires integer-valued grids and snap");

    static constexpr imax max_rem =
        std::max(abs_den(LowerImax<R>), abs_den(UpperImax<R>)) - 1;

    // Remainder consistent with the rounded quotient: r = a − round(a/b)·b. Under
    // truncation it takes the dividend's sign (non-negative for a non-negative
    // dividend grid); any directional mode can flip the sign, so the grid widens
    // to the symmetric ±max_rem (|r| ≤ max_rem for every mode).
    static constexpr round_mode rmode =
        div_round_mode(F | BoundPolicy<L> | BoundPolicy<R>);

    static constexpr grid result_grid =
        (rmode == round_mode::trunc && LowerImax<L> >= 0)
        ? grid{imax{0}, max_rem}
        : grid{-max_rem, max_rem};

    using result = bound<result_grid>;

    // Modulo never overflows (the remainder fits result_grid), so the only
    // failure is a zero divisor — excluded by the grid → plain `result`.
    template <typename A>
    using mod_return_t = std::conditional_t<
        overflow_action<plain<A>> || DivisorExcludesZero<R>,
        result,
        slim::optional<result>>;

    template <policy_flag G = F, typename E = empty_ref, typename A = no_action>
    static constexpr mod_return_t<A> mod(L, R, policy<G, E> = {}, A&& = {});
  };

  template<boundable L, boundable R, policy_flag F>
  template<policy_flag G, typename E, typename A>
  constexpr auto modulo<L,R,F>::mod(L lhs, R rhs, policy<G, E> policy, A&& action) -> mod_return_t<A>
  {
    imax rhs_val = to_value(rhs);
    // Zero check elided when R's grid excludes zero (mod_return_t is plain
    // `result`) or `ignore_zero` is set (zero divisor is then UB, matching `%= 0`).
    constexpr bool zero_unchecked = DivisorExcludesZero<R>
        || (((G | F | BoundPolicy<L> | BoundPolicy<R>) & ignore_zero) != 0);
    if constexpr (!zero_unchecked)
      if (rhs_val == 0)
        return report_or_nullopt<result>(action, policy, errc::division_by_zero,
                                         "division by zero in mod");
    result res;
    // Remainder consistent with the rounded quotient (trunc → C++ `%`).
    const imax lhs_val = to_value(lhs);
    from_value(res, lhs_val - div_rounded(lhs_val, rhs_val, rmode) * rhs_val);
    return res;
  }
} // namespace bnd::detail


// ======================================================================
//  bound/predicates.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// predicates — pure inspection (no conversion, no state change) to branch
// before a construction that might throw or land on the sentinel:
//   will_conversion_overflow<B>(v) — v falls outside B's interval.
//   will_conversion_trunc<B>(v) — v is in-range but off-notch (would round).
//   is_conversion_lossy<B>(v)      — OR of the two.
//---------------------------------------------------------------------------
namespace bnd
{
  template <boundable B, numeric A>
  [[nodiscard]] constexpr bool will_conversion_overflow(A value) noexcept
  {
    return not includes(Interval<B>, detail::as_rational(value));
  }

  template <boundable B, numeric A>
  [[nodiscard]] constexpr bool will_conversion_trunc(A value) noexcept
  {
    if constexpr (detail::rational_raw<B>)
      return false;                       // rational raw stores any value exactly
    bnd::detail::rational r = detail::as_rational(value);
    if (not includes(Interval<B>, r))
      return false;                       // out-of-range — overflow, not truncation
    // In-range: truncation occurs iff (value - Lower) / Notch is non-integer.
    auto offset = (r - Lower<B>) / Notch<B>;
    return !offset.has_value() || detail::abs_den(offset->Denominator) != 1;
  }

  template <boundable B, typename A>
  [[nodiscard]] constexpr bool is_conversion_lossy(A value) noexcept
  {
    return will_conversion_overflow<B>(value)
        || will_conversion_trunc<B>(value);
  }
} // namespace bnd



// Forward-declare the `bnd::math` entry points used in-class, so the bodies
// pass `-Wtemplate-body` without pulling cmath.hpp in unconditionally (its
// definitions live there).
namespace bnd::math
{
  template <boundable Out, boundable In> constexpr Out floor_impl(In x) noexcept;
  template <boundable Out, boundable In> constexpr Out ceil_impl (In x) noexcept;
  template <boundable Out, boundable In> constexpr Out round_impl(In x) noexcept;
  template <boundable Out, boundable In> constexpr Out trunc_impl(In x) noexcept;
  template <boundable Out, boundable In> constexpr Out abs_impl  (In x) noexcept;
  template <boundable In> constexpr auto floor(In x) noexcept;
  template <boundable In> constexpr auto ceil (In x) noexcept;
  template <boundable In> constexpr auto round(In x) noexcept;
  template <boundable In> constexpr auto trunc(In x) noexcept;
  template <boundable In> constexpr auto abs  (In x) noexcept;
}

//---------------------------------------------------------------------------
// bound — the public struct users include. Defines `bound<G, P>` and its
// per-instance operators; free-function arithmetic and `bound_range` also live
// here. Heavy lifting is delegated to addition/multiplication/division.hpp
// (per-operator code), assignment.hpp (narrowing/clamp/wrap/sentinel), and
// generic.hpp/policy.hpp (traits + policy machinery).
//---------------------------------------------------------------------------
namespace slim
{
  template <bnd::grid G, bnd::policy_flag P>
  struct sentinel_traits<bnd::bound<G, P>>
  {
    protected:
      static constexpr bnd::bound<G, P> sentinel() noexcept;
      static constexpr bool is_sentinel(const bnd::bound<G, P>& v) noexcept;
  };
} // namespace slim

namespace bnd
{
  //---------------------------------------------------------------------------
  // bound
  //---------------------------------------------------------------------------
  template<grid G, policy_flag P>
  struct bound
  {
    static_assert(grid::validate<G>());
    static_assert(!(P & clamp) || !(P & wrap), "clamp and wrap are mutually exclusive");
    static_assert(!(P & sentinel) || !(P & clamp), "sentinel and clamp are mutually exclusive");
    static_assert(!(P & sentinel) || !(P & wrap), "sentinel and wrap are mutually exclusive");
#ifndef BND_MATH_FIXED
    // Under the default (double) engine the `real` policy is double-backed, and
    // its value snaps to the grid (Lower + k·Notch). That snap is only exact
    // when the grid is dyadic — power-of-two notch and Lower — so grid points
    // are representable in IEEE-754 double. A continuous grid (Notch == 0) has
    // no grid to snap to. Anything else is rejected here rather than silently
    // demoted to integer storage.
    static_assert(!has_flag(P, real) || detail::dyadic_grid<G> || G.Notch == 0,
                  "bnd: the `real`/`f64` policy requires a dyadic grid (power-of-two "
                  "notch and Lower, so values are exactly representable in double)");
    static_assert(!has_flag(P, f32) || detail::dyadic_grid<G> || G.Notch == 0,
                  "bnd: the `f32` policy requires a dyadic grid (power-of-two notch "
                  "and Lower); values must also fit float's 24-bit significand "
                  "(checked at storage selection — see `float_exact`)");
#endif
    // Representation flags vs grid shape (exact has no requirement; a result
    // policy may carry several flags — storage selection resolves widest-wins,
    // so no mutual-exclusion asserts here).
    static_assert(!has_flag(P, direct) || G.Notch == 1,
                  "bnd: the `direct` policy (raw == value as a plain integer) "
                  "requires Notch == 1");
    static_assert(!has_flag(P, indexed) || G.Notch != 0,
                  "bnd: the `indexed` policy (raw == 0-based notch index) "
                  "requires a notch (Notch != 0)");

    using negative = bound<-G, P>;
    using raw_type = detail::storage_for<G, P>;

    private:
    raw_type Raw;

    public:
    // raw() — access escape hatch, symmetric with `from_raw`. Read overload
    // under every policy (read-only C interop: `&std::as_const(b).raw()`). The
    // mutable overload is gated to `unsafe` — only a bound that has opted out of
    // every check can honestly hand out a writable storage handle; writing an
    // out-of-range raw elsewhere would make conversions lie, so it's a compile error.
    [[nodiscard]] constexpr raw_type const& raw() const noexcept { return Raw; }
    [[nodiscard]] constexpr raw_type&       raw()       noexcept
      requires (has_flag(P, unsafe)) { return Raw; }

    // Trivial default ctor — Raw is left uninitialized, like a built-in scalar: a
    // default-constructed bound has no value until assigned. (A previous checked
    // overload zero-filled Raw, which decoded to an out-of-range value or the {0,0}
    // rational sentinel for grids not containing 0 — a defined-but-invalid footgun.
    // Value-init `bound{}` still zero-fills where a zero raw is genuinely wanted.)
    constexpr bound() = default;

    // fp storage (f64/f32) holds the value as a floating raw directly. An
    // arithmetic rhs casts straight to double; a bound rhs goes through its exact
    // rational view.
    private:
    template <numeric A>
    constexpr double to_double(A const& value)
    {
      if constexpr (std::is_arithmetic_v<A>) return value;
      else                                   return static_cast<double>(detail::as_rational(value));
    }
    public:
    // Snap a value onto fp storage: lossless on the (fp-exact) dyadic grid — the
    // snap is computed in double and narrowed to the raw type (double or float),
    // which is exact because every grid point fits the raw's significand. Out-of-
    // range values run the same policy cascade as the fractional path (clamp →
    // wrap → sentinel/checked-report → store as-is).
    constexpr void store_fp(double v)
    {
      // NaN/±inf would reach snap_double's integer cast (UB); reject like the
      // non-real path. `v - v` is 0 for every finite v, NaN otherwise.
      if (!(v - v == 0))
        detail::raise(errc::not_finite, "non-finite double");
      const double lo = static_cast<double>(G.Interval.Lower);
      const double hi = static_cast<double>(G.Interval.Upper);
      if (v < lo || v > hi)
      {
        if constexpr (has_flag(P, clamp))
          v = v < lo ? lo : hi;
        else if constexpr (has_flag(P, wrap))
        {
          // Fold into [Lower, Lower + range), range = span + notch — the same
          // convention as the fractional apply_wrap. floor(q) without an
          // unguarded imax cast: for |q| >= 2^52 the double is already integral
          // (floor(q) == q), else narrow to imax (safe) and adjust toward -inf.
          const double range = hi - lo + static_cast<double>(G.Notch);
          const double q = (v - lo) / range;
          const double aq = q < 0 ? -q : q;
          double kd;
          if (aq >= 4503599627370496.0)            // 2^52
            kd = q;
          else
          {
            const imax k = static_cast<imax>(q);   // |q| < 2^52 < imax
            kd = static_cast<double>(k);
            if (q < 0 && kd != q) kd -= 1.0;        // floor toward -inf
          }
          v -= kd * range;
        }
        else if (detail::domain_fail(*this, make_policy<P>()))
          return;            // sentinel stored / reported (error_code mode)
        // no handler (unchecked policy): fall through and store snapped as-is
      }
      Raw = static_cast<raw_type>(G.snap_double(v));   // narrow to float for f32 (lossless)
    }

    template <numeric A>
    constexpr void store_value(A const& value)
    {
      if constexpr (detail::fp_raw<bound>)
        store_fp(to_double(value));
      else if constexpr (is_bound_v<A>)
      {
        // A `real` SOURCE holds its value as a double raw; the assignment engine's
        // integer offset formula (Lower + raw·Notch) would misread it. Extract as
        // a double and route through the arithmetic-source path.
        if constexpr (detail::fp_raw<A>)
          detail::assignment<bound, double>::assign(*this, detail::as_double(value), make_policy<P>());
        else
          detail::assignment<bound, A>::assign(*this, value, make_policy<P>());
      }
      else
        detail::assignment<bound, A>::assign(*this, value, make_policy<P>());
    }

    template <numeric A>
      requires bound_assignable<bound, A, P>
    constexpr bound(A value)
    { store_value(value); }

    template <numeric A, typename Pol>
      requires bound_assignable<bound, A, P | detail::policy_flags_of<std::remove_cvref_t<Pol>>>
    constexpr bound(A value, Pol&& pol)   // if assign reports an error (ec mode), Raw is
    {                                     // left ill-defined — check the error before reading
      // The one-shot `pol` widens the assignable check (a clamp/round passed here
      // relaxes the notch/interval clause), so a notch-incompatible boundable source
      // is accepted — e.g. clamp_round<B>(some_bound). Body honours `pol` as before.
      if constexpr (detail::fp_raw<bound>)
        store_fp(to_double(value));
      else
        detail::assignment<bound, A>::assign(*this, value, pol);
    }

    // Error-code construction: `bound x(value, ec)`. Needs its own overload (a raw
    // error_code would bind the Pol&& template above). On a reported (out-of-range)
    // error, ec is set and the bound's value is ill-defined — do not read it without
    // checking ec first.
    template <numeric A>
      requires bound_assignable<bound, A, P>
    constexpr bound(A value, errc& ec)
    {
      if constexpr (detail::fp_raw<bound>)
        store_fp(to_double(value));
      else
        detail::assignment<bound, A>::assign(*this, value, make_policy<P>(ec));
    }

    // optional<A> sink — unwrap once at the construction boundary so callers can
    // chain checked arithmetic without per-step `.value()`. Throws on `nullopt`.
    template <numeric A>
      requires bound_assignable<bound, A, P>
    constexpr bound(slim::optional<A> const& value)
    { store_value(value.value()); }

    template <numeric B>
      requires bound_assignable<bound, B, P>
    constexpr bound& operator=(B const& other)
    { store_value(other); return *this; }

    template <numeric B>
      requires bound_assignable<bound, B, P>
    constexpr bound& operator=(slim::optional<B> const& other)
    { store_value(other.value()); return *this; }

    // ---- Diagnostic fallbacks (default on; -DBND_STRICT_SFINAE removes them) ----
    // When a source is numeric but NOT assignable to this bound, every constrained
    // sink above drops out of overload resolution and the compiler emits a bare
    // "could not convert" — losing the reason. These complementary overloads (enabled
    // exactly when `bound_assignable` is false) catch that case and turn it into the
    // named per-clause notes from `bound_assignable_why` (interval-excludes /
    // incompatible-notch). The trade: they make `bound` *appear* is_constructible /
    // assignable from incompatible types (the static_assert is in the body, not the
    // immediate context, so trait probes return true then hard-error only on real
    // use). Define BND_STRICT_SFINAE to drop them and restore SFINAE-pure traits for
    // metaprogramming that probes convertibility (variant/optional/`if constexpr`).
#ifndef BND_STRICT_SFINAE
    template <numeric A>
      requires (!bound_assignable<bound, A, P>)
    constexpr bound(A)
    { static_assert(bound_assignable_why<bound, A, P>::value,
        "bnd: cannot construct this bound from the value — see the per-clause notes above"); }

    template <numeric A>
      requires (!bound_assignable<bound, A, P>)
    constexpr bound(slim::optional<A> const&)
    { static_assert(bound_assignable_why<bound, A, P>::value,
        "bnd: cannot construct this bound from the optional's value — see the per-clause notes above"); }

    template <numeric B>
      requires (!bound_assignable<bound, B, P>)
    constexpr bound& operator=(B const&)
    { static_assert(bound_assignable_why<bound, B, P>::value,
        "bnd: cannot assign this value to this bound — see the per-clause notes above"); return *this; }

    template <numeric B>
      requires (!bound_assignable<bound, B, P>)
    constexpr bound& operator=(slim::optional<B> const&)
    { static_assert(bound_assignable_why<bound, B, P>::value,
        "bnd: cannot assign this optional's value to this bound — see the per-clause notes above"); return *this; }
#endif

    // Trusted construction from a storage-layout raw — no validation; the caller
    // asserts `r` is a valid slot. Entry point for tests, fast paths, and same-grid
    // raw transfer (e.g. `unchecked_cast`).
    [[nodiscard]] static constexpr bound from_raw(raw_type r) noexcept
    { bound b; b.Raw = r; return b; }

    // The reserved empty slot used by `slim::optional<bound>` and `sentinel`
    // policy, without poking `Raw`.
    [[nodiscard]] static constexpr bound make_sentinel() noexcept
    { return from_raw(detail::sentinel_raw<bound>()); }

    // Canonical "is this slot empty?" check under `sentinel` policy.
    [[nodiscard]] constexpr bool is_sentinel() const noexcept
    {
      return detail::raw_is_sentinel<bound>(Raw);
    }

    // to<T>() emptiness predicate: under sentinel policy an empty slot reports
    // overflow; compiles to constant `false` under every other policy (the
    // reserved slot is unreachable). Internal: used only by the to<T>() overloads.
    private:
    [[nodiscard]] constexpr bool is_sentinel_under_policy() const noexcept
    {
      if constexpr (has_flag(P, sentinel)) return is_sentinel();
      else                               return false;
    }
    public:

    // Conversion summary:
    //   operator imax     — implicit, when the grid is notch-aligned and fits in
    //                       int64 (else use `to<imax>()`). Also the `vec[b]` index
    //                       path. No second implicit integer operator (would make
    //                       `imax_var += b` ambiguous).
    //   operator rational — implicit; lossless and exact.
    //   operator double   — implicit for `real` bounds (dyadic grid → lossless);
    //                       explicit otherwise and gated on a rounding flag.
    //                       Strict bounds opt in via `to<double>().value()`.
    //   to<T>()           — typed-error narrowing/widening → `expected<T, errc>`
    //                       (overflow / domain_error / sentinel-state).
    //   as<T>()           — non-expected sibling; asserts on sentinel. For known-
    //                       in-range sites (array indexing). FP shares the gate.
    //   to<T>(b)/as<T>(b) — free-function forms, for generic code.
    constexpr operator imax() const
      requires (detail::notch_is_unit_integer<G>
             && G.Interval.Lower >= bnd::detail::rational{std::numeric_limits<imax>::min()}
             && G.Interval.Upper <= bnd::detail::rational{std::numeric_limits<imax>::max()})
    { return detail::to_value(*this); }

    constexpr explicit(!has_flag(P, real) && !has_flag(P, f32)) operator double() const
      requires ((P & (round_floor | round_ceil | round_nearest
                    | round_half_even | snap)) != 0)
    { return detail::as_double(*this); }

    constexpr operator bnd::detail::rational() const
    {
      if constexpr (G.Interval.Lower == G.Interval.Upper)
        return G.Interval.Lower;

      if constexpr (!detail::index_raw<bound>)
        return Raw;

      // Q-format-with-integer-Lower fast path skips the generic path's three
      // rational ops. Falls through to the rational path when the raw is too wide
      // to widen safely (e.g. uint64 from a Q16.16 × Q16.16 result type).
      if constexpr (detail::HasQFormatFastPath<bound>)
        return detail::q_format_decode(*this);

      return (*(Raw * G.Notch) + G.Interval.Lower).value();
    }

    // to<T>() — typed-error scalar extraction (mirrors rational::to<T>, extended
    // to signed and floating point). Returns `errc::not_a_value` (sentinel
    // state), `errc::overflow` (out of T's range), and `errc::domain_error`
    // (negative into unsigned T); fractional truncation is silent.
    template <std::unsigned_integral T>
    [[nodiscard]] constexpr slim::expected<T, errc> to() const
    {
      if (is_sentinel_under_policy()) return slim::unexpected{errc::not_a_value};

      constexpr bool needs_neg_check = (Lower<bound> < 0);
      constexpr bool needs_max_check =
          (Upper<bound> > bnd::detail::rational{std::numeric_limits<T>::max()});

      if constexpr (!needs_neg_check && !needs_max_check)
        return static_cast<T>(detail::to_value(*this));
      else
      {
        auto r = detail::as_rational(*this);
        if constexpr (needs_neg_check)
          if (r < 0) return slim::unexpected{errc::domain_error};
        if constexpr (needs_max_check)
          if (r > bnd::detail::rational{std::numeric_limits<T>::max()})
            return slim::unexpected{errc::overflow};
        return static_cast<T>(trunc(r));
      }
    }

    template <std::signed_integral T>
    [[nodiscard]] constexpr slim::expected<T, errc> to() const
    {
      if (is_sentinel_under_policy()) return slim::unexpected{errc::not_a_value};

      constexpr bool needs_min_check =
          (Lower<bound> < bnd::detail::rational{std::numeric_limits<T>::min()});
      constexpr bool needs_max_check =
          (Upper<bound> > bnd::detail::rational{std::numeric_limits<T>::max()});

      if constexpr (!needs_min_check && !needs_max_check)
        return static_cast<T>(detail::to_value(*this));
      else
      {
        auto r = detail::as_rational(*this);
        if constexpr (needs_min_check)
          if (r < bnd::detail::rational{std::numeric_limits<T>::min()})
            return slim::unexpected{errc::overflow};
        if constexpr (needs_max_check)
          if (r > bnd::detail::rational{std::numeric_limits<T>::max()})
            return slim::unexpected{errc::overflow};
        return static_cast<T>(trunc(r));
      }
    }

    template <std::floating_point T>
    [[nodiscard]] constexpr slim::expected<T, errc> to() const
    {
      if (is_sentinel_under_policy()) return slim::unexpected{errc::not_a_value};
      return static_cast<T>(detail::as_double(*this));
    }

    // as<T>() — non-expected sibling of to<T>(): returns T directly, letting any
    // error surface as bad_expected_access from `to<T>().value()`. For known-in-
    // range sites (array indexing, capacity arithmetic). FP targets share operator
    // double's policy gate, so a strict bound rejects `b.as<double>()` too.
    template <typename T>
    [[nodiscard]] constexpr T as() const
      requires (!std::floating_point<T>
             || (P & (round_floor | round_ceil | round_nearest
                    | round_half_even | snap)) != 0)
    { return to<T>().value(); }

    // numerator() / denominator() — the exact value of a fractional bound as an
    // integer pair (sign on the numerator, denominator positive). The supported
    // exact read-out that keeps callers in plain integers. Integer-notch ⇒ den == 1.
    [[nodiscard]] constexpr imax numerator() const
    {
      auto r = detail::as_rational(*this);
      return (r.Denominator < 0) ? -r.Numerator : r.Numerator;
    }

    [[nodiscard]] constexpr imax denominator() const
    {
      auto r = detail::as_rational(*this);
      return detail::abs_den(r.Denominator);
    }

    // Integer reductions (floor/ceil/round/trunc) and abs live as free
    // functions in `bnd::math` — `bnd::math::floor(b)` etc. (auto-deduced Out)
    // or `bnd::math::floor_impl<Out>(b)` for an explicit output grid. There is
    // deliberately no member-syntax alias: one spelling, in `<bound/cmath.hpp>`.

    [[nodiscard]] constexpr negative operator-() const
    {
      negative neg;
      if constexpr (detail::fp_raw<bound>)
        neg = negative::from_raw(-Raw);
      else if constexpr (detail::rational_raw<bound>)
        neg = negative::from_raw(-(Raw));
      else if constexpr (!detail::index_raw<bound> || !detail::index_raw<negative>)
        detail::from_value(neg, -detail::to_value(*this));
      else
        // Unsigned-offset fast path: with `value = Raw*Notch + Lower`, negating
        // is `NotchCount - Raw` (index from the opposite end) — no rational ops.
        // Unreachable for direct storage, so `Raw` here is guaranteed an offset.
        neg = negative::from_raw(detail::raw_cast<negative>(detail::NotchCount<bound> - Raw));
      return neg;
    }

    // policy<F>() — per-operation policy override. On an lvalue it returns a
    // policy_ref holding `*this` by reference (needed so `b.policy<…>() = x` writes
    // back into b, and cheap for the common immediate use). On an *rvalue* receiver
    // it returns a policy_buffer that OWNS the moved-in value, so a snapped temporary
    // survives being returned/stored (`return (a*b).with_snap();`) — no dangling.
    template <policy_flag F = none>
    [[nodiscard]] constexpr auto policy() &
    {
       auto pol = make_policy<P | F>();
       return detail::policy_ref<bound, decltype(pol)>{*this, pol};
    }

    template <policy_flag F = none>
    [[nodiscard]] constexpr auto policy() &&
    {
       auto pol = make_policy<P | F>();
       return detail::policy_buffer<bound, decltype(pol)>{std::move(*this), pol};
    }

    template <policy_flag F = none>
    [[nodiscard]] constexpr auto policy(errc& ec)
    {
       auto pol = make_policy<P | F>(ec);
       return detail::policy_ref<bound, decltype(pol)>{*this, pol};
    }

    // with_snap<Mode>() — opt this assignment into snapping with the given rounding
    // mode. Bare `with_snap()` is truncate-toward-zero (Mode == snap); pass an
    // explicit mode for the others: with_snap<round_nearest>(), <round_floor>,
    // <round_ceil>, <round_half_even>. The `&&` overloads forward the rvalue receiver
    // so a temporary yields a value-owning policy_buffer (see policy<F>() above) —
    // `*this` is an lvalue inside the body, hence the explicit std::move.
    template <policy_flag Mode = snap>
    [[nodiscard]] constexpr auto with_snap() &
    {
      static_assert(has_flag(Mode, snap),
        "with_snap<Mode>: Mode must be a snapping mode — snap (truncate), round_nearest, "
        "round_floor, round_ceil, or round_half_even");
      return policy<Mode>();
    }
    template <policy_flag Mode = snap>
    [[nodiscard]] constexpr auto with_snap() &&
    {
      static_assert(has_flag(Mode, snap),
        "with_snap<Mode>: Mode must be a snapping mode — snap (truncate), round_nearest, "
        "round_floor, round_ceil, or round_half_even");
      return std::move(*this).template policy<Mode>();
    }
    [[nodiscard]] constexpr auto with_clamp() &         { return policy<clamp>(); }
    [[nodiscard]] constexpr auto with_clamp() &&        { return std::move(*this).template policy<clamp>(); }
    [[nodiscard]] constexpr auto with_wrap()  &         { return policy<wrap>(); }
    [[nodiscard]] constexpr auto with_wrap()  &&        { return std::move(*this).template policy<wrap>(); }

    // Shared builder for the single-action fluent hooks below. Merges the tag's
    // implied policy flag, then returns a policy_ref bound to *this carrying the
    // tagged action. Each on_* hook is a thin wrapper that fixes the tag.
    // Internal: consumed only by the on_* hooks below.
    private:
    template <template <class> class Tag, typename A>
    [[nodiscard]] constexpr auto make_action_ref(A&& action)
    {
       using tag = Tag<std::remove_cvref_t<A>>;
       auto pol = make_policy<P | detail::implied_flags<tag>>();
       return detail::policy_ref<bound, decltype(pol), tag>{
         *this, pol, tag{std::forward<A>(action)}};
    }
    public:

    template <typename A>
    [[nodiscard]] constexpr auto on_wrap(A&& a)     { return make_action_ref<on_wrap_t>(std::forward<A>(a)); }
    template <typename A>
    [[nodiscard]] constexpr auto on_clamp(A&& a)    { return make_action_ref<on_clamp_t>(std::forward<A>(a)); }
    template <typename A>
    [[nodiscard]] constexpr auto on_error(A&& a)    { return make_action_ref<on_error_t>(std::forward<A>(a)); }
    template <typename A>
    [[nodiscard]] constexpr auto on_sentinel(A&& a) { return make_action_ref<on_sentinel_t>(std::forward<A>(a)); }
    template <typename A>
    [[nodiscard]] constexpr auto on_overflow(A&& a) { return make_action_ref<on_overflow_t>(std::forward<A>(a)); }

    // Multi-action entry point: combine N tagged actions into one policy_ref.
    // policy_ref rejects mutually exclusive combinations at compile time. E.g.
    // `b.with(on_overflow(λ1), on_clamp(λ2)) += rhs` — overflow probe fires λ1,
    // post-probe narrowing fires λ2.
    template <typename... Actions>
    [[nodiscard]] constexpr auto with(Actions&&... actions)
    {
       constexpr policy_flag merged = detail::merged_implied_flags<Actions...>;
       auto pol = make_policy<P | merged>();
       return detail::policy_ref<bound, decltype(pol), std::remove_cvref_t<Actions>...>{
         *this, pol,
         std::tuple<std::remove_cvref_t<Actions>...>{std::forward<Actions>(actions)...}};
    }

    template <boundable R>
    constexpr bound& operator+=(R const& rhs)
    {
      // Fast path: raw-level integer addition, safe when raw_a + raw_b is the raw
      // of value_a + value_b — direct storage, or offset encoding with Lower==0 both.
      if constexpr (!detail::rational_raw<bound> && !detail::rational_raw<R>
                    && Notch<bound> == Notch<R>
                    && (!detail::index_raw<R>
                        || (Lower<bound> == 0 && Lower<R> == 0)))
      {
        if constexpr (P & (clamp | wrap | checked | sentinel))
        {
          imax new_raw = detail::raw_imax(*this) + detail::raw_imax(rhs);
          if (new_raw < detail::RawLo<bound> || new_raw > detail::RawHi<bound>)
            return apply_raw_overflow(new_raw);
          Raw = detail::raw_cast<bound>(new_raw);
        }
        else
          Raw += detail::raw_cast<bound>(rhs.raw());
        return *this;
      }
      else
      {
        *this = *this + rhs;
        return *this;
      }
    }

    private:
    // Out-of-range tail for raw-space compound arithmetic: dispatch on policy
    // (clamp/wrap/sentinel/checked) and store back to `Raw`. Called from
    // `operator+=(boundable)` only.
    constexpr bound& apply_raw_overflow(imax new_raw)
    {
      if constexpr (P & clamp)
        // RawLo/RawHi are already raw-space constants, so no raw_from_offset.
        Raw = detail::raw_cast<bound>(new_raw < detail::RawLo<bound> ? detail::RawLo<bound> : detail::RawHi<bound>);
      else if constexpr (P & wrap)
      {
        constexpr imax range = detail::RawHi<bound> - detail::RawLo<bound> + 1;
        new_raw = ((new_raw - detail::RawLo<bound>) % range + range) % range + detail::RawLo<bound>;
        Raw = detail::raw_cast<bound>(new_raw);
      }
      else if constexpr (P & sentinel)
        Raw = detail::sentinel_raw<bound>();
      else
        make_policy<P>().report(errc::domain_error);
      return *this;
    }
    public:

    //-----------------------------------------------------------------------
    // Compound assignment private helpers — extracted to keep each
    // `operator*=` body focused on its own arithmetic shape.
    //-----------------------------------------------------------------------
    private:
    template <typename Result>
    constexpr bound& assign_op_result(Result const& r)
    {
      if constexpr (requires { typename Result::value_type; })
        *this = r.value();
      else
        *this = r;
      return *this;
    }

    constexpr bool report_div_by_zero([[maybe_unused]] const char* msg)
    {
      if constexpr (!(P & ignore_zero))
        make_policy<P>().report(errc::division_by_zero);
      return false;   // caller returns *this directly
    }
    public:

    // Only a `rational` (a library type) may join a bound in a compound assign;
    // raw int/float/double are ill-formed — give the scalar a grid (`1_b` /
    // `just<1>` / `bound<{lo,hi}>{n}`), mirroring the binary operators.
    template <std::same_as<bnd::detail::rational> A>
    constexpr bound& operator+=(A const& rhs)
    { return assign_op_result(bnd::detail::rational{*this} + rhs); }

    template <boundable R>
    constexpr bound& operator-=(R const& rhs)
    { return *this += (-rhs); }

    template <std::same_as<bnd::detail::rational> A>
    constexpr bound& operator-=(A const& rhs)
    { return assign_op_result(bnd::detail::rational{*this} - rhs); }

    template <boundable R>
    constexpr bound& operator*=(R const& rhs)
    { return assign_op_result(*this * rhs); }

    template <boundable R>
    constexpr bound& operator/=(R const& rhs)
    {
      if (rhs == 0)
      { report_div_by_zero("operator/= division by zero"); return *this; }
      return assign_op_result(*this / rhs);
    }

    template <boundable R>
    constexpr bound& operator%=(R const& rhs)
    {
      if (rhs == 0)
      { report_div_by_zero("operator%= division by zero"); return *this; }
      return assign_op_result(mod(*this, rhs, make_policy<P>()));
    }

    template <std::same_as<bnd::detail::rational> A>
    constexpr bound& operator*=(A const& rhs)
    { return assign_op_result(bnd::detail::rational{*this} * rhs); }

    template <std::same_as<bnd::detail::rational> A>
    constexpr bound& operator/=(A const& rhs)
    {
      if (detail::is_canonical_zero(rhs))
      { report_div_by_zero("operator/= division by zero"); return *this; }
      return assign_op_result(bnd::detail::rational{*this} / rhs);
    }

    constexpr bound& operator++()    { return *this += bnd::detail::rational{1}; }
    constexpr bound  operator++(int) { bound t = *this; ++*this; return t; }
    constexpr bound& operator--()    { return *this -= bnd::detail::rational{1}; }
    constexpr bound  operator--(int) { bound t = *this; --*this; return t; }

    template <numeric A>
    [[nodiscard]] static constexpr slim::expected<bound, errc> try_make(A value)
    {
      errc ec{};
      bound result;
      detail::assignment<bound, A>::assign(result, value, make_policy<P>(ec));
      if (ec != errc{}) return slim::unexpected{ec};
      // For `sentinel` policy types, an out-of-range write silently sets
      // result.Raw to the sentinel; surface that as a domain error.
      if constexpr (has_flag(P, sentinel))
        if (detail::raw_is_sentinel<bound>(result.Raw))
          return slim::unexpected{errc::domain_error};
      return result;
    }
  };

  //---------------------------------------------------------------------------
  // to<T>(b) / as<T>(b) — free-function forms, for generic code that would
  // otherwise need the `.template` disambiguator. Same semantics as the members.
  //---------------------------------------------------------------------------
  template <typename T, boundable B>
  [[nodiscard]] constexpr auto to(B const& b)
    requires requires { b.template to<T>(); }
  { return b.template to<T>(); }

  template <typename T, boundable B>
  [[nodiscard]] constexpr T as(B const& b)
    requires requires { b.template as<T>(); }
  { return b.template as<T>(); }

  //---------------------------------------------------------------------------
  // comparison
  //---------------------------------------------------------------------------
  template <boundable L, boundable R>
  constexpr auto operator<=>(L const& lhs, R const& rhs)
  {
    // same grid: Raw is monotonically ordered regardless of storage kind
    if constexpr (Grid<L> == Grid<R>)
      return lhs.raw() <=> rhs.raw();
    // double-backed (`real`) operand: compare in double (raw_imax would truncate)
    else if constexpr (detail::fp_raw<L> || detail::fp_raw<R>)
      return detail::as_double(lhs) <=> detail::as_double(rhs);
    // both integer-direct (notch=1, Raw==value): compare as integers
    else if constexpr (!detail::rational_raw<L> && !detail::rational_raw<R>
                       && !detail::index_raw<L> && !detail::index_raw<R>)
      return detail::raw_imax(lhs) <=> detail::raw_imax(rhs);
    else
      return detail::as_rational(lhs) <=> detail::as_rational(rhs);
  }

  template <boundable L, boundable R>
  constexpr bool operator==(L const& lhs, R const& rhs)
  {
    if constexpr (Grid<L> == Grid<R>)
      return lhs.raw() == rhs.raw();
    else if constexpr (detail::fp_raw<L> || detail::fp_raw<R>)
      return detail::as_double(lhs) == detail::as_double(rhs);
    else if constexpr (!detail::rational_raw<L> && !detail::rational_raw<R>
                       && !detail::index_raw<L> && !detail::index_raw<R>)
      return detail::raw_imax(lhs) == detail::raw_imax(rhs);
    else
      return detail::as_rational(lhs) == detail::as_rational(rhs);
  }

  template <boundable B, arithmetic A>
  constexpr auto operator<=>(B const& lhs, A rhs)
  {
    if constexpr (detail::value_raw<B>)
      return detail::raw_imax(lhs) <=> static_cast<imax>(rhs);
    else
      return detail::as_rational(lhs) <=> bnd::detail::rational{rhs};
  }

  template <boundable B, arithmetic A>
  constexpr bool operator==(B const& lhs, A rhs)
  {
    if constexpr (detail::value_raw<B>)
      return detail::raw_imax(lhs) == static_cast<imax>(rhs);
    else
      return detail::as_rational(lhs) == bnd::detail::rational{rhs};
  }

  //---------------------------------------------------------------------------
  // just
  //---------------------------------------------------------------------------
  template<auto value>
  inline constexpr auto just = bound<grid{value}>{value};

  //---------------------------------------------------------------------------
  // zero / one — universal exact constants. Single-point bounds that assign into
  // any grid able to represent the value (compile-time checked) and otherwise
  // behave as 0 / 1. `b = zero;` is a compile error when 0 is not on b's grid.
  //---------------------------------------------------------------------------
  inline constexpr auto zero = just<0>;
  inline constexpr auto one  = just<1>;

  //---------------------------------------------------------------------------
  // _b literal — compile-time `bound<{V, V}>` from a numeric literal.
  //   5_b           // bound<{5, 5}>            integer
  //   1.25_b        // bound<{rational{5,4}}>   decimal
  //   1.5e2_b       // bound<{150}>             decimal scientific
  //   0xff_b        // bound<{255}>             hex integer
  //   0b1010_b      // bound<{10}>              binary integer
  //   0x1p15_b      // bound<{32768}>           hex with 2^N exponent (Q-format)
  //   0x1p-15_b     // bound<{rational{1,32768}}>   1/2^15 grid notch
  //   0x1.8p3_b     // bound<{12}>              hex float
  //   1'000_b       // bound<{1000}>            digit separator
  //
  // Parse is exact (no double round-trip); same parser backs `_r` in
  // rational.hpp. `-1.5_b` parses as `-(1.5_b)`.
  //---------------------------------------------------------------------------
  template<char... Chars>
  constexpr auto operator""_b() { return just<detail::_detail::parse_b_literal<Chars...>()>; }

  //---------------------------------------------------------------------------
  // operator++ / operator-- for slim::optional<bound>
  //---------------------------------------------------------------------------
  template <boundable B>
    requires (has_flag(BoundPolicy<B>, sentinel))
  constexpr slim::optional<B>& operator++(slim::optional<B>& opt)
  {
    if (opt) ++(*opt);
    return opt;
  }

  template <boundable B>
    requires (has_flag(BoundPolicy<B>, sentinel))
  constexpr slim::optional<B> operator++(slim::optional<B>& opt, int)
  { auto t = opt; ++opt; return t; }

  template <boundable B>
    requires (has_flag(BoundPolicy<B>, sentinel))
  constexpr slim::optional<B>& operator--(slim::optional<B>& opt)
  {
    if (opt) --(*opt);
    return opt;
  }

  template <boundable B>
    requires (has_flag(BoundPolicy<B>, sentinel))
  constexpr slim::optional<B> operator--(slim::optional<B>& opt, int)
  { auto t = opt; --opt; return t; }

} // namespace bnd

namespace slim
{
  template <bnd::grid G, bnd::policy_flag P>
  constexpr bnd::bound<G, P> sentinel_traits<bnd::bound<G, P>>::sentinel() noexcept
  {
    return bnd::bound<G, P>::from_raw(bnd::detail::sentinel_raw<bnd::bound<G, P>>());
  }

  template <bnd::grid G, bnd::policy_flag P>
  constexpr bool sentinel_traits<bnd::bound<G, P>>::is_sentinel(const bnd::bound<G, P>& v) noexcept
  {
    // Rational uses a broader check (any zero denominator); real compares the
    // reserved NaN bit pattern (NaN != NaN); everything else is plain equality.
    return bnd::detail::raw_is_sentinel<bnd::bound<G, P>>(v.raw());
  }
} // namespace slim


// ======================================================================
//  bound/casts.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// Free-function casts complementing the constructors. Unlike a direct B{value}
// call, these read naturally in algorithm callbacks and make the intent (clamp
// vs. wrap vs. throw vs. trust) explicit at the call site.
//---------------------------------------------------------------------------
namespace bnd
{
  // Each cast constructs via the value+policy constructor, passing a one-shot
  // policy that overrides B's declared one for this conversion only.
  template <boundable B, numeric N>
  [[nodiscard]] constexpr B clamp_cast(N value)
  { return B{value, make_policy<clamp>()}; }

  // `wrap_cast` — modular semantics: the input is reduced into the target grid's
  // interval rather than clipped. For integer-style wraparound (angles, indices).
  template <boundable B, numeric N>
  [[nodiscard]] constexpr B wrap_cast(N value)
  { return B{value, make_policy<wrap>()}; }

  //---------------------------------------------------------------------------
  // clamp_floor / clamp_ceil / clamp_round — compose `clamp` with a rounding
  // mode: the canonical "double in, bounded integer out, never throw" pipeline.
  //---------------------------------------------------------------------------
  template <policy_flag RoundMode, boundable B, numeric N>
  [[nodiscard]] constexpr B clamp_with_rounding(N value)
  { return B{value, make_policy<clamp | RoundMode>()}; }

  template <boundable B, numeric N>
  [[nodiscard]] constexpr B clamp_floor(N value)
  { return clamp_with_rounding<round_floor, B>(value); }

  template <boundable B, numeric N>
  [[nodiscard]] constexpr B clamp_ceil(N value)
  { return clamp_with_rounding<round_ceil, B>(value); }

  template <boundable B, numeric N>
  [[nodiscard]] constexpr B clamp_round(N value)
  { return clamp_with_rounding<round_nearest, B>(value); }

  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr B checked_cast(A value)
  {
    if (will_conversion_overflow<B>(value))
      detail::raise(errc::domain_error, "checked_cast: value out of bound interval");
    if (will_conversion_trunc<B>(value))
      detail::raise(errc::rounding_error, "checked_cast: value does not land on notch");
    return B{value};
  }

  // `unchecked_cast` routes through `bound<G, unsafe>` so the compiler elides
  // every domain/round check. UB if the value is actually out of range.
  template <boundable B, arithmetic A>
  [[nodiscard]] constexpr B unchecked_cast(A value)
  {
    using twin = bound<Grid<B>, unsafe>;
    return B::from_raw(twin{value}.raw());   // same grid → identical raw layout
  }

} // namespace bnd


// ======================================================================
//  bound/arithmetic.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// Free-function arithmetic — wraps detail::addition/multiplication/division/
// modulo with caller-friendly overloads:
//   add(l, r) / add(l, r, policy<F>{}) / add(l, r, on_overflow(λ)) /
//   add(l, r, ec) / l + r
// Plus the variadic folds add_all/mul_all and *_into<Target>, and the
// slim::optional operator overloads (a nullopt operand propagates through).
//---------------------------------------------------------------------------
namespace bnd
{
  //---------------------------------------------------------------------------
  // add
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, detail::policy_like P = policy<>, typename A = no_action>
  [[nodiscard]] constexpr auto add(L const& lhs, R const& rhs, P&& policy = {}, A&& action = {})
  { return detail::addition<L,R>::add(lhs, rhs, std::forward<P>(policy), std::forward<A>(action)); }

  // Action-first form: 1+ tagged actions, at least one of which is on_overflow
  // (the only kind arithmetic itself fires; others are kept for forward-compat).
  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && detail::has_action<detail::IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto add(L const& lhs, R const& rhs, Actions&&... actions)
  { return detail::addition<L,R>::add(lhs, rhs,
      make_policy<detail::merged_implied_flags<Actions...>>(),
      detail::pick_action<detail::IsOverflowActionPred>(actions...)); }

  template <boundable L, boundable R, typename A = no_action>
  [[nodiscard]] constexpr auto add(L const& lhs, R const& rhs,
                                   errc& ec, A&& action = {})
  { return detail::addition<L,R>::add(lhs, rhs, make_policy<checked>(ec),
      std::forward<A>(action)); }

  //---------------------------------------------------------------------------
  // operator+
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator+(boundable auto lhs, boundable auto rhs)
  { return add(lhs, rhs); }

  // One overload covers all three optional shapes; the lambda's `l + r` re-enters
  // resolution on the unwrapped values, inheriting whichever bare overload applies.
  template <class L, class R>
    requires (detail::is_slim_optional_v<L> || detail::is_slim_optional_v<R>)
          && (!detail::expected_like<L> && !detail::expected_like<R>)
          && (boundable<detail::unwrap_t<L>> || boundable<detail::unwrap_t<R>>)
          && requires(detail::unwrap_t<L> l, detail::unwrap_t<R> r) { l + r; }
  constexpr auto operator+(L const& lhs, R const& rhs)
  { return lift([](auto const& l, auto const& r){ return l + r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // sub
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, detail::policy_like P = policy<>, typename A = no_action>
  [[nodiscard]] constexpr auto sub(L const& lhs, R const& rhs, P&& policy = {}, A&& action = {})
  { return add(lhs, -rhs, std::forward<P>(policy), std::forward<A>(action)); }

  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && detail::has_action<detail::IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto sub(L const& lhs, R const& rhs, Actions&&... actions)
  { return add(lhs, -rhs,
      make_policy<detail::merged_implied_flags<Actions...>>(),
      detail::pick_action<detail::IsOverflowActionPred>(actions...)); }

  template <boundable L, boundable R, typename A = no_action>
  [[nodiscard]] constexpr auto sub(L const& lhs, R const& rhs,
                                   errc& ec, A&& action = {})
  { return add(lhs, -rhs, make_policy<checked>(ec), std::forward<A>(action)); }

  //---------------------------------------------------------------------------
  // operator-
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator-(boundable auto lhs, boundable auto rhs)
  { return sub(lhs, rhs); }

  template <class L, class R>
    requires (detail::is_slim_optional_v<L> || detail::is_slim_optional_v<R>)
          && (!detail::expected_like<L> && !detail::expected_like<R>)
          && (boundable<detail::unwrap_t<L>> || boundable<detail::unwrap_t<R>>)
          && requires(detail::unwrap_t<L> l, detail::unwrap_t<R> r) { l - r; }
  constexpr auto operator-(L const& lhs, R const& rhs)
  { return lift([](auto const& l, auto const& r){ return l - r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // mul
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, detail::policy_like P = policy<>, typename A = no_action>
  [[nodiscard]] constexpr auto mul(L const& lhs, R const& rhs, P&& policy = {}, A&& action = {})
  { return detail::multiplication<L,R>::mul(lhs, rhs, std::forward<P>(policy), std::forward<A>(action)); }

  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && detail::has_action<detail::IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto mul(L const& lhs, R const& rhs, Actions&&... actions)
  { return detail::multiplication<L,R>::mul(lhs, rhs,
      make_policy<detail::merged_implied_flags<Actions...>>(),
      detail::pick_action<detail::IsOverflowActionPred>(actions...)); }

  template <boundable L, boundable R, typename A = no_action>
  [[nodiscard]] constexpr auto mul(L const& lhs, R const& rhs,
                                   errc& ec, A&& action = {})
  { return detail::multiplication<L,R>::mul(lhs, rhs, make_policy<checked>(ec),
      std::forward<A>(action)); }

  //---------------------------------------------------------------------------
  // operator*
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator*(boundable auto lhs, boundable auto rhs)
  { return bnd::mul(lhs, rhs); }

  template <class L, class R>
    requires (detail::is_slim_optional_v<L> || detail::is_slim_optional_v<R>)
          && (!detail::expected_like<L> && !detail::expected_like<R>)
          && (boundable<detail::unwrap_t<L>> || boundable<detail::unwrap_t<R>>)
          && requires(detail::unwrap_t<L> l, detail::unwrap_t<R> r) { l * r; }
  constexpr auto operator*(L const& lhs, R const& rhs)
  { return lift([](auto const& l, auto const& r){ return l * r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // add_all / mul_all — variadic folds (pairwise widening, same as `a + b + c`
  // but reads cleaner; matches Chromium's `CheckAdd(a, b, c)`).
  //---------------------------------------------------------------------------
  template <boundable First, boundable... Rest>
  [[nodiscard]] constexpr auto add_all(First const& first, Rest const&... rest)
  { return (first + ... + rest); }

  template <boundable First, boundable... Rest>
  [[nodiscard]] constexpr auto mul_all(First const& first, Rest const&... rest)
  { return (first * ... * rest); }

  // add_all_into<Target> / mul_all_into<Target> — fold, then collapse the widened
  // intermediate into Target via clamp_cast (widen for exactness, then clip).
  template <boundable Target, boundable First, boundable... Rest>
  [[nodiscard]] constexpr Target add_all_into(First const& first, Rest const&... rest)
  {
    auto sum = (first + ... + rest);
    if constexpr (requires { typename decltype(sum)::value_type; })
      return clamp_cast<Target>(sum.value());
    else
      return clamp_cast<Target>(sum);
  }

  template <boundable Target, boundable First, boundable... Rest>
  [[nodiscard]] constexpr Target mul_all_into(First const& first, Rest const&... rest)
  {
    auto prod = (first * ... * rest);
    if constexpr (requires { typename decltype(prod)::value_type; })
      return clamp_cast<Target>(prod.value());
    else
      return clamp_cast<Target>(prod);
  }

  //---------------------------------------------------------------------------
  // sum<Target> — bulk reduction with ONE deferred range check. Per-element
  // `target += b` re-validates every step (blocks vectorization); this
  // accumulates raws in imax and applies Target's policy once to the total
  // (semantic difference: the *total* is validated, not every prefix). Fast
  // path: ≤32-bit integer raws, flushed to a rational every 2^30 elements so the
  // accumulator can't overflow; wider/rational/real take the per-element fold.
  //---------------------------------------------------------------------------
  template <boundable Target, std::ranges::input_range Rng>
    requires boundable<std::remove_cvref_t<std::ranges::range_reference_t<Rng>>>
  [[nodiscard]] constexpr Target sum(Rng&& r)
  {
    using B = std::remove_cvref_t<std::ranges::range_reference_t<Rng>>;
    using bnd::detail::rational;
    rational total{0};

    if constexpr ((detail::value_raw<B> || detail::index_raw<B>)
                  && sizeof(detail::raw_t<B>) <= 4)
    {
      auto flush = [&](imax acc, imax cnt)
      {
        // value storage: raw IS the value. index: Σvalue = cnt·Lower + Σraw·Notch.
        rational part = [&]
        {
          if constexpr (detail::index_raw<B>)
            return ((rational{acc} * Notch<B>).value()
                    + (rational{cnt} * Lower<B>).value()).value();
          else
            return rational{acc};
        }();
        total = (total + part).value();
      };
      auto it  = std::ranges::begin(r);
      auto end = std::ranges::end(r);
      while (it != end)
      {
        // Branch-free inner block (≤ 2^30 elements keeps the imax accumulator
        // overflow-free) — the loop that vectorizes.
        imax acc = 0, cnt = 0;
        if constexpr (std::ranges::random_access_range<Rng>)
        {
          const imax block =
              std::min<imax>(end - it, imax{1} << 30);
          for (imax j = 0; j < block; ++j)
            acc += detail::raw_imax(it[j]);
          it += block;
          cnt = block;
        }
        else
        {
          for (; it != end && cnt < (imax{1} << 30); ++it, ++cnt)
            acc += detail::raw_imax(*it);
        }
        flush(acc, cnt);
      }
    }
    else
    {
      for (auto const& b : r)
        total = (total + detail::as_rational(b)).value();
    }
    return Target{total};
  }

  //---------------------------------------------------------------------------
  // dot / cross / lerp — 2-D bound-space vector helpers. Each widens its result
  // grid like the underlying `+`/`*`, so no overflow and the result is a plain
  // `bound`. (cross is the z-component, useful for "which side" tests.)
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto dot(boundable auto ax, boundable auto ay,
                                   boundable auto bx, boundable auto by)
  { return ax * bx + ay * by; }

  [[nodiscard]] constexpr auto cross(boundable auto ax, boundable auto ay,
                                     boundable auto bx, boundable auto by)
  { return ax * by - ay * bx; }

  // lerp(a, b, t) = a + (b - a) * t. `t` is itself a bound (typically a
  // [0, 1] fixed-point grid), so the interpolation never leaves bound-space.
  [[nodiscard]] constexpr auto lerp(boundable auto a, boundable auto b,
                                    boundable auto t)
  { return a + (b - a) * t; }

  //---------------------------------------------------------------------------
  // std-vocabulary helpers — ADL-found `min` / `max` / `midpoint` for generic
  // code. min/max mirror std; midpoint returns the *exact* average on a refined
  // grid (so, unlike std::midpoint, it neither rounds nor overflows). There is
  // no free `bnd::clamp` (the name is the policy flag — use clamp_cast<Target>).
  //---------------------------------------------------------------------------
  template <boundable T>
  [[nodiscard]] constexpr T min(T a, T b) { return (b < a) ? b : a; }

  template <boundable T>
  [[nodiscard]] constexpr T max(T a, T b) { return (a < b) ? b : a; }

  template <boundable T>
  [[nodiscard]] constexpr auto midpoint(T a, T b) { return (a + b) * just<frac<1, 2>>; }

  //---------------------------------------------------------------------------
  // div
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_flag F = none, typename A = no_action>
  [[nodiscard]] constexpr auto div(L lhs, R rhs, policy<F> pol = {}, A&& action = {})
  { return detail::division<L, R, F>::div(lhs, rhs, pol, std::forward<A>(action)); }

  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && detail::has_action<detail::IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto div(L lhs, R rhs, Actions&&... actions)
  { return detail::division<L, R, detail::merged_implied_flags<Actions...>>::div(lhs, rhs,
      make_policy<detail::merged_implied_flags<Actions...>>(),
      detail::pick_action<detail::IsOverflowActionPred>(actions...)); }

  template <boundable L, boundable R, typename A = no_action>
  [[nodiscard]] constexpr auto div(L lhs, R rhs,
                                   errc& ec, A&& action = {})
  { return detail::division<L, R, checked>::div(lhs, rhs, make_policy<checked>(ec),
      std::forward<A>(action)); }

  //---------------------------------------------------------------------------
  // operator/
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator/(boundable auto lhs, boundable auto rhs)
  {
    constexpr policy_flag F = BoundPolicy<decltype(lhs)> | BoundPolicy<decltype(rhs)>;
    return bnd::div(lhs, rhs, make_policy<F>());
  }

  template <class L, class R>
    requires (detail::is_slim_optional_v<L> || detail::is_slim_optional_v<R>)
          && (!detail::expected_like<L> && !detail::expected_like<R>)
          && (boundable<detail::unwrap_t<L>> || boundable<detail::unwrap_t<R>>)
          && requires(detail::unwrap_t<L> l, detail::unwrap_t<R> r) { l / r; }
  constexpr auto operator/(L const& lhs, R const& rhs)
  { return lift([](auto const& l, auto const& r){ return l / r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // mod
  //---------------------------------------------------------------------------
  template <boundable L, boundable R, policy_flag F = none, typename A = no_action>
  [[nodiscard]] constexpr auto mod(L lhs, R rhs, policy<F> pol = {}, A&& action = {})
  { return detail::modulo<L, R, F>::mod(lhs, rhs, pol, std::forward<A>(action)); }

  template <boundable L, boundable R, typename... Actions>
    requires (sizeof...(Actions) >= 1)
          && detail::has_action<detail::IsOverflowActionPred, std::remove_cvref_t<Actions>...>
  [[nodiscard]] constexpr auto mod(L lhs, R rhs, Actions&&... actions)
  { return detail::modulo<L, R, detail::merged_implied_flags<Actions...>>::mod(lhs, rhs,
      make_policy<detail::merged_implied_flags<Actions...>>(),
      detail::pick_action<detail::IsOverflowActionPred>(actions...)); }

  template <boundable L, boundable R, typename A = no_action>
  [[nodiscard]] constexpr auto mod(L lhs, R rhs,
                                   errc& ec, A&& action = {})
  { return detail::modulo<L, R, checked>::mod(lhs, rhs, make_policy<checked>(ec),
      std::forward<A>(action)); }

  //---------------------------------------------------------------------------
  // operator%
  //---------------------------------------------------------------------------
  [[nodiscard]] constexpr auto operator%(boundable auto lhs, boundable auto rhs)
  {
    constexpr policy_flag F = BoundPolicy<decltype(lhs)> | BoundPolicy<decltype(rhs)>;
    return bnd::mod(lhs, rhs, make_policy<F>());
  }

  template <class L, class R>
    requires (detail::is_slim_optional_v<L> || detail::is_slim_optional_v<R>)
          && (!detail::expected_like<L> && !detail::expected_like<R>)
          && (boundable<detail::unwrap_t<L>> || boundable<detail::unwrap_t<R>>)
          && requires(detail::unwrap_t<L> l, detail::unwrap_t<R> r) { l % r; }
  constexpr auto operator%(L const& lhs, R const& rhs)
  { return lift([](auto const& l, auto const& r){ return l % r; }, lhs, rhs); }

  //---------------------------------------------------------------------------
  // expected-lift operators — bridge bnd::math's expected results into chains, so
  // `math::tan(x) * gain + offset` stays an expected end to end (first error
  // short-circuits). An underlying nullopt maps to the operator's documented
  // cause: overflow for + − ×, division_by_zero for /. To drop the cause and
  // enter the optional world instead, convert with `bnd::ok(e)` (see lift.hpp).
  //---------------------------------------------------------------------------
  namespace detail
  {
    template <class L, class R>
    concept expected_operands =
        (expected_like<L> || expected_like<R>)
        && !is_slim_optional_v<L> && !is_slim_optional_v<R>
        && (boundable<expected_value_t<L>> || boundable<expected_value_t<R>>);

    // Mixing the two vocabularies in one expression is refused: the optional
    // operand's original cause is unknowable, so we won't invent one.
    template <class L, class R>
    concept mixed_error_operands =
        (expected_like<L> && is_slim_optional_v<R>)
        || (is_slim_optional_v<L> && expected_like<R>);
  }

  template <class L, class R>
    requires detail::expected_operands<L, R>
          && requires(detail::expected_value_t<L> l, detail::expected_value_t<R> r) { l + r; }
  constexpr auto operator+(L const& lhs, R const& rhs)
  { return lift_expected([](auto const& l, auto const& r){ return l + r; },
                         errc::overflow, lhs, rhs); }

  template <class L, class R>
    requires detail::expected_operands<L, R>
          && requires(detail::expected_value_t<L> l, detail::expected_value_t<R> r) { l - r; }
  constexpr auto operator-(L const& lhs, R const& rhs)
  { return lift_expected([](auto const& l, auto const& r){ return l - r; },
                         errc::overflow, lhs, rhs); }

  template <class L, class R>
    requires detail::expected_operands<L, R>
          && requires(detail::expected_value_t<L> l, detail::expected_value_t<R> r) { l * r; }
  constexpr auto operator*(L const& lhs, R const& rhs)
  { return lift_expected([](auto const& l, auto const& r){ return l * r; },
                         errc::overflow, lhs, rhs); }

  template <class L, class R>
    requires detail::expected_operands<L, R>
          && requires(detail::expected_value_t<L> l, detail::expected_value_t<R> r) { l / r; }
  constexpr auto operator/(L const& lhs, R const& rhs)
  { return lift_expected([](auto const& l, auto const& r){ return l / r; },
                         errc::division_by_zero, lhs, rhs); }

  template <class L, class R>
    requires detail::expected_operands<L, R>
          && requires(detail::expected_value_t<L> l, detail::expected_value_t<R> r) { l % r; }
  constexpr auto operator%(L const& lhs, R const& rhs)
  { return lift_expected([](auto const& l, auto const& r){ return l % r; },
                         errc::division_by_zero, lhs, rhs); }

  template <class L, class R>
    requires detail::mixed_error_operands<L, R>
  constexpr auto operator+(L const&, R const&)
  { static_assert(detail::dependent_false<L, R>,
      "bnd: don't mix expected and optional operands in one expression — "
      "convert the expected side with bnd::ok(e) (drops the error cause) "
      "or unwrap explicitly"); }

  template <class L, class R>
    requires detail::mixed_error_operands<L, R>
  constexpr auto operator-(L const&, R const&)
  { static_assert(detail::dependent_false<L, R>,
      "bnd: don't mix expected and optional operands in one expression — "
      "convert the expected side with bnd::ok(e) (drops the error cause) "
      "or unwrap explicitly"); }

  template <class L, class R>
    requires detail::mixed_error_operands<L, R>
  constexpr auto operator*(L const&, R const&)
  { static_assert(detail::dependent_false<L, R>,
      "bnd: don't mix expected and optional operands in one expression — "
      "convert the expected side with bnd::ok(e) (drops the error cause) "
      "or unwrap explicitly"); }

  template <class L, class R>
    requires detail::mixed_error_operands<L, R>
  constexpr auto operator/(L const&, R const&)
  { static_assert(detail::dependent_false<L, R>,
      "bnd: don't mix expected and optional operands in one expression — "
      "convert the expected side with bnd::ok(e) (drops the error cause) "
      "or unwrap explicitly"); }

  template <class L, class R>
    requires detail::mixed_error_operands<L, R>
  constexpr auto operator%(L const&, R const&)
  { static_assert(detail::dependent_false<L, R>,
      "bnd: don't mix expected and optional operands in one expression — "
      "convert the expected side with bnd::ok(e) (drops the error cause) "
      "or unwrap explicitly"); }

  //---------------------------------------------------------------------------
  // Grid-less scalar operands are rejected. A raw int/double carries no grid, so
  // `bound op rawscalar` has no type-safe result; rather than silently escape
  // into rational/double, these guidance overloads make it ill-formed with a fix
  // (give the literal a grid: `1_b` / `just<1>`, or a bound over its range).
  // Comparisons and compound assignment with raw scalars are unaffected.
  //
  // Concrete (non-auto) return type on purpose: keeps these SFINAE-transparent,
  // so `requires { b + 1; }` stays well-formed and the static_assert fires only
  // on a real call.
  //---------------------------------------------------------------------------
  template <typename A> concept raw_scalar = std::integral<A> || std::floating_point<A>;

  template <boundable B, raw_scalar A> B operator+(B const&, A) {
    static_assert(detail::dependent_false<B>,
      "a bound can only be added to another bound: give the scalar a grid — "
      "write `a + 1_b` (or `a + just<1>` / `a + one`), or `a + bound<{lo,hi}>{n}` "
      "for a runtime value with a known range"); }
  template <raw_scalar A, boundable B> B operator+(A, B const&) {
    static_assert(detail::dependent_false<B>,
      "a scalar can only be added to a bound that is itself a bound: write "
      "`1_b + a` / `just<1> + a` / `one + a`, or `bound<{lo,hi}>{n} + a`"); }

  template <boundable B, raw_scalar A> B operator-(B const&, A) {
    static_assert(detail::dependent_false<B>,
      "subtract a bound, not a raw scalar: write `a - 1_b` / `a - just<1>` / "
      "`a - one`, or `a - bound<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <raw_scalar A, boundable B> B operator-(A, B const&) {
    static_assert(detail::dependent_false<B>,
      "subtract from a bound, not a raw scalar: write `1_b - a` / `just<1> - "
      "a` / `one - a`, or `bound<{lo,hi}>{n} - a`"); }

  template <boundable B, raw_scalar A> B operator*(B const&, A) {
    static_assert(detail::dependent_false<B>,
      "multiply by a bound, not a raw scalar: write `a * 2_b` / `a * just<2>`, "
      "or `a * bound<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <raw_scalar A, boundable B> B operator*(A, B const&) {
    static_assert(detail::dependent_false<B>,
      "multiply a bound by a bound, not a raw scalar: write `2_b * a` / "
      "`just<2> * a`, or `bound<{lo,hi}>{n} * a`"); }

  template <boundable B, raw_scalar A> B operator/(B const&, A) {
    static_assert(detail::dependent_false<B>,
      "divide by a bound, not a raw scalar: write `a / 2_b` / `a / just<2>`, "
      "or `a / bound<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <raw_scalar A, boundable B> B operator/(A, B const&) {
    static_assert(detail::dependent_false<B>,
      "divide a bound by a bound, not a raw scalar: write `6_b / a` / "
      "`just<6> / a`, or `bound<{lo,hi}>{n} / a`"); }

  //---------------------------------------------------------------------------
  // Compound assignment with a raw scalar is ill-formed for the same reason —
  // a bound is mutated by another bound (or a rational), never a bare number.
  // These guidance overloads turn `b += 1` into a readable diagnostic instead
  // of a generic "no viable operator+=". Same SFINAE-transparent shape as the
  // binary operators above.
  //---------------------------------------------------------------------------
  template <boundable B, raw_scalar A> B& operator+=(B&, A) {
    static_assert(detail::dependent_false<B>,
      "add a bound, not a raw scalar: write `b += 1_b` / `b += just<1>`, or "
      "`b += bound<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <boundable B, raw_scalar A> B& operator-=(B&, A) {
    static_assert(detail::dependent_false<B>,
      "subtract a bound, not a raw scalar: write `b -= 1_b` / `b -= just<1>`, "
      "or `b -= bound<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <boundable B, raw_scalar A> B& operator*=(B&, A) {
    static_assert(detail::dependent_false<B>,
      "multiply by a bound, not a raw scalar: write `b *= 2_b` / `b *= just<2>`, "
      "or `b *= bound<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <boundable B, raw_scalar A> B& operator/=(B&, A) {
    static_assert(detail::dependent_false<B>,
      "divide by a bound, not a raw scalar: write `b /= 2_b` / `b /= just<2>`, "
      "or `b /= bound<{lo,hi}>{n}` for a runtime value with a known range"); }
  template <boundable B, raw_scalar A> B& operator%=(B&, A) {
    static_assert(detail::dependent_false<B>,
      "take the modulus by a bound, not a raw scalar: write `b %= 2_b` / "
      "`b %= just<2>`, or `b %= bound<{lo,hi}>{n}` for a runtime value"); }

} // namespace bnd


// ======================================================================
//  bound/range.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// bound_range — random-access range over a grid. Walks by notch index (any
// non-zero notch), each `*it` computing the exact `Lower + index·Notch`; the
// iterator wraps modulo the slot count so a mid-range start visits every slot
// once. Models random_access_range + sized_range (so std::ranges algorithms
// work directly). iterator_category is input_iterator_tag because operator*
// returns by value; iterator_concept carries the real random-access capability.
//---------------------------------------------------------------------------
namespace bnd
{
  namespace detail
  {
    // enumerate_view — C++20 stand-in for std::views::enumerate (C++23), yielding
    // pair<index, value> by value (all indexed() needs).
    template <class R>
    struct enumerate_view
    {
      R base_;

      struct iterator
      {
        std::ranges::iterator_t<const R> it{};
        std::size_t index{0};

        using value_type      = std::pair<std::size_t, std::ranges::range_value_t<R>>;
        using difference_type  = std::ptrdiff_t;

        constexpr value_type operator*() const { return {index, *it}; }
        constexpr iterator& operator++() { ++it; ++index; return *this; }
        constexpr iterator  operator++(int) { auto t = *this; ++*this; return t; }
        constexpr bool operator==(iterator const& o) const { return it == o.it; }
      };

      constexpr iterator begin() const { return {std::ranges::begin(base_), 0}; }
      constexpr iterator end()   const { return {std::ranges::end(base_), 0}; }
    };

    // stride_view — C++20 stand-in for std::views::stride (C++23). Visits every
    // `step`-th element; forward-only, and the advance checks `end` so a length
    // that isn't a multiple of the stride still terminates.
    template <class R>
    struct stride_view
    {
      R base_;
      std::size_t step_{1};

      struct iterator
      {
        std::ranges::iterator_t<const R> it{};
        std::ranges::iterator_t<const R> end{};
        std::size_t step{1};

        using value_type      = std::ranges::range_value_t<R>;
        using difference_type = std::ptrdiff_t;

        constexpr value_type operator*() const { return *it; }
        constexpr iterator& operator++()
        {
          for (std::size_t k = 0; k < step && it != end; ++k) ++it;
          return *this;
        }
        constexpr iterator operator++(int) { auto t = *this; ++*this; return t; }
        constexpr bool operator==(iterator const& o) const { return it == o.it; }
      };

      constexpr iterator begin() const
      { return {std::ranges::begin(base_), std::ranges::end(base_), step_}; }
      constexpr iterator end() const
      { return {std::ranges::end(base_), std::ranges::end(base_), step_}; }
    };
  } // namespace detail

  template <grid G, policy_flag P = checked>
    requires (G.Notch != 0)
  struct bound_range
  {
    using value_type = bound<G, P>;
    static constexpr umax slot_count = detail::NotchCount<value_type> + 1;

    struct iterator
    {
      using iterator_concept  = std::random_access_iterator_tag;
      using iterator_category = std::input_iterator_tag;
      using value_type        = bound<G, P>;
      using difference_type   = imax;

      umax index     {0};
      imax remaining {0};

      constexpr iterator() = default;
      constexpr iterator(umax i, imax r) : index{i}, remaining{r} {}

      constexpr value_type operator*() const
      {
        // value = Lower + index * Notch  (always exact: lies on the grid).
        bnd::detail::rational val = (G.Interval.Lower
                        + (bnd::detail::rational{index} * G.Notch).value()).value();
        return value_type{val};
      }

      constexpr value_type operator[](difference_type n) const
      { return *(*this + n); }

      // Advance / retreat: `remaining` is the position counter (advancing
      // decreases it), so the <=> below flips the comparison.
      constexpr iterator& operator++() { return *this += 1; }
      constexpr iterator  operator++(int) { auto t = *this; ++*this; return t; }
      constexpr iterator& operator--() { return *this -= 1; }
      constexpr iterator  operator--(int) { auto t = *this; --*this; return t; }

      constexpr iterator& operator+=(difference_type n)
      {
        constexpr imax M = slot_count;
        imax i = static_cast<imax>(index) + n;
        // euclidean mod so negative n still lands in [0, slot_count)
        i = ((i % M) + M) % M;
        index = i;
        remaining -= n;
        return *this;
      }
      constexpr iterator& operator-=(difference_type n) { return *this += -n; }

      constexpr iterator operator+(difference_type n) const { auto t = *this; t += n; return t; }
      constexpr iterator operator-(difference_type n) const { auto t = *this; t -= n; return t; }
      friend constexpr iterator operator+(difference_type n, iterator it) { return it + n; }

      constexpr difference_type operator-(iterator o) const
      { return o.remaining - remaining; }

      constexpr bool operator==(iterator o) const { return remaining == o.remaining; }
      constexpr auto operator<=>(iterator o) const { return o.remaining <=> remaining; }
    };

    umax start_index_;

    constexpr bound_range() : start_index_{0} {}

    constexpr bound_range(value_type start)
    {
      // Map a grid value back to its notch index: (start - Lower) / Notch.
      // The result has integer denominator (start is on the grid) so the
      // numerator is the index directly.
      auto offset = ((detail::as_rational(start) - G.Interval.Lower)
                     / G.Notch).value();
      start_index_ = offset.Numerator;
    }

    constexpr iterator begin() const
    { return {start_index_, static_cast<imax>(slot_count)}; }
    constexpr iterator end() const
    { return {start_index_, 0}; }

    constexpr std::size_t size() const { return slot_count; }

    // `indexed()` pairs each value with its zero-based position (≈ C++23
    // std::views::enumerate), via detail::enumerate_view for C++20.
    constexpr auto indexed() const { return detail::enumerate_view<bound_range>{*this}; }

    // `strided(step)` visits every `step`-th grid value (≈ C++23 std::views::
    // stride). `std::views::reverse` already works directly, so there's no reverse().
    constexpr auto strided(std::size_t step) const
    { return detail::stride_view<bound_range>{*this, step}; }
  };

} // namespace bnd



// ======================================================================
//  bound/cmath.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


// ======================================================================
//  bound/cmath_double.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// bnd::math double engine — a small, reproducible libm in `double`. Bit-identical
// on every IEEE-754 binary64 platform compiled without `-ffast-math`, via:
//   * NO <cmath> transcendentals — sin/cos/exp/log are fixed polynomials here;
//     only std::fma/sqrt/nearbyint (well-defined) and the constexpr ldexp.
//   * Horner evaluation with explicit std::fma (immune to FMA-contraction).
//   * Cody-Waite range reduction for full-precision args.
// The default engine; `BND_MATH_FIXED` selects the integer CORDIC engine instead.
//---------------------------------------------------------------------------


// BND_MATH_NO_FP — resolved here (the lowest math header) so both this file and
// cmath.hpp see it. When defined, the library uses NO hardware floating point and
// NO <cmath>: the double (FP) engine below compiles out and the always-present
// integer/CORDIC engine carries every transcendental. Define it (any value) to
// force the FP-free path; it is auto-enabled on freestanding targets
// (__STDC_HOSTED__ == 0) and whenever the integer engine is selected as the
// default (BND_MATH_FIXED). The integer engine is constexpr and bit-exact, so the
// public API and grid deduction are unchanged — only the compute backend differs.
#if !defined(BND_MATH_NO_FP)
#  if defined(BND_MATH_FIXED) || (defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 0)
#    define BND_MATH_NO_FP
#  endif
#endif

#ifndef BND_MATH_NO_FP   // ===== FP engine present (needs <cmath> + an FPU) =====

#include <cmath>            // std::fma, std::sqrt, std::nearbyint ONLY

// `BND_DBL_FN`: the engine cores become `constexpr` on C++26 toolchains with
// constexpr <cmath> (P1383). Inert otherwise — see BND_MATH_FN in cmath.hpp.
#if defined(__cpp_lib_constexpr_cmath) && __cpp_lib_constexpr_cmath >= 202202L
#  define BND_DBL_FN constexpr
#else
#  define BND_DBL_FN
#endif

namespace bnd::math::dbl::detail
{
  using std::fma;

  inline constexpr double kHalfPiHi = 0x1.921fb54442d18p+0;   // π/2  high
  inline constexpr double kHalfPiLo = 0x1.1a62633145c07p-54;  // π/2  low
  inline constexpr double kTwoOverPi = 0x1.45f306dc9c883p-1;  // 2/π
  inline constexpr double kLn2Hi    = 0x1.62e42fee00000p-1;   // ln2  high
  inline constexpr double kLn2Lo    = 0x1.a39ef35793c76p-33;  // ln2  low
  inline constexpr double kLog2e    = 0x1.71547652b82fep+0;   // 1/ln2

  // sin(r), r ∈ [−π/4, π/4]: r·P(r²), P = Σ (−1)ᵏ zᵏ/(2k+1)! to z⁷ (r¹⁵).
  inline BND_DBL_FN double sin_poly(double r)
  {
    double z = r * r;
    double p = -1.0 / 1307674368000.0;             // −1/15!
    p = fma(p, z,  1.0 / 6227020800.0);            //  1/13!
    p = fma(p, z, -1.0 / 39916800.0);              // −1/11!
    p = fma(p, z,  1.0 / 362880.0);                //  1/9!
    p = fma(p, z, -1.0 / 5040.0);                  // −1/7!
    p = fma(p, z,  1.0 / 120.0);                   //  1/5!
    p = fma(p, z, -1.0 / 6.0);                     // −1/3!
    p = fma(p, z,  1.0);                           //  1/1!
    return r * p;
  }

  // cos(r), r ∈ [−π/4, π/4]: Q(r²), Q = Σ (−1)ᵏ zᵏ/(2k)! to z⁸ (r¹⁶).
  inline BND_DBL_FN double cos_poly(double r)
  {
    double z = r * r;
    double p =  1.0 / 20922789888000.0;            //  1/16!
    p = fma(p, z, -1.0 / 87178291200.0);           // −1/14!
    p = fma(p, z,  1.0 / 479001600.0);             //  1/12!
    p = fma(p, z, -1.0 / 3628800.0);               // −1/10!
    p = fma(p, z,  1.0 / 40320.0);                 //  1/8!
    p = fma(p, z, -1.0 / 720.0);                   // −1/6!
    p = fma(p, z,  1.0 / 24.0);                    //  1/4!
    p = fma(p, z, -1.0 / 2.0);                     // −1/2!
    p = fma(p, z,  1.0);                           //  1
    return p;
  }

  // e^r, r ∈ [−ln2/2, ln2/2]: Σ rᵏ/k! to r¹².
  inline BND_DBL_FN double exp_poly(double r)
  {
    double p = 1.0 / 479001600.0;                  // 1/12!
    p = fma(p, r, 1.0 / 39916800.0);               // 1/11!
    p = fma(p, r, 1.0 / 3628800.0);                // 1/10!
    p = fma(p, r, 1.0 / 362880.0);                 // 1/9!
    p = fma(p, r, 1.0 / 40320.0);                  // 1/8!
    p = fma(p, r, 1.0 / 5040.0);                   // 1/7!
    p = fma(p, r, 1.0 / 720.0);                    // 1/6!
    p = fma(p, r, 1.0 / 120.0);                    // 1/5!
    p = fma(p, r, 1.0 / 24.0);                     // 1/4!
    p = fma(p, r, 1.0 / 6.0);                      // 1/3!
    p = fma(p, r, 1.0 / 2.0);                      // 1/2!
    p = fma(p, r, 1.0);                            // 1/1!
    p = fma(p, r, 1.0);                            // 1
    return p;
  }

  inline BND_DBL_FN double d_sin(double x)
  {
    double k = std::nearbyint(x * kTwoOverPi);
    double r = fma(-k, kHalfPiHi, x);
    r = fma(-k, kHalfPiLo, r);
    long q = static_cast<long>(k) & 3;
    switch (q) {
      case 0:  return sin_poly(r);
      case 1:  return cos_poly(r);
      case 2:  return -sin_poly(r);
      default: return -cos_poly(r);
    }
  }

  inline BND_DBL_FN double d_cos(double x) { return d_sin(x + (kHalfPiHi + kHalfPiLo)); }

  // e^x = 2^k · e^r, x = k·ln2 + r, r ∈ [−ln2/2, ln2/2].
  inline BND_DBL_FN double d_exp(double x)
  {
    double k = std::nearbyint(x * kLog2e);
    double r = fma(-k, kLn2Hi, x);
    r = fma(-k, kLn2Lo, r);
    return bnd::detail::ldexp(exp_poly(r), static_cast<int>(k));
  }

  inline BND_DBL_FN double d_sqrt(double x) { return std::sqrt(x); }   // correctly rounded

  inline constexpr double kSqrtHalf = 0x1.6a09e667f3bcdp-1; // √½

  // ln(x): frexp to m∈[½,1), rebalance to [√½,√2); ln(x) = e·ln2 + 2·atanh(f),
  // f = (m−1)/(m+1) ∈ [−0.18,0.18] (atanh series converges fast). Pre: x > 0.
  inline BND_DBL_FN double d_log(double x)
  {
    int e;
    double m = bnd::detail::frexp(x, &e);
    if (m < kSqrtHalf) { m += m; --e; }
    double f  = (m - 1.0) / (m + 1.0);
    double f2 = f * f;
    double p = 1.0 / 17.0;
    p = fma(p, f2, 1.0 / 15.0);
    p = fma(p, f2, 1.0 / 13.0);
    p = fma(p, f2, 1.0 / 11.0);
    p = fma(p, f2, 1.0 / 9.0);
    p = fma(p, f2, 1.0 / 7.0);
    p = fma(p, f2, 1.0 / 5.0);
    p = fma(p, f2, 1.0 / 3.0);
    p = fma(p, f2, 1.0);
    double logm = 2.0 * f * p;
    double r = fma(static_cast<double>(e), kLn2Hi, logm);
    return fma(static_cast<double>(e), kLn2Lo, r);
  }

  inline constexpr double kLn2Full  = 0x1.62e42fefa39efp-1;  // ln2
  inline constexpr double kLog10e   = 0x1.bcb7b1526e50ep-2;  // 1/ln10

  // Compositions on the validated primitives.
  inline BND_DBL_FN double d_exp2(double x)  { return d_exp(x * kLn2Full); }
  inline BND_DBL_FN double d_log2(double x)  { return d_log(x) * kLog2e; }
  inline BND_DBL_FN double d_log10(double x) { return d_log(x) * kLog10e; }
  inline BND_DBL_FN double d_pow(double b, double e) { return d_exp(e * d_log(b)); }
  inline BND_DBL_FN double d_cbrt(double x)
  {
    if (x == 0.0) return 0.0;
    double m = d_exp(d_log(x < 0 ? -x : x) * (1.0 / 3.0));
    return x < 0 ? -m : m;
  }
  inline BND_DBL_FN double d_sinh(double x) { double e = d_exp(x); return (e - 1.0 / e) * 0.5; }
  inline BND_DBL_FN double d_cosh(double x) { double e = d_exp(x); return (e + 1.0 / e) * 0.5; }
  inline BND_DBL_FN double d_tanh(double x)
  {
    double e = d_exp(x + x);            // e^{2x}
    return (e - 1.0) / (e + 1.0);
  }
  // √(x²+y²). The public domain caps |x|,|y| ≤ 2^20, so x²+y² ≤ 2^41 — no
  // overflow, no scaling needed; the correctly-rounded √ keeps it accurate.
  inline BND_DBL_FN double d_hypot(double x, double y) { return d_sqrt(x * x + y * y); }

  inline constexpr double kPi      = 0x1.921fb54442d18p+1;   // π
  inline constexpr double kPiHalf  = 0x1.921fb54442d18p+0;   // π/2
  inline constexpr double kPiSixth = 0x1.0c152382d7366p-1;   // π/6
  inline constexpr double kInvSqrt3 = 0x1.279a74590331cp-1;  // 1/√3 = tan(π/6)
  inline constexpr double kTanPi12 = 0x1.126145e9ecd56p-2;   // tan(π/12) ≈ 0.2679

  // atan(x). Reduce |x|>1 via reciprocal (π/2 − atan(1/x)); then |a|>tan(π/12)
  // via the π/6 addition formula → |t| ≤ tan(π/12); atan(t) = t·P(t²) Taylor.
  inline BND_DBL_FN double d_atan(double x)
  {
    bool neg = x < 0; double a = neg ? -x : x;
    bool inv = a > 1.0; if (inv) a = 1.0 / a;
    double off = 0.0;
    if (a > kTanPi12) { a = (a - kInvSqrt3) / fma(a, kInvSqrt3, 1.0); off = kPiSixth; }
    double z = a * a;
    double p = -1.0 / 23.0;
    p = fma(p, z,  1.0 / 21.0); p = fma(p, z, -1.0 / 19.0); p = fma(p, z,  1.0 / 17.0);
    p = fma(p, z, -1.0 / 15.0); p = fma(p, z,  1.0 / 13.0); p = fma(p, z, -1.0 / 11.0);
    p = fma(p, z,  1.0 / 9.0);  p = fma(p, z, -1.0 / 7.0);  p = fma(p, z,  1.0 / 5.0);
    p = fma(p, z, -1.0 / 3.0);  p = fma(p, z,  1.0);
    double r = off + a * p;
    if (inv) r = kPiHalf - r;
    return neg ? -r : r;
  }

  inline BND_DBL_FN double d_atan2(double y, double x)
  {
    if (x > 0.0) return d_atan(y / x);
    if (x < 0.0) return d_atan(y / x) + (y >= 0.0 ? kPi : -kPi);
    if (y > 0.0) return kPiHalf;
    if (y < 0.0) return -kPiHalf;
    return 0.0;
  }

  inline BND_DBL_FN double d_asin(double x) { return d_atan(x / d_sqrt((1.0 - x) * (1.0 + x))); }
  inline BND_DBL_FN double d_acos(double x) { return kPiHalf - d_asin(x); }
} // namespace bnd::math::dbl::detail

namespace bnd::math::dbl
{
  // Engine cores: `real` (double-backed) bound in → `double` math → bound out.
  // The bound I/O is a plain double read/store (operator double / Out{double}),
  // so the cost is the polynomial itself. These plug into the shared public
  // surface as `fn_core` under the default build.
  template <typename Out>
  [[nodiscard]] BND_DBL_FN Out store(double d)
  {
    // An fp-backed Out (f64 or f32) stores the value directly via its raw (an f32
    // Out narrows double→float, lossless on its float-exact grid); a non-fp snap
    // grid assigns through the rational path, snapping via Out's round policy.
    if constexpr (bnd::detail::fp_raw<Out>) return Out{d};
    else { Out o{}; o = bnd::detail::rational{d}; return o; }
  }

  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out sin_core(In x)  { return store<Out>(detail::d_sin(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out cos_core(In x)  { return store<Out>(detail::d_cos(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out exp_core(In x)  { return store<Out>(detail::d_exp(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out sqrt_core(In x) { return store<Out>(detail::d_sqrt(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out log_core(In x)  { return store<Out>(detail::d_log(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out exp2_core(In x) { return store<Out>(detail::d_exp2(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out log2_core(In x) { return store<Out>(detail::d_log2(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out log10_core(In x){ return store<Out>(detail::d_log10(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out cbrt_core(In x) { return store<Out>(detail::d_cbrt(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out sinh_core(In x) { return store<Out>(detail::d_sinh(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out cosh_core(In x) { return store<Out>(detail::d_cosh(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out tanh_core(In x) { return store<Out>(detail::d_tanh(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out atan_core(In x) { return store<Out>(detail::d_atan(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out asin_core(In x) { return store<Out>(detail::d_asin(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out acos_core(In x) { return store<Out>(detail::d_acos(static_cast<double>(x))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out atan2_core(In y, In x)
  { return store<Out>(detail::d_atan2(static_cast<double>(y), static_cast<double>(x))); }
  template <typename Out, typename InX, typename InY>
  [[nodiscard]] BND_DBL_FN Out hypot_core(InX x, InY y)
  { return store<Out>(detail::d_hypot(static_cast<double>(x), static_cast<double>(y))); }
} // namespace bnd::math::dbl

#endif // !BND_MATH_NO_FP


// ======================================================================
//  bound/cmath_float.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// bnd::math float engine — a small, reproducible libm in `float` (binary32).
// Bit-identical on every IEEE-754 binary32 platform compiled without
// `-ffast-math`, via the same recipe as the double engine but in single
// precision, for single-precision-only FPUs (Cortex-M4F etc.) and size/speed:
//   * NO <cmath> transcendentals — own fixed polynomials evaluated with the
//     correctly-rounded std::fma(float) (immune to FMA-contraction).
//   * Cody-Waite range reduction whose hi/lo split constants are derived at
//     compile time (mask the low mantissa bits of the float-rounded constant),
//     no external codegen — honoring the bit-exact contract.
//   * Correctly-rounded std::sqrt(float).
// Float is a THIRD value set: float ≠ double ≠ cordic, each ≤ a few notches of
// truth where the grid permits (table-maker's dilemma — see determinism.md).
// Present only when FP is available; compiled out under BND_MATH_NO_FP.
//---------------------------------------------------------------------------


#ifndef BND_MATH_NO_FP

#include <bit>     // std::bit_cast (constexpr Cody-Waite split derivation)
#include <cmath>   // std::fma, std::sqrt, std::nearbyint, std::ldexp, std::frexp (float)

namespace bnd::math::flt::detail
{
  using std::fma;

  // Constexpr Cody-Waite split of a high-precision (double) reference into a
  // float `hi` with `keep` significant mantissa bits (low bits zeroed, so k·hi
  // stays exact for modest k) plus a float `lo` carrying the remainder. Pure
  // bit manipulation — identical on every IEEE-754 target.
  inline constexpr float split_hi(double full, int keep) noexcept
  {
    float f = static_cast<float>(full);
    std::uint32_t b = std::bit_cast<std::uint32_t>(f);
    b &= ~((std::uint32_t{1} << (23 - keep)) - 1);
    return std::bit_cast<float>(b);
  }
  inline constexpr float split_lo(double full, int keep) noexcept
  {
    return static_cast<float>(full - static_cast<double>(split_hi(full, keep)));
  }

  // High-precision references (double literals; only their float projections are
  // used at runtime). 12 kept bits hold accuracy across the shared ±2^20 domain.
  inline constexpr double kPiD    = 3.14159265358979323846;
  inline constexpr double kPiO2D  = kPiD / 2;
  inline constexpr double kLn2D   = 0.69314718055994530942;

  inline constexpr float kPio2Hi    = split_hi(kPiO2D, 12);
  inline constexpr float kPio2Lo    = split_lo(kPiO2D, 12);
  inline constexpr float kTwoOverPi  = static_cast<float>(2.0 / kPiD);
  inline constexpr float kLn2Hi     = split_hi(kLn2D, 12);
  inline constexpr float kLn2Lo     = split_lo(kLn2D, 12);
  inline constexpr float kLn2Full   = static_cast<float>(kLn2D);
  inline constexpr float kLog2e     = static_cast<float>(1.44269504088896340736);
  inline constexpr float kLog10e    = static_cast<float>(0.43429448190325182765);
  inline constexpr float kSqrtHalf  = 0x1.6a09e6p-1f;             // √½
  inline constexpr float kPi        = static_cast<float>(kPiD);
  inline constexpr float kPiHalf    = static_cast<float>(kPiO2D);
  inline constexpr float kPiSixth   = static_cast<float>(kPiD / 6);
  inline constexpr float kInvSqrt3  = 0x1.279a74p-1f;             // 1/√3 = tan(π/6)
  inline constexpr float kTanPi12   = 0x1.126146p-2f;             // tan(π/12)

  // sin(r), r ∈ [−π/4, π/4]: r·P(r²) to r¹¹ (float-sufficient).
  inline BND_DBL_FN float sin_poly(float r)
  {
    float z = r * r;
    float p = -1.0f / 39916800.0f;       // −1/11!
    p = fma(p, z,  1.0f / 362880.0f);    //  1/9!
    p = fma(p, z, -1.0f / 5040.0f);      // −1/7!
    p = fma(p, z,  1.0f / 120.0f);       //  1/5!
    p = fma(p, z, -1.0f / 6.0f);         // −1/3!
    p = fma(p, z,  1.0f);                //  1
    return r * p;
  }

  // cos(r), r ∈ [−π/4, π/4]: Q(r²) to r¹⁰.
  inline BND_DBL_FN float cos_poly(float r)
  {
    float z = r * r;
    float p = -1.0f / 3628800.0f;        // −1/10!
    p = fma(p, z,  1.0f / 40320.0f);     //  1/8!
    p = fma(p, z, -1.0f / 720.0f);       // −1/6!
    p = fma(p, z,  1.0f / 24.0f);        //  1/4!
    p = fma(p, z, -1.0f / 2.0f);         // −1/2!
    p = fma(p, z,  1.0f);                //  1
    return p;
  }

  // e^r, r ∈ [−ln2/2, ln2/2]: Σ rᵏ/k! to r⁷.
  inline BND_DBL_FN float exp_poly(float r)
  {
    float p = 1.0f / 5040.0f;            // 1/7!
    p = fma(p, r, 1.0f / 720.0f);        // 1/6!
    p = fma(p, r, 1.0f / 120.0f);        // 1/5!
    p = fma(p, r, 1.0f / 24.0f);         // 1/4!
    p = fma(p, r, 1.0f / 6.0f);          // 1/3!
    p = fma(p, r, 1.0f / 2.0f);          // 1/2!
    p = fma(p, r, 1.0f);                 // 1/1!
    p = fma(p, r, 1.0f);                 // 1
    return p;
  }

  // Shared quadrant reduction: x → (r ∈ [−π/4,π/4], q = quadrant mod 4).
  inline BND_DBL_FN float reduce_quadrant(float x, long& q)
  {
    float k = std::nearbyint(x * kTwoOverPi);
    float r = fma(-k, kPio2Hi, x);
    r = fma(-k, kPio2Lo, r);
    q = static_cast<long>(k) & 3;
    return r;
  }

  inline BND_DBL_FN float d_sin(float x)
  {
    long q; float r = reduce_quadrant(x, q);
    switch (q) {
      case 0:  return sin_poly(r);
      case 1:  return cos_poly(r);
      case 2:  return -sin_poly(r);
      default: return -cos_poly(r);
    }
  }

  // cos via quadrant reduction (NOT sin(x+π/2): shifting the float input would
  // lose bits for large x — the double engine can afford that, float cannot).
  inline BND_DBL_FN float d_cos(float x)
  {
    long q; float r = reduce_quadrant(x, q);
    switch (q) {
      case 0:  return cos_poly(r);
      case 1:  return -sin_poly(r);
      case 2:  return -cos_poly(r);
      default: return sin_poly(r);
    }
  }

  // e^x = 2^k · e^r, x = k·ln2 + r, r ∈ [−ln2/2, ln2/2].
  inline BND_DBL_FN float d_exp(float x)
  {
    float k = std::nearbyint(x * kLog2e);
    float r = fma(-k, kLn2Hi, x);
    r = fma(-k, kLn2Lo, r);
    return std::ldexp(exp_poly(r), static_cast<int>(k));
  }

  inline BND_DBL_FN float d_sqrt(float x) { return std::sqrt(x); }   // correctly rounded

  // ln(x): frexp to m∈[½,1), rebalance to [√½,√2); ln = e·ln2 + 2·atanh(f),
  // f = (m−1)/(m+1). Pre: x > 0.
  inline BND_DBL_FN float d_log(float x)
  {
    int e;
    float m = std::frexp(x, &e);
    if (m < kSqrtHalf) { m += m; --e; }
    float f  = (m - 1.0f) / (m + 1.0f);
    float f2 = f * f;
    float p = 1.0f / 9.0f;
    p = fma(p, f2, 1.0f / 7.0f);
    p = fma(p, f2, 1.0f / 5.0f);
    p = fma(p, f2, 1.0f / 3.0f);
    p = fma(p, f2, 1.0f);
    float logm = 2.0f * f * p;
    float r = fma(static_cast<float>(e), kLn2Hi, logm);
    return fma(static_cast<float>(e), kLn2Lo, r);
  }

  // Compositions on the validated primitives (same shapes as the double engine).
  inline BND_DBL_FN float d_exp2(float x)  { return d_exp(x * kLn2Full); }
  inline BND_DBL_FN float d_log2(float x)  { return d_log(x) * kLog2e; }
  inline BND_DBL_FN float d_log10(float x) { return d_log(x) * kLog10e; }
  inline BND_DBL_FN float d_pow(float b, float e) { return d_exp(e * d_log(b)); }
  inline BND_DBL_FN float d_cbrt(float x)
  {
    if (x == 0.0f) return 0.0f;
    float m = d_exp(d_log(x < 0 ? -x : x) * (1.0f / 3.0f));
    return x < 0 ? -m : m;
  }
  inline BND_DBL_FN float d_sinh(float x) { float e = d_exp(x); return (e - 1.0f / e) * 0.5f; }
  inline BND_DBL_FN float d_cosh(float x) { float e = d_exp(x); return (e + 1.0f / e) * 0.5f; }
  inline BND_DBL_FN float d_tanh(float x)
  {
    float e = d_exp(x + x);             // e^{2x}
    return (e - 1.0f) / (e + 1.0f);
  }
  inline BND_DBL_FN float d_hypot(float x, float y) { return d_sqrt(x * x + y * y); }

  // atan(x): reduce |x|>1 via reciprocal; |a|>tan(π/12) via the π/6 addition
  // formula → |t| ≤ tan(π/12); atan(t) = t·P(t²) Taylor.
  inline BND_DBL_FN float d_atan(float x)
  {
    bool neg = x < 0; float a = neg ? -x : x;
    bool inv = a > 1.0f; if (inv) a = 1.0f / a;
    float off = 0.0f;
    if (a > kTanPi12) { a = (a - kInvSqrt3) / fma(a, kInvSqrt3, 1.0f); off = kPiSixth; }
    float z = a * a;
    float p = 1.0f / 13.0f;
    p = fma(p, z, -1.0f / 11.0f); p = fma(p, z,  1.0f / 9.0f); p = fma(p, z, -1.0f / 7.0f);
    p = fma(p, z,  1.0f / 5.0f);  p = fma(p, z, -1.0f / 3.0f); p = fma(p, z,  1.0f);
    float r = off + a * p;
    if (inv) r = kPiHalf - r;
    return neg ? -r : r;
  }

  inline BND_DBL_FN float d_atan2(float y, float x)
  {
    if (x > 0.0f) return d_atan(y / x);
    if (x < 0.0f) return d_atan(y / x) + (y >= 0.0f ? kPi : -kPi);
    if (y > 0.0f) return kPiHalf;
    if (y < 0.0f) return -kPiHalf;
    return 0.0f;
  }

  inline BND_DBL_FN float d_asin(float x) { return d_atan(x / d_sqrt((1.0f - x) * (1.0f + x))); }
  inline BND_DBL_FN float d_acos(float x) { return kPiHalf - d_asin(x); }
} // namespace bnd::math::flt::detail

namespace bnd::math::flt
{
  // Engine cores: bound in → `float` math → bound out. Storing the float result:
  // an fp-backed Out (f32 OR f64) stores the value directly via its float/double
  // raw (the natural pairing for `flt` is `f32` — no rational, no double round-
  // trip on the result); any other snap grid assigns through the rational path,
  // snapping via Out's round policy.
  template <typename Out>
  [[nodiscard]] BND_DBL_FN Out store(float f)
  {
    if constexpr (bnd::detail::fp_raw<Out>) return Out{static_cast<double>(f)};
    else { Out o{}; o = bnd::detail::rational{static_cast<double>(f)}; return o; }
  }

  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out sin_core(In x)  { return store<Out>(detail::d_sin(static_cast<float>(static_cast<double>(x)))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out cos_core(In x)  { return store<Out>(detail::d_cos(static_cast<float>(static_cast<double>(x)))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out exp_core(In x)  { return store<Out>(detail::d_exp(static_cast<float>(static_cast<double>(x)))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out sqrt_core(In x) { return store<Out>(detail::d_sqrt(static_cast<float>(static_cast<double>(x)))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out log_core(In x)  { return store<Out>(detail::d_log(static_cast<float>(static_cast<double>(x)))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out exp2_core(In x) { return store<Out>(detail::d_exp2(static_cast<float>(static_cast<double>(x)))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out log2_core(In x) { return store<Out>(detail::d_log2(static_cast<float>(static_cast<double>(x)))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out log10_core(In x){ return store<Out>(detail::d_log10(static_cast<float>(static_cast<double>(x)))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out cbrt_core(In x) { return store<Out>(detail::d_cbrt(static_cast<float>(static_cast<double>(x)))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out sinh_core(In x) { return store<Out>(detail::d_sinh(static_cast<float>(static_cast<double>(x)))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out cosh_core(In x) { return store<Out>(detail::d_cosh(static_cast<float>(static_cast<double>(x)))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out tanh_core(In x) { return store<Out>(detail::d_tanh(static_cast<float>(static_cast<double>(x)))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out atan_core(In x) { return store<Out>(detail::d_atan(static_cast<float>(static_cast<double>(x)))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out asin_core(In x) { return store<Out>(detail::d_asin(static_cast<float>(static_cast<double>(x)))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out acos_core(In x) { return store<Out>(detail::d_acos(static_cast<float>(static_cast<double>(x)))); }
  template <typename Out, typename In>
  [[nodiscard]] BND_DBL_FN Out atan2_core(In y, In x)
  { return store<Out>(detail::d_atan2(static_cast<float>(static_cast<double>(y)), static_cast<float>(static_cast<double>(x)))); }
  template <typename Out, typename InX, typename InY>
  [[nodiscard]] BND_DBL_FN Out hypot_core(InX x, InY y)
  { return store<Out>(detail::d_hypot(static_cast<float>(static_cast<double>(x)), static_cast<float>(static_cast<double>(y)))); }
} // namespace bnd::math::flt

#endif // !BND_MATH_NO_FP




// The public bnd::math::* functions dispatch to the double engine (default) or
// the integer/CORDIC engine (`-DBOUND_MATH_FIXED`). The integer engine is
// always `constexpr`; the double engine becomes `constexpr` automatically on
// C++26 toolchains where <cmath> is constexpr (P1383 — std::fma / std::sqrt /
// std::nearbyint; feature macro __cpp_lib_constexpr_cmath). That branch is
// inert (and untested) until such a toolchain exists. Decision 2026-06-12:
// no compile-time softfloat emulation — wait for the standard.
// BND_MATH_NO_FP (resolved in cmath_double.hpp, included above) selects the
// integer/CORDIC engine and is implied by BND_MATH_FIXED — so the integer engine
// is constexpr here. The double engine becomes constexpr only on a C++26 toolchain
// with constexpr <cmath> (P1383); that branch is inert until such a toolchain.
#if defined(BND_MATH_NO_FP) \
    || (defined(__cpp_lib_constexpr_cmath) && __cpp_lib_constexpr_cmath >= 202202L)
#  define BND_MATH_FN constexpr
#else
#  define BND_MATH_FN
#endif

//---------------------------------------------------------------------------
// bnd::math — one transcendental API, two interchangeable engines selected by
// the `BND_MATH_FIXED` macro. Both are feature-equivalent (same functions,
// signatures, domains):
//
//   * DEFAULT — double engine (`cmath_double.hpp`): hardware `double`
//     polynomials on `real` bounds. Bit-identical on any IEEE-754 binary64
//     platform built without `-ffast-math`. Fast (~ns); needs an FPU; runtime.
//   * `BND_MATH_FIXED` — integer/CORDIC engine (this file): FPU-free, constexpr,
//     UNCONDITIONALLY bit-identical (any platform/flags). For embedded/portability.
//   * `BND_MATH_FLOAT` — float (binary32) engine (`cmath_float.hpp`): like the
//     double engine but single precision, for single-precision-only FPUs.
//
// The macro picks only which engine the UNQUALIFIED `bnd::math::fn` uses; all
// engines are always reachable by namespace (`cordic::`/`dbl::`/`flt::`). Default
// selection (the dispatch below): `BND_MATH_NO_FP`→cordic, else
// `BND_MATH_FLOAT`→flt, else dbl.
//
// `BND_MATH_NO_FP` (implied by `BND_MATH_FIXED`, auto-enabled when
// `__STDC_HOSTED__ == 0`) compiles the double AND float engines and their
// `<cmath>` out entirely, leaving the integer engine — so the library, including
// the single header, builds with no hardware floating point.
//
// =====================================================================
//   INTEGER (CORDIC) ENGINE — BIT-EXACT REPRODUCIBILITY CONTRACT
// =====================================================================
// Every function below produces bit-identical output for the same input across
// compiler, platform, optimisation level, and FP flags — relied on for fuzzing
// corpora, record-and-replay, deterministic simulation, regression testing.
// (The double engine's contract lives atop cmath_double.hpp.) Requirements:
//   1. NO `<cmath>`/FPU/intrinsics; hot paths are integer-only over int64.
//   2. NO runtime-derived tables — coefficients derived at compile time from
//      `rational` literals, quantized to integer Q-format via constexpr rounding.
//   3. NO external code generators; derivation is constexpr C++.
//   4. C++20+ (well-defined signed right-shift semantics).
//   5. Each transcendental ships checked-in `static_assert` vectors pinning its
//      bit-exact output.
//
// Pattern per function: pick a working scale 2^W from the output grid
// (`working_bits`), range-reduce with integer ops, evaluate via the shared
// shift-add CORDIC (or Newton for sqrt/cbrt) using the portable wide `fmul`,
// then quantize onto `Out`'s grid through its assignment policy.
//---------------------------------------------------------------------------
namespace bnd::math
{
  namespace detail
  {
    using namespace bnd::detail;

    // Exact rational source for the irrational constants — the fixed-point cores
    // need the exact form; bit-identical across platforms.
    inline constexpr rational pi_r{1068966896, 340262731};
    inline constexpr rational two_pi_r = 2 * pi_r;

    // Every transcendental operand must carry the `real` policy flag: under the
    // default engine it selects double-backed dyadic storage, under BND_MATH_FIXED
    // integer round_nearest. Requiring it keeps both engines' call sites identical
    // and avoids the slow integer-I/O path. Pure grid ops (abs/floor/ceil/round/
    // trunc/fmod) have no engine and don't require it.
    template <boundable In>
    consteval bool require_snap() noexcept
    {
      static_assert(has_flag(BoundPolicy<In>, snap),
          "bnd::math: a transcendental result is rounded onto the grid — its "
          "operand must permit rounding. Declare it with `round_nearest` (or "
          "`snap` / a `round_*` mode / `real`).");
      return true;
    }
  }

  // Public irrational constants as POINT-BOUNDS, so they compose directly in
  // bound-space (`angle * math::pi`) with no rational on the surface.
  inline constexpr auto pi     = just<detail::pi_r>;
  inline constexpr auto two_pi = just<detail::two_pi_r>;

  namespace detail
  {
    using namespace bnd::detail;

    // Internal turn-phase shape: Q.N turns, period implicit in the unsigned raw's
    // modular wrap. Public sin/cos/tan take radians and route through this shape;
    // callers don't construct it directly (see examples/oscillator.cpp).
    template <int N>
    using turns_t = bound<{0, rational{(imax{1} << N) - 1, imax{1} << N},
                           notch<1, (imax{1} << N)>}>;


    // log2(d) for a power-of-2 imax d. Constexpr loop; cheap at compile time.
    constexpr int log2_pow2(imax d) noexcept
    {
      int n = 0;
      while (d > 1) { d >>= 1; ++n; }
      return n;
    }

    // Extract N from a turns-shaped input bound (notch denominator is 2^N).
    // Alias templates can't be reverse-deduced, so sin/cos take a `boundable In`
    // and derive N from its grid.
    template <boundable In>
    inline constexpr int turn_bits = []{
      static_assert(Lower<In> == 0,
                    "bnd::math: turn-phase input must have Lower == 0");
      static_assert(Notch<In>.Numerator == 1,
                    "bnd::math: turn-phase input must have notch 1/2^N");
      return log2_pow2(abs_den(Notch<In>.Denominator));
    }();

    // Forward declarations of the CORDIC engine pieces the turn-input workers
    // rely on (the engine is defined below, after the radians sin/cos).
    template <boundable Out> constexpr int working_bits() noexcept;
    template <int W, int N> constexpr rational sin_from_turn_fixed(imax turn_w) noexcept;
    template <int W, int N> constexpr rational cos_from_turn_fixed(imax turn_w) noexcept;
    template <boundable Out> constexpr Out store_grid(rational r) noexcept;

    // sin (turn-input, internal). Q.N turn-phase → amplitude on `Out`'s grid via
    // the CORDIC engine: rescale the phase to the working scale 2^W and run the
    // shared `sin_from_turn_fixed` reducer.
    template <boundable Out, boundable In>
    [[nodiscard]] constexpr Out sin_turn_impl(In phase) noexcept
    {
      constexpr int N = turn_bits<In>;
      static_assert(N >= 2 && N <= 30, "bnd::math: turn-phase N must be in [2, 30]");
      static_assert(Lower<Out> <= -1 && Upper<Out> >= 1,
                    "bnd::math: Out must cover [-1, 1]");

      constexpr int W = working_bits<Out>();
      imax raw    = raw_imax(phase);                       // Q.N turn
      imax turn_w = (W >= N) ? (raw << (W - N)) : (raw >> (N - W));       // → Q.W
      return store_grid<Out>(sin_from_turn_fixed<W, W>(turn_w));
    }

    // cos (turn-input, internal). cos(x) = sin(x + π/2) — shift the phase
    // by one quarter-turn (modular wrap on the raw) and reuse sin. The
    // shift is integer-exact, no precision cost at this tier.
    template <boundable Out, boundable In>
    [[nodiscard]] constexpr Out cos_turn_impl(In phase) noexcept
    {
      constexpr int  N            = turn_bits<In>;
      constexpr imax full_mask    = (imax{1} << N) - 1;
      constexpr imax quarter_turn = imax{1} << (N - 2);

      In shifted = In::from_raw(raw_cast<In>(
          (raw_imax(phase) + quarter_turn) & full_mask));
      return sin_turn_impl<Out>(shifted);
    }

    //=========================================================================
    // Grid-scaled CORDIC engine. Values cross the API as `rational`; internally
    // we work in fixed-point at a scale 2^W chosen from the output grid
    // (`working_bits`), so precision follows the grid. The iteration is pure
    // shift-add (overflow-free); the only multiplies (table/gain derivation,
    // input scaling) use the wide `fmul`, bit-identical on every toolchain.
    //=========================================================================

    // (a·b) >> W via the full 128-bit product, magnitude-truncating (toward zero).
    // The two paths below are bit-for-bit equal by construction.
    constexpr imax fmul(imax a, imax b, int W) noexcept
    {
      bool neg = (a < 0) ^ (b < 0);
      umax ua = (a < 0) ? umax{0} - static_cast<umax>(a) : static_cast<umax>(a);
      umax ub = (b < 0) ? umax{0} - static_cast<umax>(b) : static_cast<umax>(b);
#if defined(__SIZEOF_INT128__)
      // gcc/clang: native 128-bit, constexpr-friendly.
      umax r = static_cast<umax>((static_cast<unsigned __int128>(ua) * ub) >> W);
#else
      // portable: 32-bit split → 128-bit (hi:lo) → shift. Also the MSVC path.
      umax al = ua & 0xffffffffu, ah = ua >> 32;
      umax bl = ub & 0xffffffffu, bh = ub >> 32;
      umax ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;
      umax mid = (ll >> 32) + (lh & 0xffffffffu) + (hl & 0xffffffffu);
      umax lo  = (ll & 0xffffffffu) | (mid << 32);
      umax hi  = hh + (lh >> 32) + (hl >> 32) + (mid >> 32);
      umax r   = (W == 0) ? lo
               : (W < 64) ? ((lo >> W) | (hi << (64 - W)))
               :            (hi >> (W - 64));
#endif
      return neg ? -static_cast<imax>(r) : static_cast<imax>(r);
    }

    // round(v · 2^W) — scale-W marshalling (parametric Q.W). The product num·2^W
    // is formed at 128 bits, so a reduced numerator near 2^63 cannot wrap.
    // Rounding is half-away-from-zero, matching rational::round().
    constexpr imax to_fixed(rational v, int W) noexcept
    {
      const umax n = v.Numerator;
      const umax d = abs_den(v.Denominator);
#if defined(__SIZEOF_INT128__)
      using u128 = unsigned __int128;
      const u128 t = (u128{n} << W) + d / 2;
      const umax q = static_cast<umax>(t / d);
#else
      // portable: 128-bit (hi:lo) dividend, restoring shift-subtract divide.
      // d < 2^63 (imax denominator), so the partial remainder fits umax.
      umax hi = (W == 0) ? 0 : (n >> (64 - W));
      umax lo = n << W;
      const umax half = d / 2;
      lo += half;
      hi += (lo < half);
      umax q = 0, r = 0;
      for (int i = 127; i >= 0; --i)
      {
        r = (r << 1) | ((i >= 64 ? (hi >> (i - 64)) : (lo >> i)) & 1u);
        q <<= 1;
        if (r >= d) { r -= d; q |= 1; }
      }
#endif
      return (v.Denominator < 0) ? -static_cast<imax>(q) : static_cast<imax>(q);
    }
    constexpr rational fixed_to_rational(imax x, int W) noexcept
    { return rational{x, imax{1} << W}; }

    // Grids eligible for the GCD-free store: unit-numerator notch, non-rational
    // storage, raw fits imax, assigned with round_nearest. (Unlike the Q-format
    // fast path this does NOT require integer Lower — Lower·K is an exact
    // integer by the grid invariant regardless.) The math results all carry a
    // power-of-two denominator, so the offset index is formed with integer
    // shifts + round-half-up — identical to the rational assignment path
    // (round_quotient round_nearest is `(num+den/2)/den`, invariant under
    // fraction reduction), but skipping `(value−Lower)/Notch`'s GCD reductions.
    template <boundable Out>
    inline constexpr bool grid_fast_store =
        Notch<Out>.Numerator == 1
        && !rational_raw<Out>
        // `real` storage holds the VALUE, not an offset index, so route it
        // through the rational fallback `Out{r}` (same guard as fmod_int_fast).
        && !fp_raw<Out>
        && has_flag(BoundPolicy<Out>, round_nearest)
        && (std::signed_integral<raw_t<Out>>
            || NotchCount<Out>
                 <= static_cast<umax>(std::numeric_limits<imax>::max()));

    // Store a power-of-two-denominator result (the shape every core returns)
    // onto Out's grid. Fast path: pure integer. Fallback: the general rational
    // assignment (handles non-fast grids, clamp/wrap on out-of-range, etc).
    template <boundable Out>
    constexpr Out store_grid(rational r) noexcept
    {
      if constexpr (grid_fast_store<Out>)
      {
        umax den = abs_den(r.Denominator);
        if ((den & (den - 1)) == 0)                       // power-of-two denom
        {
          int  D   = std::countr_zero(den);
          imax num = (r.Denominator < 0) ? -r.Numerator
                                         :  r.Numerator;
          constexpr imax K = abs_den(Notch<Out>.Denominator);  // 1/notch
          constexpr imax m = trunc((Lower<Out> * rational{K}).value()); // Lower·K (exact int)
          // K·num + half must fit imax (a wide-denominator r, e.g. hypot's
          // 2^46, would wrap K·num and silently store `value mod 2^k`).
          constexpr imax lim = std::numeric_limits<imax>::max() / 2 / K;
          if (-lim <= num && num <= lim)
          {
            imax half = (D > 0) ? (imax{1} << (D - 1)) : 0;
            imax off  = ((K * num + half) >> D) - m;      // round-half-up((value−Lower)·K)
            if (off >= 0 && static_cast<umax>(off) <= NotchCount<Out>)
              return Out::from_raw(raw_from_offset<Out>(static_cast<umax>(off)));
          }
        }
      }
      return Out{r};
    }

    // Working scale for an output grid: fractional bits to resolve the notch,
    // plus integer bits of the largest output magnitude (error ~V·2^-W, so large
    // outputs like pow's 10^k need headroom), plus CORDIC guard bits. Capped at 31.
    template <boundable Out>
    constexpr int working_bits() noexcept
    {
      constexpr int GUARD = 6;
      umax den        = abs_den(Notch<Out>.Denominator);   // 1/notch
      int  notch_bits = (den <= 1) ? 0 : std::bit_width(den - 1);
      imax hi  = ceil(abs(Upper<Out>));
      imax lo  = ceil(abs(Lower<Out>));
      imax mag = (hi > lo) ? hi : lo;
      int  int_bits = (mag <= 1) ? 0 : std::bit_width(static_cast<umax>(mag));
      int  W = notch_bits + int_bits + GUARD;
      return (W < 12) ? 12 : (W > 31) ? 31 : W;
    }

    // atan(2^-i) in RADIANS at scale 2^W. i=0 is π/4 (exact, from pi_r); i≥1
    // uses the fast-converging series atan(z)=z−z³/3+z⁵/5−… for tiny z=2^-i.
    constexpr imax atan_pow2_fixed(int i, int W) noexcept
    {
      if (i == 0)
        return to_fixed(pi_r / 4, W);
      if (W - i < 1) return 0;
      imax z = imax{1} << (W - i);
      imax z2 = fmul(z, z, W), term = z, acc = 0;
      for (int k = 0; k < 64; ++k) {
        imax t = term / (2 * k + 1);
        acc += (k & 1) ? -t : t;
        if (z2 == 0) break;
        term = fmul(term, z2, W);
        if (term == 0) break;
      }
      return acc;
    }

    // Newton iterations to converge rsqrt to W bits from the y=1 seed (a∈[1,2],
    // ~2-bit start, quadratic): ≈ ceil(log2 W) + 1.
    constexpr int rsqrt_iters(int W) noexcept
    { int n = 3; while ((1 << n) < W) ++n; return n + 1; }

    // 1/sqrt(a) at scale 2^W for a ∈ [1,2], division-free Newton (y←y(3−ay²)/2).
    // `iters` lets the runtime sqrt path scale work with W while the
    // compile-time CORDIC gains stay at a fixed high count.
    constexpr imax rsqrt_fixed(imax a, int W, int iters) noexcept
    {
      imax one = imax{1} << W, three = 3 * one, y = one;
      for (int k = 0; k < iters; ++k) {
        imax ay2 = fmul(a, fmul(y, y, W), W);
        y = fmul(y, three - ay2, W) >> 1;
      }
      return y;
    }

    // √2 as a rational (literal source, like pi_r / ln2_r), for sqrt's odd-
    // exponent step.
    inline constexpr rational sqrt2_r{1414213562, 1000000000};

    // √a at scale 2^W, a_w = a·2^W ≥ 0. Reduce a = m·2^e, m ∈ [1,2);
    // √a = √m · 2^(e/2), √m = m·(1/√m) via rsqrt; odd e multiplies in √2.
    // Templated on W so the √2 constant and iteration count fold at compile time.
    template <int W>
    constexpr imax sqrt_fixed(imax a_w) noexcept
    {
      if (a_w <= 0) return 0;
      int  lead = 63 - std::countl_zero(static_cast<umax>(a_w));
      int  e    = lead - W;
      imax m_w  = (e >= 0) ? (a_w >> e) : (a_w << (-e));    // m·2^W ∈ [2^W, 2^(W+1))
      imax sm   = fmul(m_w, rsqrt_fixed(m_w, W, rsqrt_iters(W)), W);        // √m · 2^W
      if (e & 1) { constexpr imax sqrt2_w = to_fixed(sqrt2_r, W); sm = fmul(sm, sqrt2_w, W); }
      int h = e >> 1;                                       // floor(e/2)
      return (h >= 0) ? (sm << h) : (sm >> (-h));
    }

    // CORDIC circular gain 1/K = ∏ 1/√(1+4^-i) at scale 2^W.
    constexpr imax cordic_invgain(int W, int N) noexcept
    {
      imax one = imax{1} << W, invK = one;
      for (int i = 0; i < N; ++i) {
        if (2 * i > W - 1) break;
        invK = fmul(invK, rsqrt_fixed(one + (one >> (2 * i)), W, 12), W);
      }
      return invK;
    }

    // Per-<W,N> compile-time atan table + prescaled gain (one per instantiation).
    template <int W, int N>
    inline constexpr auto cordic_atan_tbl = []{
      std::array<imax, static_cast<std::size_t>(N)> t{};
      for (int i = 0; i < N; ++i) t[i] = atan_pow2_fixed(i, W);
      return t;
    }();
    template <int W, int N>
    inline constexpr imax cordic_invgain_v = cordic_invgain(W, N);

    // Circular rotation: sin/cos of z (radians at scale 2^W, |z| ≤ ~π/2).
    template <int W, int N>
    constexpr void cordic_sincos(imax z, imax& sin_out, imax& cos_out) noexcept
    {
      imax x = cordic_invgain_v<W, N>, y = 0;
      for (int i = 0; i < N; ++i) {
        imax d  = (z >= 0) ? 1 : -1;
        imax xn = x - d * (y >> i);
        imax yn = y + d * (x >> i);
        z -= d * cordic_atan_tbl<W, N>[i];
        x = xn; y = yn;
      }
      sin_out = y; cos_out = x;
    }

    // sin(x) as a rational, x given as a Q.W turn-phase (one turn = 2^W). Reduces
    // to the first quadrant in turns (exact powers of two), then CORDICs the
    // residual converted to radians. cos = sin(+¼ turn).
    template <int W, int N>
    constexpr rational sin_from_turn_fixed(imax turn_w) noexcept
    {
      imax one_turn = imax{1} << W, half = imax{1} << (W - 1), quarter = imax{1} << (W - 2);
      turn_w &= (one_turn - 1);                      // wrap into [0,1) turn
      bool flip = (turn_w & half) != 0;
      turn_w &= (half - 1);
      if (turn_w > quarter) turn_w = half - turn_w;  // reflect about π/4
      // Exact zero at multiples of a half-turn: CORDIC leaves a ~1-ULP residual
      // at angle 0, but sin(kπ) must be exactly 0 (pole detection in tan relies
      // on it). Quadrant peaks (turn_w == quarter) round to ±1 on the grid.
      if (turn_w == 0) return rational{0};
      imax rad = fmul(turn_w, to_fixed(two_pi_r, W), W);
      imax s, c;
      cordic_sincos<W, N>(rad, s, c);
      return fixed_to_rational(flip ? -s : s, W);
    }
    template <int W, int N>
    constexpr rational cos_from_turn_fixed(imax turn_w) noexcept
    { return sin_from_turn_fixed<W, N>(turn_w + (imax{1} << (W - 2))); }

    // 1/(2π) as a rational, for radians→turn reduction at any scale.
    inline constexpr rational inv_two_pi =
      (rational{1} / two_pi_r).value();

    // radians → turn at scale 2^W. The single-term product's error (~2^-(W+1))
    // scales with |a|, capping the envelope at ±1024 rad — grids within it keep
    // that expression verbatim (bit-identical). Wider grids (up to ±2^20 rad) use
    // a two-term hi+lo split of 1/2π (lo carried at scale 2^(W+24)) to recover
    // ~2^-W turn accuracy, combined through the 128-bit fmul.
    template <int W, boundable In>
    constexpr imax rad_to_turn_w(rational a) noexcept
    {
      const imax a_w = to_fixed(a, W);
      if constexpr (Lower<In> >= -1024 && Upper<In> <= 1024)
        return fmul(a_w, to_fixed(inv_two_pi, W), W);
      else
      {
        constexpr int  S    = W + 24;                 // ≤ 55 for W ≤ 31
        constexpr imax hi_w = to_fixed(inv_two_pi, W);
        constexpr rational lo = inv_two_pi - fixed_to_rational(hi_w, W);
        constexpr imax lo_s = to_fixed(lo, S);
        return fmul(a_w, hi_w, W) + fmul(a_w, lo_s, S);
      }
    }

    // CORDIC circular vectoring: atan2(y, x) in RADIANS at scale 2^W. Pre: x > 0
    // (caller pre-rotates other quadrants). Rotates (x, y) toward the +x axis,
    // accumulating the atan table; the gain cancels in y/x, so no prescale.
    template <int W, int N>
    constexpr imax cordic_atan2_rad(imax y, imax x) noexcept
    {
      imax z = 0;
      for (int i = 0; i < N; ++i) {
        imax dx = y >> i, dy = x >> i;
        if (y >= 0) { x += dx; y -= dy; z += cordic_atan_tbl<W, N>[i]; }
        else        { x -= dx; y += dy; z -= cordic_atan_tbl<W, N>[i]; }
      }
      return z;
    }

    //----- hyperbolic CORDIC (exp via sinh+cosh, ln via atanh-vectoring) ------

    // Reference precision for compile-time interval derivation and composed
    // endpoints (sinh/cosh/tanh/log10/cbrt/pow). Runtime impls use working_bits<Out>
    // so the value still follows the grid; this only bounds the derived intervals.
    inline constexpr int kRefBits = 30;

    // ln 2 as a rational (10-digit literal), plus its reciprocal — for exp/log
    // range reduction and base changes.
    inline constexpr rational ln2_r{6931471806, 10000000000};
    inline constexpr rational inv_ln2_r = (rational{1} / ln2_r).value();

    // atanh(2^-i) at scale 2^W (series; 2^-i ≤ ½ ⇒ converges). i ≥ 1 only.
    constexpr imax atanh_pow2_fixed(int i, int W) noexcept
    {
      if (W - i < 1) return 0;
      imax z = imax{1} << (W - i);
      imax z2 = fmul(z, z, W), term = z, acc = 0;
      for (int k = 0; k < 64; ++k) {
        acc += term / (2 * k + 1);
        if (z2 == 0) break;
        term = fmul(term, z2, W);
        if (term == 0) break;
      }
      return acc;
    }

    // Hyperbolic CORDIC shift schedule with the convergence repeats at
    // i = 4, 13, 40, … (each 3·prev+1). Length L covers W + guard distinct bits.
    template <int L>
    constexpr std::array<int, static_cast<std::size_t>(L)> hyp_seq() noexcept
    {
      std::array<int, static_cast<std::size_t>(L)> s{};
      int idx = 0, i = 1, rep = 4;
      while (idx < L) {
        s[idx++] = i;
        if (i == rep && idx < L) { s[idx++] = i; rep = 3 * rep + 1; }
        ++i;
      }
      return s;
    }

    constexpr int hyp_len(int W) noexcept { return W + 6; }

    template <int W, int L>
    inline constexpr auto cordic_atanh_tbl = []{
      constexpr auto seq = hyp_seq<L>();
      std::array<imax, static_cast<std::size_t>(L)> t{};
      for (int j = 0; j < L; ++j)
        t[j] = atanh_pow2_fixed(seq[j], W);
      return t;
    }();

    // Hyperbolic gain 1/Kh = ∏ 1/√(1−4^-i) over the schedule, at scale 2^W.
    template <int W, int L>
    inline constexpr imax cordic_hyp_invgain_v = []{
      constexpr auto seq = hyp_seq<L>();
      imax one = imax{1} << W, invK = one;
      for (int j = 0; j < L; ++j) {
        int i = seq[j];
        if (2 * i > W - 1) continue;
        invK = fmul(invK, rsqrt_fixed(one - (one >> (2 * i)), W, 12), W);   // ×1/√(1−4^-i)
      }
      return invK;
    }();

    // Rotation: sinh/cosh of z (scale 2^W, |z| ≤ ~1.11). exp(z) = sinh+cosh.
    template <int W, int L>
    constexpr void cordic_sinhcosh(imax z, imax& sh, imax& ch) noexcept
    {
      constexpr auto seq = hyp_seq<L>();
      imax x = cordic_hyp_invgain_v<W, L>, y = 0;
      for (int j = 0; j < L; ++j) {
        int i = seq[j];
        imax d  = (z >= 0) ? 1 : -1;
        imax xn = x + d * (y >> i);
        imax yn = y + d * (x >> i);
        z -= d * cordic_atanh_tbl<W, L>[j];
        x = xn; y = yn;
      }
      sh = y; ch = x;
    }

    // Vectoring: atanh(y/x) at scale 2^W (drives y → 0). ln(m) = 2·atanh((m−1)/(m+1)).
    template <int W, int L>
    constexpr imax cordic_atanh_vec(imax x, imax y) noexcept
    {
      constexpr auto seq = hyp_seq<L>();
      imax z = 0;
      for (int j = 0; j < L; ++j) {
        int i = seq[j];
        imax d  = (y < 0) ? 1 : -1;
        imax xn = x + d * (y >> i);
        imax yn = y + d * (x >> i);
        z -= d * cordic_atanh_tbl<W, L>[j];
        x = xn; y = yn;
      }
      return z;
    }

    // 2^(x_w / 2^W) as a rational, x_w a fixed-point exponent at scale 2^W.
    // Split x = k + f (k integer, f ∈ [−½,½]); 2^x = 2^k · e^(f·ln2). The 2^k
    // lives in the rational's power-of-two num/den so large |x| never overflows.
    // Pure fixed-point — composing this from a log result (pow/cbrt) never
    // stacks rational denominators.
    template <int W>
    constexpr rational exp2_from_fixed(imax x_w) noexcept
    {
      imax k   = (x_w + (imax{1} << (W - 1))) >> W;        // round to nearest int
      imax f_w = x_w - (k << W);                            // ∈ [−2^(W−1), 2^(W−1)]
      imax fr_w = fmul(f_w, to_fixed(ln2_r, W), W);         // f·ln2 (natural)
      imax er_w;
      if (fr_w == 0) er_w = imax{1} << W;                   // 2^k exactly
      else { imax sh, ch; cordic_sinhcosh<W, hyp_len(W)>(fr_w, sh, ch); er_w = sh + ch; }
      if (k <= W) return rational{static_cast<umax>(er_w), imax{1} << (W - k)};
      return rational{static_cast<umax>(er_w) << (k - W), 1};
    }

    // ln(w) at scale 2^W as a fixed-point imax. Leading-bit reduce w = 2^e·m,
    // m ∈ [1,2); ln(w) = e·ln2 + 2·atanh((m−1)/(m+1)). Pre: w > 0.
    template <int W>
    constexpr imax ln_to_fixed(rational w) noexcept
    {
      imax w_w  = to_fixed(w, W);
      int  lead = 63 - std::countl_zero(static_cast<umax>(w_w));
      int  e    = lead - W;
      imax one  = imax{1} << W;
      imax m_w  = (e >= 0) ? (w_w >> e) : (w_w << (-e));   // m·2^W ∈ [2^W, 2^(W+1))
      imax z    = cordic_atanh_vec<W, hyp_len(W)>(m_w + one, m_w - one);
      return e * to_fixed(ln2_r, W) + 2 * z;
    }

    // log2(x) at scale 2^W as imax: ln(x)·log2(e).
    template <int W>
    constexpr imax log2_to_fixed(rational x) noexcept
    { return fmul(ln_to_fixed<W>(x), to_fixed(inv_ln2_r, W), W); }

    // e^(v_w / 2^W) as a rational: 2^(v·log2 e).
    template <int W>
    constexpr rational exp_from_fixed(imax v_w) noexcept
    { return exp2_from_fixed<W>(fmul(v_w, to_fixed(inv_ln2_r, W), W)); }

    // Rational-input wrappers (inputs are small-denominator values; fine to
    // marshal through to_fixed). pow/cbrt compose via the *_fixed primitives
    // above instead, to avoid rational-denominator blow-up.
    template <int W>
    constexpr rational exp_fixed(rational v) noexcept
    { return exp_from_fixed<W>(to_fixed(v, W)); }
    template <int W>
    constexpr rational ln_fixed(rational w) noexcept
    { return fixed_to_rational(ln_to_fixed<W>(w), W); }
    template <int W>
    constexpr rational exp2_fixed(rational x) noexcept
    { return exp2_from_fixed<W>(to_fixed(x, W)); }
    template <int W>
    constexpr rational log2_fixed(rational x) noexcept
    { return fixed_to_rational(log2_to_fixed<W>(x), W); }

    // 1/ln10 at scale 2^W — for log10 = ln·(1/ln10), composed in fixed-point.
    template <int W>
    constexpr imax inv_ln10_fixed() noexcept
    {
      // ln10 ≈ 2.302585 (small, no overflow): derive the rational once, marshal.
      return to_fixed((rational{1} /
                       fixed_to_rational(ln_to_fixed<W>(rational{10}), W)).value(), W);
    }
  } // namespace detail

  // sin: radians-valued bound → amplitude on the auto-deduced output grid.
  // Converts to a turn (× 1/(2π)) at the grid-derived working scale, then runs
  // the circular-CORDIC reducer. Inputs up to |angle| ≤ 2^20 rad (see
  // rad_to_turn_w for the reduction split beyond ±1024).
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out sin_impl(In angle) noexcept
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "bnd::math::sin: input magnitudes must be \u2264 2^20 rad");
    static_assert(Lower<Out> <= -1 && Upper<Out> >= 1,
                  "bnd::math::sin: Out must cover [-1, 1]");

    constexpr int W = detail::working_bits<Out>();
    imax turn_w = detail::rad_to_turn_w<W, In>(angle);
    return detail::store_grid<Out>(detail::sin_from_turn_fixed<W, W>(turn_w));
  }

  // cos: radians-valued bound → amplitude. cos(x) = sin(x + π/2) — add a
  // quarter-turn before the quadrant reducer, same precision as sin.
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out cos_impl(In angle) noexcept
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "bnd::math::cos: input magnitudes must be \u2264 2^20 rad");
    static_assert(Lower<Out> <= -1 && Upper<Out> >= 1,
                  "bnd::math::cos: Out must cover [-1, 1]");

    constexpr int W = detail::working_bits<Out>();
    imax turn_w = detail::rad_to_turn_w<W, In>(angle);
    return detail::store_grid<Out>(detail::cos_from_turn_fixed<W, W>(turn_w));
  }

  namespace detail
  {
    using namespace bnd::detail;

    // tan (turn-input, internal). sin/cos from the grid-scaled engine, divided
    // with a pole guard. Returns `unexpected(errc::division_by_zero)` when the
    // phase lands on a pole (cos == 0) and `unexpected(errc::overflow)` when the
    // result exceeds Out's range.
    template <boundable Out, boundable In>
    [[nodiscard]] constexpr slim::expected<Out, errc> tan_turn_impl(In phase) noexcept
    {
      constexpr int N = turn_bits<In>;
      static_assert(N >= 2 && N <= 30, "bnd::math: turn-phase N must be in [2, 30]");

      constexpr int W = working_bits<Out>();
      imax raw    = raw_imax(phase);
      imax turn_w = (W >= N) ? (raw << (W - N)) : (raw >> (N - W));

      rational sin_v = sin_from_turn_fixed<W, W>(turn_w);
      rational cos_v = cos_from_turn_fixed<W, W>(turn_w);
      if (cos_v == 0) return slim::unexpected(errc::division_by_zero);

      rational tan_v = (sin_v / cos_v).value();
      // Under a clamp policy the out-of-range result saturates via the
      // store below instead of erroring; the pole stays an error.
      if constexpr (!has_flag(BoundPolicy<Out>, clamp))
        if (tan_v < Lower<Out> || tan_v > Upper<Out>)
          return slim::unexpected(errc::overflow);

      return detail::store_grid<Out>(tan_v);
    }
  } // namespace detail

  // tan: radians-valued bound → amplitude, with pole guard. sin/cos from the
  // radians input, divided. Returns `unexpected(division_by_zero)` if cos rounds
  // to 0 (input on a pole), `unexpected(overflow)` if the result exceeds Out.
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr slim::expected<Out, errc> tan_impl(In angle) noexcept
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "bnd::math::tan: input magnitudes must be \u2264 2^20 rad");

    constexpr int W = detail::working_bits<Out>();
    imax turn_w = detail::rad_to_turn_w<W, In>(angle);

    bnd::detail::rational sin_v = detail::sin_from_turn_fixed<W, W>(turn_w);
    bnd::detail::rational cos_v = detail::cos_from_turn_fixed<W, W>(turn_w);

    if (cos_v == 0) return slim::unexpected(errc::division_by_zero);

    bnd::detail::rational tan_v = (sin_v / cos_v).value();
    // Under a clamp policy the out-of-range result saturates via the store
    // below instead of erroring; the pole stays an error.
    if constexpr (!has_flag(BoundPolicy<Out>, clamp))
      if (tan_v < Lower<Out> || tan_v > Upper<Out>)
        return slim::unexpected(errc::overflow);

    return detail::store_grid<Out>(tan_v);
  }


  // log2: positive bound → bound. log2(x) = ln(x)·log2(e) via the grid-scaled
  // hyperbolic-CORDIC `log2_fixed` core (leading-bit reduction + atanh vectoring).
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out log2_impl(In x) noexcept
  {
    static_assert(Lower<In> > 0,
                  "bnd::math::log2: input must be strictly positive");

    return detail::store_grid<Out>(detail::log2_fixed<detail::working_bits<Out>()>(bnd::detail::rational{x}));
  }

  // exp2: bound → bound, returning 2^x. 2^x = e^(x·ln2) via the grid-scaled
  // hyperbolic-CORDIC `exp2_fixed` core (integer/fractional split + sinh/cosh).
  //
  // Restrict |x| ≤ 30 so the rational denominator 2^(30 - k) fits in int63.
  // The output `Out` must include non-negative values and cover at least
  // [2^Lower<In>, 2^Upper<In>] — anything narrower needs `clamp` to absorb
  // overflow at the assignment.
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out exp2_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -30 && Upper<In> <= 30,
                  "bnd::math::exp2: input must be in [-30, 30]");
    static_assert(Lower<Out> >= 0,
                  "bnd::math::exp2: Out must be non-negative");

    return detail::store_grid<Out>(detail::exp2_fixed<detail::working_bits<Out>()>(bnd::detail::rational{x}));
  }

  // exp: thin wrapper. exp(x) = exp2(x · log2(e)). The scaling factor
  // log2(e) ≈ 1.4427, so x must stay inside [-30/log2(e), 30/log2(e)] ≈
  // [-20.79, 20.79] for exp2's denominator-shift envelope. We use [-20, 20].
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out exp_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -20 && Upper<In> <= 20,
                  "bnd::math::exp: input must be in [-20, 20]");
    static_assert(Lower<Out> >= 0,
                  "bnd::math::exp: Out must be non-negative");

    return detail::store_grid<Out>(detail::exp_fixed<detail::working_bits<Out>()>(bnd::detail::rational{x}));
  }

  // log: thin wrapper. log(x) = log2(x) · ln(2). Result precision matches
  // log2 minus 1-2 ULP from the final fixed-point scaling.
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out log_impl(In x) noexcept
  {
    static_assert(Lower<In> > 0,
                  "bnd::math::log: input must be strictly positive");

    return detail::store_grid<Out>(detail::ln_fixed<detail::working_bits<Out>()>(bnd::detail::rational{x}));
  }

  // pow_base<Base>(x) = Base^x for compile-time-known integer Base ≥ 2.
  // Implemented as exp2(x · log2(Base)) with log2(Base) from the grid-scaled
  // `log2_to_fixed` core — no hand-typed magic constants.
  // For Base = 10, this is the building block for `db_to_linear`.
  template <imax Base, boundable Out, boundable In>
  [[nodiscard]] constexpr Out pow_base_impl(In x) noexcept
  {
    static_assert(Base >= 2, "bnd::math::pow_base: Base must be ≥ 2");
    static_assert(Lower<Out> >= 0,
                  "bnd::math::pow_base: Out must be non-negative");

    constexpr int W = detail::working_bits<Out>();
    imax lb_w = detail::log2_to_fixed<W>(bnd::detail::rational{Base});   // log2(Base)·2^W
    imax sc_w = detail::fmul(detail::to_fixed(bnd::detail::rational{x}, W), lb_w, W);
    return detail::store_grid<Out>(detail::exp2_from_fixed<W>(sc_w));
  }


  // atan2: signed bound, signed bound → radians ∈ [-π, π], via CORDIC vectoring
  // with quadrant pre-rotation. CORDIC depends only on y/x, so inputs beyond
  // magnitude 1 are normalized by the larger magnitude (exact rational division);
  // inputs already in [-1, 1] skip it.
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out atan2_impl(In y, In x) noexcept
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "bnd::math::atan2: input magnitudes must be \u2264 2^20 for the working-scale envelope");
    static_assert(Lower<Out> <= -detail::pi_r && Upper<Out> >= detail::pi_r,
                  "bnd::math::atan2: Out must cover [-π, π]");

    constexpr int W = detail::working_bits<Out>();
    bnd::detail::rational yv = y, xv = x;
    {
      bnd::detail::rational ay = bnd::detail::abs(yv);
      bnd::detail::rational ax = bnd::detail::abs(xv);
      bnd::detail::rational m  = (ax > ay) ? ax : ay;
      if (m > bnd::detail::rational{1})
      {
        yv = yv / m;
        xv = xv / m;
      }
    }
    imax y_w = detail::to_fixed(yv, W);
    imax x_w = detail::to_fixed(xv, W);

    // atan2(0, 0) is undefined; convention is 0. Without the guard CORDIC
    // accumulates the angle table on zero x,y and produces garbage.
    if (x_w == 0 && y_w == 0) return Out{0};

    // Quadrant pre-rotation: CORDIC requires x > 0. For x < 0, rotate the
    // vector by ±π/2 (in radians, at scale W) to land in the right half-plane
    // and add the rotation back at the end.
    //   Q2 (x<0, y≥0): (x',y') = (y, −x),  θ = CORDIC + π/2.
    //   Q3 (x<0, y<0): (x',y') = (−y, x),  θ = CORDIC − π/2.
    imax half_pi_w = detail::to_fixed(detail::pi_r / 2, W);
    imax pre_rotation = 0;
    if (x_w < 0) {
      if (y_w >= 0) { imax nx = y_w;  imax ny = -x_w; x_w = nx; y_w = ny; pre_rotation =  half_pi_w; }
      else          { imax nx = -y_w; imax ny =  x_w; x_w = nx; y_w = ny; pre_rotation = -half_pi_w; }
    }

    imax rad = detail::cordic_atan2_rad<W, W>(y_w, x_w) + pre_rotation;   // radians, scale W
    return detail::store_grid<Out>(detail::fixed_to_rational(rad, W));
  }

  //---------------------------------------------------------------------------
  // Algebraic tier — exact, no polynomial machinery. Each function wraps the
  // corresponding `rational` operation and routes through `Out`'s assignment.
  //---------------------------------------------------------------------------

  // |x|. Output Lower must be ≥ 0 (the result is always non-negative).
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out abs_impl(In x) noexcept
  {
    static_assert(Lower<Out> <= 0,
                  "bnd::math::abs: Out must include 0");
    return detail::store_grid<Out>(bnd::detail::abs(bnd::detail::rational{x}));
  }

  // ⌊x⌋ — largest integer ≤ x.
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out floor_impl(In x) noexcept
  {
    return detail::store_grid<Out>(floor(bnd::detail::rational{x}));
  }

  // ⌈x⌉ — smallest integer ≥ x.
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out ceil_impl(In x) noexcept
  {
    return detail::store_grid<Out>(ceil(bnd::detail::rational{x}));
  }

  // x rounded to nearest integer, half-away-from-zero (matches the existing
  // `rational::round()` convention used throughout the library).
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out round_impl(In x) noexcept
  {
    return detail::store_grid<Out>(round(bnd::detail::rational{x}));
  }

  // x truncated toward zero. Distinct from floor for negative inputs:
  // trunc(-1.7) = -1 vs floor(-1.7) = -2.
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out trunc_impl(In x) noexcept
  {
    return detail::store_grid<Out>(trunc(bnd::detail::rational{x}));
  }

  namespace detail
  {
    using namespace bnd::detail;

    // Gate for fmod's integer fast path. When both operands and Out are
    // integer-backed on commensurable notches, fmod collapses to ONE integer
    // remainder in units of g = gcd(Notch<InX>, Notch<InY>): with x = a·g and
    // y = b·g, x − trunc(x/y)·y = (a − (a/b)·b)·g = (a % b)·g exactly (C++ %
    // is truncated division, the same convention). Conditions:
    //   * integer raws only (rational/double raws keep the rational path),
    //   * non-zero notches, g on Out's grid (g / Notch<Out> integer),
    //   * divisor grid excludes zero (no runtime zero check needed),
    //   * Out's interval covers ±max|y| (result magnitude is < |y|),
    //   * all unit counts fit comfortably in imax (headroom 4).
    template <boundable Out, boundable InX, boundable InY>
    inline constexpr bool fmod_int_fast = []{
      if (rational_raw<InX> || fp_raw<InX>
       || rational_raw<InY> || fp_raw<InY>
       || rational_raw<Out> || fp_raw<Out>)
        return false;
      if (Notch<InX> == 0 || Notch<InY> == 0 || Notch<Out> == 0)
        return false;
      if (!DivisorExcludesZero<InY>)
        return false;
      auto go = gcd(Notch<InX>, Notch<InY>);
      if (!go.has_value()) return false;
      rational g = *go;
      auto qo = g / Notch<Out>;
      if (!qo.has_value() || abs_den(qo->Denominator) != 1)
        return false;
      rational maxx =
          abs(Lower<InX>) > abs(Upper<InX>)
            ? abs(Lower<InX>) : abs(Upper<InX>);
      rational maxy =
          abs(Lower<InY>) > abs(Upper<InY>)
            ? abs(Lower<InY>) : abs(Upper<InY>);
      if (Lower<Out> > -maxy || Upper<Out> < maxy)
        return false;
      constexpr umax lim = static_cast<umax>(std::numeric_limits<imax>::max() / 4);
      auto ux = maxx / g;  auto uy = maxy / g;  auto uo = maxy / Notch<Out>;
      return ux.has_value() && uy.has_value() && uo.has_value()
          && ux->Numerator <= lim && uy->Numerator <= lim && uo->Numerator <= lim;
    }();
  }

  // x mod y = x − ⌊x/y⌋·y (truncated-division convention, matching std::fmod).
  // Result has the sign of x. Pre: y must not span zero (caller-enforced).
  template <boundable Out, boundable InX, boundable InY>
  [[nodiscard]] constexpr Out fmod_impl(InX x, InY y) noexcept
  {
    if constexpr (detail::fmod_int_fast<Out, InX, InY>)
    {
      // One integer remainder in g-units; bit-identical to the rational path.
      constexpr bnd::detail::rational g = *bnd::detail::gcd(Notch<InX>, Notch<InY>);
      constexpr imax wx  = trunc((Notch<InX> / g).value());
      constexpr imax wy  = trunc((Notch<InY> / g).value());
      constexpr imax wo  = trunc((g / Notch<Out>).value());
      constexpr imax lox = trunc((Lower<InX> / g).value());   // exact: grid invariant
      constexpr imax loy = trunc((Lower<InY> / g).value());
      constexpr imax loo = trunc((Lower<Out> / Notch<Out>).value());
      const imax a = bnd::detail::raw_imax(x) * wx
                   + (bnd::detail::index_raw<InX> ? lox : 0);
      const imax b = bnd::detail::raw_imax(y) * wy
                   + (bnd::detail::index_raw<InY> ? loy : 0);
      const imax r = a % b;                                    // |r| < |b|, in Out's range
      return Out::from_raw(bnd::detail::raw_from_offset<Out>(r * wo - loo));
    }
    else
    {
      bnd::detail::rational xv = x;
      bnd::detail::rational yv = y;
      bnd::detail::rational q  = xv / yv;
      imax     qt = trunc(q);
      bnd::detail::rational qy = qt * yv;
      bnd::detail::rational r  = xv - qy;
      return detail::store_grid<Out>(r);
    }
  }

  //---------------------------------------------------------------------------
  // Auto-deducing forms — algebraic tier.
  //
  // Each function gets a second overload that derives `Out` from `In` and
  // delegates to the explicit form. `f<Out>(x)` picks the explicit form, `f(x)`
  // the auto form (explicit Out can't be deduced from a parameter, so it drops
  // out). Notch policy: abs/fmod inherit `Notch<In>`; floor/ceil/round/trunc
  // deduce `notch<1>` since their outputs are integer-valued.
  //---------------------------------------------------------------------------

  //---------------------------------------------------------------------------
  // pown<E> — compile-time integer powers, pure grid arithmetic
  //---------------------------------------------------------------------------
  // Repeated squaring in bound-space: every multiply widens the result grid
  // corner-correctly, so the result is exact for exact inputs and negative
  // bases are fine. No engine, no `real` requirement — works on any bound
  // (like abs/floor/fmod). Checked rational raws may return slim::optional
  // per the usual arithmetic vocabulary. Negative exponents are deferred
  // (they need the division optional story).
  template <imax E, boundable In>
    requires (E >= 0)
  [[nodiscard]] constexpr auto pown(In x) noexcept
  {
    if constexpr (E == 0)      { (void)x; return just<1>; }
    else if constexpr (E == 1) return x;
    else if constexpr (E % 2)  return x * pown<E - 1>(x);
    else                       { auto h = pown<E / 2>(x); return h * h; }
  }

  namespace detail
  {
    using namespace bnd::detail;

    // max(|Lower<In>|, |Upper<In>|) as a constexpr rational. Used to size
    // the auto-deduced abs output.
    template <boundable In>
    inline constexpr rational abs_auto_upper =
      (abs(Lower<In>) > abs(Upper<In>))
        ? abs(Lower<In>) : abs(Upper<In>);

    template <boundable In>
    using abs_auto_t = bound<{{rational{0}, abs_auto_upper<In>},
                              Notch<In>}, BoundPolicy<In>>;

    template <boundable In>
    using floor_auto_t = bound<{{rational{floor(Lower<In>)},
                                  rational{floor(Upper<In>)}},
                                 notch<1>}, BoundPolicy<In>>;

    template <boundable In>
    using ceil_auto_t = bound<{{rational{ceil(Lower<In>)},
                                 rational{ceil(Upper<In>)}},
                                notch<1>}, BoundPolicy<In>>;

    template <boundable In>
    using round_auto_t = bound<{{rational{round(Lower<In>)},
                                  rational{round(Upper<In>)}},
                                 notch<1>}, BoundPolicy<In>>;

    template <boundable In>
    using trunc_auto_t = bound<{{rational{trunc(Lower<In>)},
                                  rational{trunc(Upper<In>)}},
                                 notch<1>}, BoundPolicy<In>>;

    // (fmod has no auto form: with two boundable inputs, `fmod<X>(x, y)` is
    // ambiguous between the explicit-Out and auto overloads — partial ordering
    // can't tell them apart. The explicit form is the canonical entry point.)
  } // namespace detail

  template <boundable In>
  [[nodiscard]] constexpr auto abs(In x) noexcept { return abs_impl<detail::abs_auto_t<In>>(x); }

  template <boundable In>
  [[nodiscard]] constexpr auto floor(In x) noexcept { return floor_impl<detail::floor_auto_t<In>>(x); }

  template <boundable In>
  [[nodiscard]] constexpr auto ceil(In x) noexcept { return ceil_impl<detail::ceil_auto_t<In>>(x); }

  template <boundable In>
  [[nodiscard]] constexpr auto round(In x) noexcept { return round_impl<detail::round_auto_t<In>>(x); }

  template <boundable In>
  [[nodiscard]] constexpr auto trunc(In x) noexcept { return trunc_impl<detail::trunc_auto_t<In>>(x); }

  // sqrt: non-negative bound → bound. Newton-Raphson on grid-scaled integer math
  // with a leading-bit initial guess; input must have Lower == 0. The mixed-sign
  // overload below accepts Lower < 0 and errors on a negative runtime value.
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out sqrt_impl(In x) noexcept
  {
    static_assert(Lower<In> == 0,
                  "bnd::math::sqrt: input must start at 0 (use the mixed-sign overload)");
    static_assert(Lower<Out> <= 0,
                  "bnd::math::sqrt: Out must include 0");

    constexpr int W = detail::working_bits<Out>();
    imax a_w = detail::to_fixed(bnd::detail::rational{x}, W);
    return detail::store_grid<Out>(detail::fixed_to_rational(detail::sqrt_fixed<W>(a_w), W));
  }

  // Mixed-sign sqrt: accepts inputs whose interval crosses zero. Returns
  // `unexpected(errc::domain_error)` on a negative runtime value, else same as
  // sqrt_impl.
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr slim::expected<Out, errc> sqrt_signed_impl(In x) noexcept
  {
    static_assert(Lower<Out> <= 0,
                  "bnd::math::sqrt: Out must include 0");

    bnd::detail::rational v = bnd::detail::as_rational(x);
    if (v < bnd::detail::rational{0})
      return slim::unexpected(errc::domain_error);

    constexpr int W = detail::working_bits<Out>();
    imax a_w = detail::to_fixed(v, W);
    return detail::store_grid<Out>(detail::fixed_to_rational(detail::sqrt_fixed<W>(a_w), W));
  }

  //---------------------------------------------------------------------------
  // Auto-deducing forms — monotonic transcendental tier. Each derives Out from
  // In: Lower/Upper from running the engine cores on the input endpoints at
  // compile time, rounded outward to Notch<In> so the deduced bound covers the
  // true range even for irrational endpoints; notch and policy inherited from In.
  //---------------------------------------------------------------------------
  namespace detail
  {
    using namespace bnd::detail;

    // Round a rational down to the nearest multiple of `notch`.
    constexpr rational floor_to_notch(rational x, rational notch) noexcept
    {
      rational q = x / notch;
      imax n  = floor(q);
      return n * notch;
    }

    // Round a rational up to the nearest multiple of `notch`.
    constexpr rational ceil_to_notch(rational x, rational notch) noexcept
    {
      rational q = x / notch;
      imax n = ceil(q);
      return n * notch;
    }

    // Helpers: evaluate the engine cores on a compile-time-known
    // rational endpoint and return the result as a rational.
    constexpr rational sqrt_endpoint(rational v) noexcept
    {
      if (v == 0) return rational{0};
      return fixed_to_rational(sqrt_fixed<kRefBits>(to_fixed(v, kRefBits)), kRefBits);
    }

    constexpr rational exp2_endpoint(rational v) noexcept
    { return exp2_fixed<kRefBits>(v); }

    constexpr rational log2_endpoint(rational v) noexcept
    { return log2_fixed<kRefBits>(v); }

    constexpr rational exp_endpoint(rational v) noexcept
    { return exp_fixed<kRefBits>(v); }

    constexpr rational log_endpoint(rational v) noexcept
    { return ln_fixed<kRefBits>(v); }

    template <imax Base>
    constexpr rational pow_base_endpoint(rational v) noexcept
    {
      imax sc_w = fmul(to_fixed(v, kRefBits),
                       log2_to_fixed<kRefBits>(rational{Base}), kRefBits);
      return exp2_from_fixed<kRefBits>(sc_w);
    }

    // Deduction aliases. Each rounds endpoints outward to Notch<In> and adds
    // `round_nearest` — the cores emit sub-notch drift, so the assignment needs
    // a rounding rule to land on the grid.
    template <boundable In>
    using sqrt_auto_t = bound<{{rational{0},
                                ceil_to_notch(sqrt_endpoint(Upper<In>), Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    // Mixed-sign sqrt: Upper of the result is sqrt of the larger absolute
    // endpoint, since the runtime value can be anywhere in [Lower, Upper].
    template <boundable In>
    inline constexpr rational sqrt_signed_upper =
        (abs(Lower<In>) > abs(Upper<In>))
            ? abs(Lower<In>) : abs(Upper<In>);

    template <boundable In>
    using sqrt_signed_auto_t = bound<{{rational{0},
                                       ceil_to_notch(sqrt_endpoint(sqrt_signed_upper<In>),
                                                     Notch<In>)},
                                      Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using exp2_auto_t = bound<{{floor_to_notch(exp2_endpoint(Lower<In>), Notch<In>),
                                ceil_to_notch (exp2_endpoint(Upper<In>), Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using log2_auto_t = bound<{{floor_to_notch(log2_endpoint(Lower<In>), Notch<In>),
                                ceil_to_notch (log2_endpoint(Upper<In>), Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using exp_auto_t = bound<{{floor_to_notch(exp_endpoint(Lower<In>), Notch<In>),
                               ceil_to_notch (exp_endpoint(Upper<In>), Notch<In>)},
                              Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using log_auto_t = bound<{{floor_to_notch(log_endpoint(Lower<In>), Notch<In>),
                               ceil_to_notch (log_endpoint(Upper<In>), Notch<In>)},
                              Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <imax Base, boundable In>
    using pow_base_auto_t = bound<{{floor_to_notch(pow_base_endpoint<Base>(Lower<In>), Notch<In>),
                                    ceil_to_notch (pow_base_endpoint<Base>(Upper<In>), Notch<In>)},
                                   Notch<In>}, BoundPolicy<In> | round_nearest>;
  } // namespace detail

  template <boundable In>
    requires (Lower<In> == bnd::detail::rational{0})
  [[nodiscard]] BND_MATH_FN auto sqrt(In x) noexcept
  {
    static_assert(detail::require_snap<In>());
#if defined(BND_MATH_NO_FP)
    return sqrt_impl<detail::sqrt_auto_t<In>>(x);
#elif defined(BND_MATH_FLOAT)
    return flt::sqrt_core<detail::sqrt_auto_t<In>>(x);
#else
    return dbl::sqrt_core<detail::sqrt_auto_t<In>>(x);
#endif
  }

  // Mixed-sign overload: dispatches to `sqrt_signed_impl`, returning
  // `slim::expected<bound, errc>` so a negative runtime value surfaces as
  // `unexpected(errc::domain_error)` instead of UB.
  template <boundable In>
    requires (Lower<In> < bnd::detail::rational{0})
  [[nodiscard]] BND_MATH_FN auto sqrt(In x) noexcept
  {
    static_assert(detail::require_snap<In>());
    using Out = detail::sqrt_signed_auto_t<In>;
#if defined(BND_MATH_NO_FP)
    return sqrt_signed_impl<Out>(x);
#elif defined(BND_MATH_FLOAT)
    float v = static_cast<float>(static_cast<double>(x));
    if (v < 0.0f)
      return slim::expected<Out, errc>{slim::unexpected(errc::domain_error)};
    return slim::expected<Out, errc>{flt::store<Out>(flt::detail::d_sqrt(v))};
#else
    double v = static_cast<double>(x);
    if (v < 0.0)
      return slim::expected<Out, errc>{slim::unexpected(errc::domain_error)};
    return slim::expected<Out, errc>{dbl::store<Out>(dbl::detail::d_sqrt(v))};
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto exp2(In x) noexcept
  {
    static_assert(detail::require_snap<In>());
#if defined(BND_MATH_NO_FP)
    return exp2_impl<detail::exp2_auto_t<In>>(x);
#elif defined(BND_MATH_FLOAT)
    return flt::exp2_core<detail::exp2_auto_t<In>>(x);
#else
    return dbl::exp2_core<detail::exp2_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto log2(In x) noexcept
  {
    static_assert(detail::require_snap<In>());
    // Domain guard belongs on the shared entry point, not just the fixed
    // engine's *_impl: the FP engines' log_core has no singularity check,
    // so log2(x<=0) would silently store finite garbage (e.g. log2(0) ≈ -7).
    static_assert(Lower<In> > 0, "bnd::math::log2: input must be strictly positive");
#if defined(BND_MATH_NO_FP)
    return log2_impl<detail::log2_auto_t<In>>(x);
#elif defined(BND_MATH_FLOAT)
    return flt::log2_core<detail::log2_auto_t<In>>(x);
#else
    return dbl::log2_core<detail::log2_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto exp(In x) noexcept
  {
    static_assert(detail::require_snap<In>());
#if defined(BND_MATH_NO_FP)
    return exp_impl<detail::exp_auto_t<In>>(x);
#elif defined(BND_MATH_FLOAT)
    return flt::exp_core<detail::exp_auto_t<In>>(x);
#else
    return dbl::exp_core<detail::exp_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto log(In x) noexcept
  {
    static_assert(detail::require_snap<In>());
    static_assert(Lower<In> > 0, "bnd::math::log: input must be strictly positive");
#if defined(BND_MATH_NO_FP)
    return log_impl<detail::log_auto_t<In>>(x);
#elif defined(BND_MATH_FLOAT)
    return flt::log_core<detail::log_auto_t<In>>(x);
#else
    return dbl::log_core<detail::log_auto_t<In>>(x);
#endif
  }

  template <imax Base, boundable In>
  [[nodiscard]] BND_MATH_FN auto pow_base(In x) noexcept
  {
    static_assert(detail::require_snap<In>());
    using Out = detail::pow_base_auto_t<Base, In>;
#if defined(BND_MATH_NO_FP)
    return pow_base_impl<Base, Out>(x);
#elif defined(BND_MATH_FLOAT)
    return flt::store<Out>(flt::detail::d_pow(static_cast<float>(Base), static_cast<float>(static_cast<double>(x))));
#else
    return dbl::store<Out>(dbl::detail::d_pow(static_cast<double>(Base), static_cast<double>(x)));
#endif
  }

  //---------------------------------------------------------------------------
  // Auto-deducing forms — trig + atan2 + tan + fmod.
  //
  // sin / cos default to the full amplitude range [-1, 1]; atan2 defaults to
  // the full angle range [-π, π] radians. tan defaults to [-1024, 1024]
  // (covers all phases >1 slot from a pole; closer-to-pole phases trip the
  // overflow branch of the returned `expected`). fmod inherits sign from x.
  // Notch is inherited from input throughout.
  //---------------------------------------------------------------------------
  namespace detail
  {
    using namespace bnd::detail;

    template <boundable In>
    using sin_auto_t = bound<{{-rational{1}, rational{1}},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using cos_auto_t = sin_auto_t<In>;

    // Output covers [-π, π] rounded outward to notch multiples — the exact
    // ±π endpoints are irrational and would violate the grid's divides-evenly
    // invariant against a rational notch.
    template <boundable In>
    using atan2_auto_t = bound<{{floor_to_notch(-pi_r, Notch<In>),
                                 ceil_to_notch (pi_r, Notch<In>)},
                                Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using tan_auto_t = bound<{{-rational{1024}, rational{1024}},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable InX, boundable InY>
    using fmod_auto_t = bound<{{-abs(Upper<InY>), abs(Upper<InY>)},
                                Notch<InX>}, BoundPolicy<InX> | round_nearest>;
  } // namespace detail

  // Public auto-form trig — radians input, std::sin-shaped. The only
  // public trig entry points; the turn-input workers live in `detail::`
  // (`sin_turn_impl`, `cos_turn_impl`, `tan_turn_impl`) for internal use.
  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto sin(In angle) noexcept
  {
    static_assert(detail::require_snap<In>());
#if defined(BND_MATH_NO_FP)
    return sin_impl<detail::sin_auto_t<In>>(angle);
#elif defined(BND_MATH_FLOAT)
    return flt::sin_core<detail::sin_auto_t<In>>(angle);
#else
    return dbl::sin_core<detail::sin_auto_t<In>>(angle);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto cos(In angle) noexcept
  {
    static_assert(detail::require_snap<In>());
#if defined(BND_MATH_NO_FP)
    return cos_impl<detail::cos_auto_t<In>>(angle);
#elif defined(BND_MATH_FLOAT)
    return flt::cos_core<detail::cos_auto_t<In>>(angle);
#else
    return dbl::cos_core<detail::cos_auto_t<In>>(angle);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto atan2(In y, In x) noexcept
  {
    static_assert(detail::require_snap<In>());
#if defined(BND_MATH_NO_FP)
    return atan2_impl<detail::atan2_auto_t<In>>(y, x);
#elif defined(BND_MATH_FLOAT)
    return flt::atan2_core<detail::atan2_auto_t<In>>(y, x);
#else
    return dbl::atan2_core<detail::atan2_auto_t<In>>(y, x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto tan(In angle) noexcept
  {
    static_assert(detail::require_snap<In>());
    using Out = detail::tan_auto_t<In>;
#if defined(BND_MATH_NO_FP)
    return tan_impl<Out>(angle);
#elif defined(BND_MATH_FLOAT)
    float x = static_cast<float>(static_cast<double>(angle));
    float c = flt::detail::d_cos(x);
    if (c == 0.0f)
      return slim::expected<Out, errc>{slim::unexpected(errc::division_by_zero)};
    float t = flt::detail::d_sin(x) / c;
    if constexpr (!has_flag(BoundPolicy<Out>, clamp))   // clamp Out: saturate below
      if (t < static_cast<float>(static_cast<double>(Lower<Out>)) || t > static_cast<float>(static_cast<double>(Upper<Out>)))
        return slim::expected<Out, errc>{slim::unexpected(errc::overflow)};
    return slim::expected<Out, errc>{flt::store<Out>(t)};
#else
    double x = static_cast<double>(angle);
    double c = dbl::detail::d_cos(x);
    if (c == 0.0)
      return slim::expected<Out, errc>{slim::unexpected(errc::division_by_zero)};
    double t = dbl::detail::d_sin(x) / c;
    if constexpr (!has_flag(BoundPolicy<Out>, clamp))   // clamp Out: saturate below
      if (t < static_cast<double>(Lower<Out>) || t > static_cast<double>(Upper<Out>))
        return slim::expected<Out, errc>{slim::unexpected(errc::overflow)};
    return slim::expected<Out, errc>{dbl::store<Out>(t)};
#endif
  }

  template <boundable InX, boundable InY>
  [[nodiscard]] constexpr auto fmod(InX x, InY y) noexcept
  { return fmod_impl<detail::fmod_auto_t<InX, InY>>(x, y); }

  //===========================================================================
  // Grid-native periodic trig: circle<M> angle + caller-owned amplitude.
  //
  // A `circle<M>` is one revolution split into M equal slots, valued in DEGREES.
  // Degrees have an integer period (360), so a notch divides the circle exactly
  // and `wrap` is drift-free — unlike radians, whose 2π period no rational notch
  // divides. The raw is just the slot index 0..M-1.
  //
  // `sin`/`cos` write into a caller-supplied amplitude bound whose grid fixes the
  // output precision. The runtime path is a table lookup (no ×1/(2π), no
  // polynomial) into a first-quadrant table built at compile time by CORDIC.
  // Power-of-two M is optimal (reflection is a bitmask); any M%4==0 works.
  //===========================================================================
  template <std::uint64_t M>
  using circle = bound<{{bnd::detail::rational{0},
                         bnd::detail::rational{std::uint64_t{360} * (M - 1),
                                               static_cast<imax>(M)}},
                        notch<360, static_cast<imax>(M)>}, real | wrap>;

  // Amplitude output grid: [-1, 1] at 1/K resolution. The natural target for
  // `sin(circle<M>, amp<K>&)` — angle precision (M) and amplitude precision (K)
  // are chosen independently.
  template <std::uint64_t K>
  using amp = bound<{{bnd::detail::rational{-1}, bnd::detail::rational{1}},
                     notch<1, static_cast<imax>(K)>}, real>;

  namespace detail
  {
    using namespace bnd::detail;

    // First-quadrant sine table for an M-slot circle at working scale 2^W:
    // entry j = sin(j/M turn) for j ∈ [0, M/4], as a rational. Filled at
    // compile time by the grid-scaled CORDIC rotation (`sin_from_turn_fixed`),
    // never evaluated at runtime. Keyed by <M, W> so the entry precision tracks
    // the amplitude grid that selected W.
    template <imax M, int W>
    inline constexpr auto sin_quarter_table = []{
      std::array<rational, static_cast<std::size_t>(M / 4) + 1> t{};
      for (imax j = 0; j <= M / 4; ++j)
      {
        imax turn_w = ((j << W) + M / 2) / M;             // round(j/M · 2^W)
        t[j] = sin_from_turn_fixed<W, W>(turn_w);
      }
      return t;
    }();

    // sin(i/M turn) as a rational for any integer slot i (wraps mod M), by
    // quadrant reduction: sign from the half-turn, reflect about M/4. The table
    // holds first-quadrant magnitudes; this applies the sign. Power-of-two M
    // makes the half/quarter compares a bitmask.
    template <imax M, int W>
    constexpr rational sin_slot(imax i) noexcept
    {
      constexpr imax half = M / 2, quarter = M / 4;
      i = ((i % M) + M) % M;                  // wrap into [0, M)
      bool flip = i >= half;
      if (flip) i -= half;                    // sin(π + x) = -sin(x)
      if (i > quarter) i = half - i;          // sin(π - x) =  sin(x)
      rational mag = sin_quarter_table<M, W>[i];
      return flip ? -mag : mag;
    }

    // Recover the slot count M from a circle-shaped angle: the degree period
    // 360 divided by the notch. The public entry points validate the shape.
    template <boundable DEG>
    inline constexpr imax circle_slots =
      round((rational{360} / Notch<DEG>).value());

    // Shared shape check for the circle-input entry points.
    template <boundable DEG>
    constexpr bool valid_circle() noexcept
    {
      static_assert(Lower<DEG> == 0,
                    "bnd::math: circle angle must have Lower 0 (degrees)");
      static_assert(has_flag(BoundPolicy<DEG>, wrap),
                    "bnd::math: circle angle must carry the wrap policy");
      static_assert(has_flag(BoundPolicy<DEG>, real),
                    "bnd::math: circle angle must carry the `real` policy "
                    "(circle<M> already does; custom angle bounds must add `| real`)");
      static_assert(circle_slots<DEG> % 4 == 0,
                    "bnd::math: circle slot count M must be divisible by 4");
      return true;
    }
  } // namespace detail

  // sin(angle) → out, on out's amplitude grid. angle is a circle<M>. Reference
  // output (not a return) lets AMP be deduced from the caller's object and reuses
  // its assignment policy for the final rounding.
  template <boundable DEG, boundable AMP>
  BND_MATH_FN void sin(DEG angle, AMP& out) noexcept
  {
    static_assert(detail::valid_circle<DEG>());
#if defined(BND_MATH_NO_FP)
    constexpr imax M = detail::circle_slots<DEG>;
    constexpr int  W = detail::working_bits<AMP>();
    out = detail::sin_slot<M, W>(bnd::detail::raw_imax(angle));
#elif defined(BND_MATH_FLOAT)
    out = flt::detail::d_sin(static_cast<float>(static_cast<double>(angle)) * (flt::detail::kPi / 180.0f));
#else
    out = dbl::detail::d_sin(static_cast<double>(angle) * (dbl::detail::kPi / 180.0));
#endif
  }

  // cos(angle) → out. cos(x) = sin(x + ¼ turn): shift the slot by M/4.
  template <boundable DEG, boundable AMP>
  BND_MATH_FN void cos(DEG angle, AMP& out) noexcept
  {
    static_assert(detail::valid_circle<DEG>());
#if defined(BND_MATH_NO_FP)
    constexpr imax M = detail::circle_slots<DEG>;
    constexpr int  W = detail::working_bits<AMP>();
    out = detail::sin_slot<M, W>(bnd::detail::raw_imax(angle) + M / 4);
#elif defined(BND_MATH_FLOAT)
    out = flt::detail::d_cos(static_cast<float>(static_cast<double>(angle)) * (flt::detail::kPi / 180.0f));
#else
    out = dbl::detail::d_cos(static_cast<double>(angle) * (dbl::detail::kPi / 180.0));
#endif
  }

  // tan(angle) → out = sin/cos. Returns false (and leaves out untouched) when
  // the angle lands exactly on a pole (cos == 0); overflow of the amplitude
  // grid is handled by out's own policy (e.g. clamp).
  template <boundable DEG, boundable AMP>
  [[nodiscard]] BND_MATH_FN bool tan(DEG angle, AMP& out) noexcept
  {
    static_assert(detail::valid_circle<DEG>());
#if defined(BND_MATH_NO_FP)
    constexpr imax M = detail::circle_slots<DEG>;
    constexpr int  W = detail::working_bits<AMP>();
    imax i = bnd::detail::raw_imax(angle);
    bnd::detail::rational c = detail::sin_slot<M, W>(i + M / 4);
    if (c == 0) return false;                                  // pole
    out = (detail::sin_slot<M, W>(i) / c).value();             // sin / cos
    return true;
#elif defined(BND_MATH_FLOAT)
    float rad = static_cast<float>(static_cast<double>(angle)) * (flt::detail::kPi / 180.0f);
    float c = flt::detail::d_cos(rad);
    if (c == 0.0f) return false;                               // pole
    out = flt::detail::d_sin(rad) / c;
    return true;
#else
    double rad = static_cast<double>(angle) * (dbl::detail::kPi / 180.0);
    double c = dbl::detail::d_cos(rad);
    if (c == 0.0) return false;                                // pole
    out = dbl::detail::d_sin(rad) / c;
    return true;
#endif
  }

  //===========================================================================
  // Extended transcendentals — inverse trig, hyperbolic, log10, pow, cbrt,
  // hypot. Each composes the CORDIC / Newton cores defined above; no new
  // polynomial machinery. Outputs follow the bnd::math conventions: angles in
  // radians, runtime-conditional failures via `slim::expected<Out, errc>`,
  // statically-knowable domain limits via `static_assert`.
  //===========================================================================
  namespace detail
  {
    using namespace bnd::detail;

    // --- inverse trig (radians) -------------------------------------------
    // atan(v) in radians at scale 2^W: atan2(v, 1) — x = 1 > 0, so the
    // vectoring CORDIC runs with no pre-rotation. Grid-scaled (no Q.30).
    // Full domain: |v| > 1 reduces via atan(v) = sign(v)·(π/2 − atan(1/|v|)),
    // keeping the CORDIC argument inside its [-1, 1] window. Inputs with
    // |v| ≤ 1 take the original branch unchanged (bit-identical results).
    template <int W>
    constexpr rational atan_fixed(rational v) noexcept
    {
      rational av = abs(v);
      if (av <= rational{1})
      {
        imax rad = cordic_atan2_rad<W, W>(to_fixed(v, W), imax{1} << W);
        return fixed_to_rational(rad, W);
      }
      rational inv = 1 / av;
      imax rad = cordic_atan2_rad<W, W>(to_fixed(inv, W), imax{1} << W);
      rational mag = pi_r / 2 - fixed_to_rational(rad, W);
      return (v < rational{0}) ? -mag : mag;
    }

    // asin(v) = atan2(v, sqrt(1 − v²)); v ∈ [−1, 1] → result ∈ [−π/2, π/2].
    constexpr rational asin_endpoint(rational v) noexcept
    {
      imax one = imax{1} << kRefBits;
      imax v_w = to_fixed(v, kRefBits);
      imax c_w = sqrt_fixed<kRefBits>(one - fmul(v_w, v_w, kRefBits));   // √(1−v²) ≥ 0
      if (c_w == 0) {                                                    // v = ±1 → ±π/2
        rational half_pi = pi_r / 2;
        return (v < rational{0}) ? -half_pi : half_pi;
      }
      imax rad = cordic_atan2_rad<kRefBits, kRefBits>(v_w, c_w);        // x = c_w > 0
      return fixed_to_rational(rad, kRefBits);
    }

    // acos(v) = π/2 − asin(v); v ∈ [−1, 1] → result ∈ [0, π].
    constexpr rational acos_endpoint(rational v) noexcept
    {
      rational half_pi = pi_r / 2;
      return half_pi - asin_endpoint(v);
    }

    // --- hyperbolic (from e^x via the exp core) ---------------------------
    // sinh/cosh = (e^v ∓ e^-v)/2, combined in fixed-point at kRefBits, not as
    // rationals: e^v and e^-v have wildly different denominators and the rational
    // cross-multiply overflows imax. At scale kRefBits each term is one scaled
    // integer (|v| ≤ 10 ⇒ e^|v|·2^30 ≤ 2.4e13, well inside int63).
    constexpr rational sinh_endpoint(rational v) noexcept
    {
      imax ex  = to_fixed(exp_fixed<kRefBits>(v),  kRefBits);
      imax enx = to_fixed(exp_fixed<kRefBits>(-v), kRefBits);
      return fixed_to_rational((ex - enx) / 2, kRefBits);
    }

    constexpr rational cosh_endpoint(rational v) noexcept
    {
      imax ex  = to_fixed(exp_fixed<kRefBits>(v),  kRefBits);
      imax enx = to_fixed(exp_fixed<kRefBits>(-v), kRefBits);
      return fixed_to_rational((ex + enx) / 2, kRefBits);
    }

    // tanh via the overflow-safe form tanh(x) = (1 − e^-2|x|)/(1 + e^-2|x|),
    // odd-extended for x < 0. With u = e^-2|x| ∈ (0, 1] at scale kRefBits, the
    // quotient `((1−u)·2^kRefBits)/(1+u)` keeps the dividend bounded.
    constexpr rational tanh_endpoint(rational v) noexcept
    {
      constexpr imax one = imax{1} << kRefBits;
      rational av = abs(v);
      imax u = to_fixed(exp_fixed<kRefBits>(av * -2), kRefBits);
      imax t = ((one - u) << kRefBits) / (one + u);
      return (v < rational{0}) ? fixed_to_rational(-t, kRefBits)
                                            : fixed_to_rational(t, kRefBits);
    }

    // --- log10, cbrt ------------------------------------------------------
    constexpr rational log10_endpoint(rational v) noexcept
    { return fixed_to_rational(fmul(ln_to_fixed<kRefBits>(v), inv_ln10_fixed<kRefBits>(), kRefBits), kRefBits); }

    // cbrt(v) = sign(v)·e^(ln|v|/3); cbrt(0) = 0.
    constexpr rational cbrt_endpoint(rational v) noexcept
    {
      if (v == rational{0}) return rational{0};
      rational av = abs(v);
      rational mag = exp_from_fixed<kRefBits>(ln_to_fixed<kRefBits>(av) / 3);
      return (v < rational{0}) ? -mag : mag;
    }

    // hypot(x, y) = sqrt(x²+y²), computed as m·sqrt((x/m)²+(y/m)²) with
    // m = max(|x|,|y|) so the radicand stays in [1, 2]. Exact rational scaling;
    // reuses the grid-scaled sqrt_fixed.
    constexpr rational hypot_endpoint(rational x,
                                                   rational y) noexcept
    {
      rational ax = abs(x);
      rational ay = abs(y);
      rational m  = (ax > ay) ? ax : ay;
      if (m == rational{0}) return rational{0};
      // Form the radicand (x/m)²+(y/m)² ∈ [1, 2] at scale kRefBits — keeping it
      // a rational overflows imax (the squared numerators cross-multiply to
      // ~1e24). Each ratio is ≤ 1, so its fixed-point square fits comfortably.
      imax rx = to_fixed(x / m, kRefBits);
      imax ry = to_fixed(y / m, kRefBits);
      imax s_w = fmul(rx, rx, kRefBits) + fmul(ry, ry, kRefBits);
      const imax root_w = sqrt_fixed<kRefBits>(s_w);
      // Exact rational product when it fits; a wide-denominator m can push m·root
      // past imax, so fall back to the fixed-point form (|m| ≤ 2^20 envelope).
      if (auto exact = m * fixed_to_rational(root_w, kRefBits))
        return *exact;
      return fixed_to_rational(fmul(to_fixed(m, kRefBits), root_w, kRefBits),
                               kRefBits);
    }

    // pow(b, e) = 2^(e·log2(b)), b > 0. The exponent e·log2(b) is saturated into
    // exp2's [−30, 30] envelope so the power-of-two denominator never UB-shifts;
    // the runtime impl reports envelope overflow via `expected`.
    constexpr rational pow_endpoint(rational b,
                                                 rational e) noexcept
    {
      imax sc_w = fmul(to_fixed(e, kRefBits), log2_to_fixed<kRefBits>(b), kRefBits);
      constexpr imax lim = imax{30} << kRefBits;
      sc_w = (sc_w > lim) ? lim : (sc_w < -lim) ? -lim : sc_w;
      return exp2_from_fixed<kRefBits>(sc_w);
    }

    // --- deduction aliases ------------------------------------------------
    // Monotonic-increasing functions round endpoints outward like the exp/log
    // family. acos is decreasing; cosh is even (min at 0 if the interval
    // spans it). round_nearest lands sub-notch drift onto the grid.
    template <boundable In>
    using atan_auto_t = bound<{{floor_to_notch(atan_fixed<working_bits<In>()>(Lower<In>), Notch<In>),
                                ceil_to_notch (atan_fixed<working_bits<In>()>(Upper<In>), Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using asin_auto_t = bound<{{floor_to_notch(asin_endpoint(Lower<In>), Notch<In>),
                                ceil_to_notch (asin_endpoint(Upper<In>), Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using acos_auto_t = bound<{{floor_to_notch(acos_endpoint(Upper<In>), Notch<In>),
                                ceil_to_notch (acos_endpoint(Lower<In>), Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using sinh_auto_t = bound<{{floor_to_notch(sinh_endpoint(Lower<In>), Notch<In>),
                                ceil_to_notch (sinh_endpoint(Upper<In>), Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    inline constexpr rational cosh_auto_lo =
      (Lower<In> <= rational{0} && Upper<In> >= rational{0})
        ? rational{1}
        : (cosh_endpoint(Lower<In>) < cosh_endpoint(Upper<In>)
             ? cosh_endpoint(Lower<In>) : cosh_endpoint(Upper<In>));

    template <boundable In>
    inline constexpr rational cosh_auto_hi =
      (cosh_endpoint(Lower<In>) > cosh_endpoint(Upper<In>))
        ? cosh_endpoint(Lower<In>) : cosh_endpoint(Upper<In>);

    template <boundable In>
    using cosh_auto_t = bound<{{floor_to_notch(cosh_auto_lo<In>, Notch<In>),
                                ceil_to_notch (cosh_auto_hi<In>, Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using tanh_auto_t = bound<{{floor_to_notch(tanh_endpoint(Lower<In>), Notch<In>),
                                ceil_to_notch (tanh_endpoint(Upper<In>), Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using log10_auto_t = bound<{{floor_to_notch(log10_endpoint(Lower<In>), Notch<In>),
                                 ceil_to_notch (log10_endpoint(Upper<In>), Notch<In>)},
                                Notch<In>}, BoundPolicy<In> | round_nearest>;

    template <boundable In>
    using cbrt_auto_t = bound<{{floor_to_notch(cbrt_endpoint(Lower<In>), Notch<In>),
                                ceil_to_notch (cbrt_endpoint(Upper<In>), Notch<In>)},
                               Notch<In>}, BoundPolicy<In> | round_nearest>;

    // hypot output: non-negative, Upper at the largest-magnitude corner.
    template <boundable InX, boundable InY>
    inline constexpr rational hypot_auto_hi =
      hypot_endpoint(
        (abs(Lower<InX>) > abs(Upper<InX>)
           ? abs(Lower<InX>) : abs(Upper<InX>)),
        (abs(Lower<InY>) > abs(Upper<InY>)
           ? abs(Lower<InY>) : abs(Upper<InY>)));

    template <boundable InX, boundable InY>
    using hypot_auto_t = bound<{{rational{0},
                                 ceil_to_notch(hypot_auto_hi<InX, InY>, Notch<InX>)},
                                Notch<InX>}, BoundPolicy<InX> | round_nearest>;

    // pow output: extrema of b^e over the input rectangle occur at corners
    // (monotone in each argument for b > 0). Min and max of the 4 corners.
    template <boundable InB, boundable InE>
    inline constexpr rational pow_corner[4] = {
      pow_endpoint(Lower<InB>, Lower<InE>), pow_endpoint(Lower<InB>, Upper<InE>),
      pow_endpoint(Upper<InB>, Lower<InE>), pow_endpoint(Upper<InB>, Upper<InE>),
    };

    template <boundable InB, boundable InE>
    inline constexpr rational pow_auto_lo = []{
      rational m = pow_corner<InB, InE>[0];
      for (int i = 1; i < 4; ++i)
        if (pow_corner<InB, InE>[i] < m) m = pow_corner<InB, InE>[i];
      return m;
    }();

    template <boundable InB, boundable InE>
    inline constexpr rational pow_auto_hi = []{
      rational m = pow_corner<InB, InE>[0];
      for (int i = 1; i < 4; ++i)
        if (pow_corner<InB, InE>[i] > m) m = pow_corner<InB, InE>[i];
      return m;
    }();

    template <boundable InB, boundable InE>
    using pow_auto_t = bound<{{floor_to_notch(pow_auto_lo<InB, InE>, Notch<InB>),
                               ceil_to_notch (pow_auto_hi<InB, InE>, Notch<InB>)},
                              Notch<InB>}, BoundPolicy<InB> | round_nearest>;
  } // namespace detail

  // --- explicit-Out impls -------------------------------------------------
  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out atan_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "bnd::math::atan: input magnitudes must be \u2264 2^20 for the working-scale envelope");
    return detail::store_grid<Out>(detail::atan_fixed<detail::working_bits<Out>()>(bnd::detail::rational{x}));
  }

  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out asin_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -1 && Upper<In> <= 1,
                  "bnd::math::asin: input must be in [-1, 1]");
    return detail::store_grid<Out>(detail::asin_endpoint(bnd::detail::rational{x}));
  }

  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out acos_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -1 && Upper<In> <= 1,
                  "bnd::math::acos: input must be in [-1, 1]");
    return detail::store_grid<Out>(detail::acos_endpoint(bnd::detail::rational{x}));
  }

  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out sinh_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -10 && Upper<In> <= 10,
                  "bnd::math::sinh: input must be in [-10, 10]");
    return detail::store_grid<Out>(detail::sinh_endpoint(bnd::detail::rational{x}));
  }

  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out cosh_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -10 && Upper<In> <= 10,
                  "bnd::math::cosh: input must be in [-10, 10]");
    static_assert(Lower<Out> <= bnd::detail::rational{1},
                  "bnd::math::cosh: Out must include 1 (cosh ≥ 1)");
    return detail::store_grid<Out>(detail::cosh_endpoint(bnd::detail::rational{x}));
  }

  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out tanh_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -10 && Upper<In> <= 10,
                  "bnd::math::tanh: input must be in [-10, 10]");
    return detail::store_grid<Out>(detail::tanh_endpoint(bnd::detail::rational{x}));
  }

  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out log10_impl(In x) noexcept
  {
    static_assert(Lower<In> > 0,
                  "bnd::math::log10: input must be strictly positive");
    return detail::store_grid<Out>(detail::log10_endpoint(bnd::detail::rational{x}));
  }

  template <boundable Out, boundable In>
  [[nodiscard]] constexpr Out cbrt_impl(In x) noexcept
  {
    static_assert(Lower<In> >= -(imax{1} << 20) && Upper<In> <= (imax{1} << 20),
                  "bnd::math::cbrt: input magnitude must be ≤ 2^20 for the working-scale envelope");
    return detail::store_grid<Out>(detail::cbrt_endpoint(bnd::detail::rational{x}));
  }

  template <boundable Out, boundable InX, boundable InY>
  [[nodiscard]] constexpr Out hypot_impl(InX x, InY y) noexcept
  {
    static_assert(Lower<InX> >= -(imax{1} << 20) && Upper<InX> <= (imax{1} << 20)
               && Lower<InY> >= -(imax{1} << 20) && Upper<InY> <= (imax{1} << 20),
                  "bnd::math::hypot: input magnitudes must be ≤ 2^20 for the working-scale envelope");
    static_assert(Lower<Out> <= 0, "bnd::math::hypot: Out must include 0");
    return detail::store_grid<Out>(detail::hypot_endpoint(bnd::detail::rational{x}, bnd::detail::rational{y}));
  }

  // pow: b^e for runtime base b > 0. Returns `expected` — `overflow` when
  // e·log2(b) leaves exp2's [-30, 30] envelope or the result leaves Out's
  // interval. The auto form requires Lower<InB> > 0 (so b > 0 is guaranteed
  // and the output range is bounded for deduction).
  template <boundable Out, boundable InB, boundable InE>
  [[nodiscard]] constexpr slim::expected<Out, errc> pow_impl(InB base, InE exp) noexcept
  {
    bnd::detail::rational bv = base;
    if (bv <= bnd::detail::rational{0})
      return slim::unexpected(errc::domain_error);

    constexpr int W = detail::working_bits<Out>();
    imax sc_w = detail::fmul(detail::to_fixed(bnd::detail::rational{exp}, W),
                             detail::log2_to_fixed<W>(bv), W);     // e·log2(b), scale 2^W
    constexpr imax lim = imax{30} << W;
    if (sc_w > lim || sc_w < -lim)
      return slim::unexpected(errc::overflow);

    bnd::detail::rational r = detail::exp2_from_fixed<W>(sc_w);
    if constexpr (!has_flag(BoundPolicy<Out>, clamp))   // clamp Out: saturate below
      if (r < Lower<Out> || r > Upper<Out>)
        return slim::unexpected(errc::overflow);
    return detail::store_grid<Out>(r);
  }

  // --- public auto-deducing forms ----------------------------------------
  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto atan(In x) noexcept
  {
    static_assert(detail::require_snap<In>());
#if defined(BND_MATH_NO_FP)
    return atan_impl<detail::atan_auto_t<In>>(x);
#elif defined(BND_MATH_FLOAT)
    return flt::atan_core<detail::atan_auto_t<In>>(x);
#else
    return dbl::atan_core<detail::atan_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto asin(In x) noexcept
  {
    static_assert(detail::require_snap<In>());
#if defined(BND_MATH_NO_FP)
    return asin_impl<detail::asin_auto_t<In>>(x);
#elif defined(BND_MATH_FLOAT)
    return flt::asin_core<detail::asin_auto_t<In>>(x);
#else
    return dbl::asin_core<detail::asin_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto acos(In x) noexcept
  {
    static_assert(detail::require_snap<In>());
#if defined(BND_MATH_NO_FP)
    return acos_impl<detail::acos_auto_t<In>>(x);
#elif defined(BND_MATH_FLOAT)
    return flt::acos_core<detail::acos_auto_t<In>>(x);
#else
    return dbl::acos_core<detail::acos_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto sinh(In x) noexcept
  {
    static_assert(detail::require_snap<In>());
#if defined(BND_MATH_NO_FP)
    return sinh_impl<detail::sinh_auto_t<In>>(x);
#elif defined(BND_MATH_FLOAT)
    return flt::sinh_core<detail::sinh_auto_t<In>>(x);
#else
    return dbl::sinh_core<detail::sinh_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto cosh(In x) noexcept
  {
    static_assert(detail::require_snap<In>());
#if defined(BND_MATH_NO_FP)
    return cosh_impl<detail::cosh_auto_t<In>>(x);
#elif defined(BND_MATH_FLOAT)
    return flt::cosh_core<detail::cosh_auto_t<In>>(x);
#else
    return dbl::cosh_core<detail::cosh_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto tanh(In x) noexcept
  {
    static_assert(detail::require_snap<In>());
#if defined(BND_MATH_NO_FP)
    return tanh_impl<detail::tanh_auto_t<In>>(x);
#elif defined(BND_MATH_FLOAT)
    return flt::tanh_core<detail::tanh_auto_t<In>>(x);
#else
    return dbl::tanh_core<detail::tanh_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto log10(In x) noexcept
  {
    static_assert(detail::require_snap<In>());
    static_assert(Lower<In> > 0, "bnd::math::log10: input must be strictly positive");
#if defined(BND_MATH_NO_FP)
    return log10_impl<detail::log10_auto_t<In>>(x);
#elif defined(BND_MATH_FLOAT)
    return flt::log10_core<detail::log10_auto_t<In>>(x);
#else
    return dbl::log10_core<detail::log10_auto_t<In>>(x);
#endif
  }

  template <boundable In>
  [[nodiscard]] BND_MATH_FN auto cbrt(In x) noexcept
  {
    static_assert(detail::require_snap<In>());
#if defined(BND_MATH_NO_FP)
    return cbrt_impl<detail::cbrt_auto_t<In>>(x);
#elif defined(BND_MATH_FLOAT)
    return flt::cbrt_core<detail::cbrt_auto_t<In>>(x);
#else
    return dbl::cbrt_core<detail::cbrt_auto_t<In>>(x);
#endif
  }

  template <boundable InX, boundable InY>
  [[nodiscard]] BND_MATH_FN auto hypot(InX x, InY y) noexcept
  {
    static_assert(detail::require_snap<InX>() && detail::require_snap<InY>());
#if defined(BND_MATH_NO_FP)
    return hypot_impl<detail::hypot_auto_t<InX, InY>>(x, y);
#elif defined(BND_MATH_FLOAT)
    return flt::hypot_core<detail::hypot_auto_t<InX, InY>>(x, y);
#else
    return dbl::hypot_core<detail::hypot_auto_t<InX, InY>>(x, y);
#endif
  }

  template <boundable InB, boundable InE>
    requires (Lower<InB> > bnd::detail::rational{0})
  [[nodiscard]] BND_MATH_FN auto pow(InB base, InE exp) noexcept
  {
    static_assert(detail::require_snap<InB>() && detail::require_snap<InE>());
    using Out = detail::pow_auto_t<InB, InE>;
#if defined(BND_MATH_NO_FP)
    return pow_impl<Out>(base, exp);
#elif defined(BND_MATH_FLOAT)
    float b = static_cast<float>(static_cast<double>(base));
    if (b <= 0.0f)
      return slim::expected<Out, errc>{slim::unexpected(errc::domain_error)};
    float r = flt::detail::d_pow(b, static_cast<float>(static_cast<double>(exp)));
    if constexpr (!has_flag(BoundPolicy<Out>, clamp))   // clamp Out: saturate below
      if (r < static_cast<float>(static_cast<double>(Lower<Out>)) || r > static_cast<float>(static_cast<double>(Upper<Out>)))
        return slim::expected<Out, errc>{slim::unexpected(errc::overflow)};
    return slim::expected<Out, errc>{flt::store<Out>(r)};
#else
    double b = static_cast<double>(base);
    if (b <= 0.0)
      return slim::expected<Out, errc>{slim::unexpected(errc::domain_error)};
    double r = dbl::detail::d_pow(b, static_cast<double>(exp));
    if constexpr (!has_flag(BoundPolicy<Out>, clamp))   // clamp Out: saturate below
      if (r < static_cast<double>(Lower<Out>) || r > static_cast<double>(Upper<Out>))
        return slim::expected<Out, errc>{slim::unexpected(errc::overflow)};
    return slim::expected<Out, errc>{dbl::store<Out>(r)};
#endif
  }

  //===========================================================================
  // Explicit engine namespaces — call a chosen engine regardless of the build
  // default. `cordic::fn` (integer/CORDIC, ALWAYS present) and `dbl::fn` (the
  // double engine, present unless BND_MATH_NO_FP) expose the SAME public-shaped
  // API as the top-level `bnd::math::fn` — same signatures, domains, auto-deduced
  // output grids, and domain static_asserts. The unqualified `bnd::math::fn` is
  // an alias for whichever engine the build selects (see the #ifdef dispatch
  // above); these let a single binary mix both — e.g. `cordic::sin` on a
  // determinism-critical path and `dbl::sin` on a hot one.
  //
  // The engines are independent approximations: a grid-snapped value can differ
  // between them by up to one notch on rounding ties (table-maker's dilemma).
  // Don't compare outputs across engines — see determinism.md.
  //===========================================================================
  namespace cordic
  {
    template <boundable In>
      requires (Lower<In> == bnd::detail::rational{0})
    [[nodiscard]] constexpr auto sqrt(In x) noexcept
    { static_assert(detail::require_snap<In>()); return sqrt_impl<detail::sqrt_auto_t<In>>(x); }

    template <boundable In>
      requires (Lower<In> < bnd::detail::rational{0})
    [[nodiscard]] constexpr auto sqrt(In x) noexcept
    { static_assert(detail::require_snap<In>()); return sqrt_signed_impl<detail::sqrt_signed_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] constexpr auto exp2(In x) noexcept
    { static_assert(detail::require_snap<In>()); return exp2_impl<detail::exp2_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] constexpr auto log2(In x) noexcept
    {
      static_assert(detail::require_snap<In>());
      static_assert(Lower<In> > 0, "bnd::math::cordic::log2: input must be strictly positive");
      return log2_impl<detail::log2_auto_t<In>>(x);
    }

    template <boundable In>
    [[nodiscard]] constexpr auto exp(In x) noexcept
    { static_assert(detail::require_snap<In>()); return exp_impl<detail::exp_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] constexpr auto log(In x) noexcept
    {
      static_assert(detail::require_snap<In>());
      static_assert(Lower<In> > 0, "bnd::math::cordic::log: input must be strictly positive");
      return log_impl<detail::log_auto_t<In>>(x);
    }

    template <imax Base, boundable In>
    [[nodiscard]] constexpr auto pow_base(In x) noexcept
    { static_assert(detail::require_snap<In>()); return pow_base_impl<Base, detail::pow_base_auto_t<Base, In>>(x); }

    template <boundable In>
    [[nodiscard]] constexpr auto sin(In angle) noexcept
    { static_assert(detail::require_snap<In>()); return sin_impl<detail::sin_auto_t<In>>(angle); }

    template <boundable In>
    [[nodiscard]] constexpr auto cos(In angle) noexcept
    { static_assert(detail::require_snap<In>()); return cos_impl<detail::cos_auto_t<In>>(angle); }

    template <boundable In>
    [[nodiscard]] constexpr auto tan(In angle) noexcept
    { static_assert(detail::require_snap<In>()); return tan_impl<detail::tan_auto_t<In>>(angle); }

    template <boundable In>
    [[nodiscard]] constexpr auto atan2(In y, In x) noexcept
    { static_assert(detail::require_snap<In>()); return atan2_impl<detail::atan2_auto_t<In>>(y, x); }

    template <boundable In>
    [[nodiscard]] constexpr auto atan(In x) noexcept
    { static_assert(detail::require_snap<In>()); return atan_impl<detail::atan_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] constexpr auto asin(In x) noexcept
    { static_assert(detail::require_snap<In>()); return asin_impl<detail::asin_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] constexpr auto acos(In x) noexcept
    { static_assert(detail::require_snap<In>()); return acos_impl<detail::acos_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] constexpr auto sinh(In x) noexcept
    { static_assert(detail::require_snap<In>()); return sinh_impl<detail::sinh_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] constexpr auto cosh(In x) noexcept
    { static_assert(detail::require_snap<In>()); return cosh_impl<detail::cosh_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] constexpr auto tanh(In x) noexcept
    { static_assert(detail::require_snap<In>()); return tanh_impl<detail::tanh_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] constexpr auto log10(In x) noexcept
    {
      static_assert(detail::require_snap<In>());
      static_assert(Lower<In> > 0, "bnd::math::cordic::log10: input must be strictly positive");
      return log10_impl<detail::log10_auto_t<In>>(x);
    }

    template <boundable In>
    [[nodiscard]] constexpr auto cbrt(In x) noexcept
    { static_assert(detail::require_snap<In>()); return cbrt_impl<detail::cbrt_auto_t<In>>(x); }

    template <boundable InX, boundable InY>
    [[nodiscard]] constexpr auto hypot(InX x, InY y) noexcept
    {
      static_assert(detail::require_snap<InX>() && detail::require_snap<InY>());
      return hypot_impl<detail::hypot_auto_t<InX, InY>>(x, y);
    }

    template <boundable InB, boundable InE>
      requires (Lower<InB> > bnd::detail::rational{0})
    [[nodiscard]] constexpr auto pow(InB base, InE exp) noexcept
    {
      static_assert(detail::require_snap<InB>() && detail::require_snap<InE>());
      return pow_impl<detail::pow_auto_t<InB, InE>>(base, exp);
    }
  } // namespace cordic

#ifndef BND_MATH_NO_FP
  namespace dbl
  {
    // Public-shaped double-engine entry points. `detail::` here would resolve to
    // bnd::math::dbl::detail (the engine cores), so the shared deduction/helpers
    // are qualified as `bnd::math::detail::`; the `*_core`/`store`/`d_*` names are
    // this namespace's own. Absent under BND_MATH_NO_FP (no FP, no <cmath>).
    template <boundable In>
      requires (Lower<In> == bnd::detail::rational{0})
    [[nodiscard]] BND_DBL_FN auto sqrt(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return sqrt_core<bnd::math::detail::sqrt_auto_t<In>>(x); }

    template <boundable In>
      requires (Lower<In> < bnd::detail::rational{0})
    [[nodiscard]] BND_DBL_FN auto sqrt(In x) noexcept
    {
      static_assert(bnd::math::detail::require_snap<In>());
      using Out = bnd::math::detail::sqrt_signed_auto_t<In>;
      double v = static_cast<double>(x);
      if (v < 0.0)
        return slim::expected<Out, errc>{slim::unexpected(errc::domain_error)};
      return slim::expected<Out, errc>{store<Out>(detail::d_sqrt(v))};
    }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto exp2(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return exp2_core<bnd::math::detail::exp2_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto log2(In x) noexcept
    {
      static_assert(bnd::math::detail::require_snap<In>());
      static_assert(Lower<In> > 0, "bnd::math::dbl::log2: input must be strictly positive");
      return log2_core<bnd::math::detail::log2_auto_t<In>>(x);
    }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto exp(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return exp_core<bnd::math::detail::exp_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto log(In x) noexcept
    {
      static_assert(bnd::math::detail::require_snap<In>());
      static_assert(Lower<In> > 0, "bnd::math::dbl::log: input must be strictly positive");
      return log_core<bnd::math::detail::log_auto_t<In>>(x);
    }

    template <imax Base, boundable In>
    [[nodiscard]] BND_DBL_FN auto pow_base(In x) noexcept
    {
      static_assert(bnd::math::detail::require_snap<In>());
      using Out = bnd::math::detail::pow_base_auto_t<Base, In>;
      return store<Out>(detail::d_pow(static_cast<double>(Base), static_cast<double>(x)));
    }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto sin(In angle) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return sin_core<bnd::math::detail::sin_auto_t<In>>(angle); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto cos(In angle) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return cos_core<bnd::math::detail::cos_auto_t<In>>(angle); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto tan(In angle) noexcept
    {
      static_assert(bnd::math::detail::require_snap<In>());
      using Out = bnd::math::detail::tan_auto_t<In>;
      double x = static_cast<double>(angle);
      double c = detail::d_cos(x);
      if (c == 0.0)
        return slim::expected<Out, errc>{slim::unexpected(errc::division_by_zero)};
      double t = detail::d_sin(x) / c;
      if constexpr (!has_flag(BoundPolicy<Out>, clamp))   // clamp Out: saturate below
        if (t < static_cast<double>(Lower<Out>) || t > static_cast<double>(Upper<Out>))
          return slim::expected<Out, errc>{slim::unexpected(errc::overflow)};
      return slim::expected<Out, errc>{store<Out>(t)};
    }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto atan2(In y, In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return atan2_core<bnd::math::detail::atan2_auto_t<In>>(y, x); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto atan(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return atan_core<bnd::math::detail::atan_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto asin(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return asin_core<bnd::math::detail::asin_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto acos(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return acos_core<bnd::math::detail::acos_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto sinh(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return sinh_core<bnd::math::detail::sinh_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto cosh(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return cosh_core<bnd::math::detail::cosh_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto tanh(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return tanh_core<bnd::math::detail::tanh_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto log10(In x) noexcept
    {
      static_assert(bnd::math::detail::require_snap<In>());
      static_assert(Lower<In> > 0, "bnd::math::dbl::log10: input must be strictly positive");
      return log10_core<bnd::math::detail::log10_auto_t<In>>(x);
    }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto cbrt(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return cbrt_core<bnd::math::detail::cbrt_auto_t<In>>(x); }

    template <boundable InX, boundable InY>
    [[nodiscard]] BND_DBL_FN auto hypot(InX x, InY y) noexcept
    {
      static_assert(bnd::math::detail::require_snap<InX>() && bnd::math::detail::require_snap<InY>());
      return hypot_core<bnd::math::detail::hypot_auto_t<InX, InY>>(x, y);
    }

    template <boundable InB, boundable InE>
      requires (Lower<InB> > bnd::detail::rational{0})
    [[nodiscard]] BND_DBL_FN auto pow(InB base, InE exp) noexcept
    {
      static_assert(bnd::math::detail::require_snap<InB>() && bnd::math::detail::require_snap<InE>());
      using Out = bnd::math::detail::pow_auto_t<InB, InE>;
      double b = static_cast<double>(base);
      if (b <= 0.0)
        return slim::expected<Out, errc>{slim::unexpected(errc::domain_error)};
      double r = detail::d_pow(b, static_cast<double>(exp));
      if constexpr (!has_flag(BoundPolicy<Out>, clamp))   // clamp Out: saturate below
        if (r < static_cast<double>(Lower<Out>) || r > static_cast<double>(Upper<Out>))
          return slim::expected<Out, errc>{slim::unexpected(errc::overflow)};
      return slim::expected<Out, errc>{store<Out>(r)};
    }
  } // namespace dbl

  namespace flt
  {
    // Public-shaped float-engine entry points — binary32 compute, same shapes as
    // dbl:: (qualify shared helpers as bnd::math::detail::; *_core/store/d_* are
    // this namespace's own). A third value set (float ≠ double ≠ cordic).
    template <boundable In>
      requires (Lower<In> == bnd::detail::rational{0})
    [[nodiscard]] BND_DBL_FN auto sqrt(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return sqrt_core<bnd::math::detail::sqrt_auto_t<In>>(x); }

    template <boundable In>
      requires (Lower<In> < bnd::detail::rational{0})
    [[nodiscard]] BND_DBL_FN auto sqrt(In x) noexcept
    {
      static_assert(bnd::math::detail::require_snap<In>());
      using Out = bnd::math::detail::sqrt_signed_auto_t<In>;
      float v = static_cast<float>(static_cast<double>(x));
      if (v < 0.0f)
        return slim::expected<Out, errc>{slim::unexpected(errc::domain_error)};
      return slim::expected<Out, errc>{store<Out>(detail::d_sqrt(v))};
    }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto exp2(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return exp2_core<bnd::math::detail::exp2_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto log2(In x) noexcept
    {
      static_assert(bnd::math::detail::require_snap<In>());
      static_assert(Lower<In> > 0, "bnd::math::flt::log2: input must be strictly positive");
      return log2_core<bnd::math::detail::log2_auto_t<In>>(x);
    }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto exp(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return exp_core<bnd::math::detail::exp_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto log(In x) noexcept
    {
      static_assert(bnd::math::detail::require_snap<In>());
      static_assert(Lower<In> > 0, "bnd::math::flt::log: input must be strictly positive");
      return log_core<bnd::math::detail::log_auto_t<In>>(x);
    }

    template <imax Base, boundable In>
    [[nodiscard]] BND_DBL_FN auto pow_base(In x) noexcept
    {
      static_assert(bnd::math::detail::require_snap<In>());
      using Out = bnd::math::detail::pow_base_auto_t<Base, In>;
      return store<Out>(detail::d_pow(static_cast<float>(Base), static_cast<float>(static_cast<double>(x))));
    }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto sin(In angle) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return sin_core<bnd::math::detail::sin_auto_t<In>>(angle); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto cos(In angle) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return cos_core<bnd::math::detail::cos_auto_t<In>>(angle); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto tan(In angle) noexcept
    {
      static_assert(bnd::math::detail::require_snap<In>());
      using Out = bnd::math::detail::tan_auto_t<In>;
      float x = static_cast<float>(static_cast<double>(angle));
      float c = detail::d_cos(x);
      if (c == 0.0f)
        return slim::expected<Out, errc>{slim::unexpected(errc::division_by_zero)};
      float t = detail::d_sin(x) / c;
      if constexpr (!has_flag(BoundPolicy<Out>, clamp))   // clamp Out: saturate below
        if (t < static_cast<float>(static_cast<double>(Lower<Out>)) || t > static_cast<float>(static_cast<double>(Upper<Out>)))
          return slim::expected<Out, errc>{slim::unexpected(errc::overflow)};
      return slim::expected<Out, errc>{store<Out>(t)};
    }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto atan2(In y, In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return atan2_core<bnd::math::detail::atan2_auto_t<In>>(y, x); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto atan(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return atan_core<bnd::math::detail::atan_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto asin(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return asin_core<bnd::math::detail::asin_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto acos(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return acos_core<bnd::math::detail::acos_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto sinh(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return sinh_core<bnd::math::detail::sinh_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto cosh(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return cosh_core<bnd::math::detail::cosh_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto tanh(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return tanh_core<bnd::math::detail::tanh_auto_t<In>>(x); }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto log10(In x) noexcept
    {
      static_assert(bnd::math::detail::require_snap<In>());
      static_assert(Lower<In> > 0, "bnd::math::flt::log10: input must be strictly positive");
      return log10_core<bnd::math::detail::log10_auto_t<In>>(x);
    }

    template <boundable In>
    [[nodiscard]] BND_DBL_FN auto cbrt(In x) noexcept
    { static_assert(bnd::math::detail::require_snap<In>()); return cbrt_core<bnd::math::detail::cbrt_auto_t<In>>(x); }

    template <boundable InX, boundable InY>
    [[nodiscard]] BND_DBL_FN auto hypot(InX x, InY y) noexcept
    {
      static_assert(bnd::math::detail::require_snap<InX>() && bnd::math::detail::require_snap<InY>());
      return hypot_core<bnd::math::detail::hypot_auto_t<InX, InY>>(x, y);
    }

    template <boundable InB, boundable InE>
      requires (Lower<InB> > bnd::detail::rational{0})
    [[nodiscard]] BND_DBL_FN auto pow(InB base, InE exp) noexcept
    {
      static_assert(bnd::math::detail::require_snap<InB>() && bnd::math::detail::require_snap<InE>());
      using Out = bnd::math::detail::pow_auto_t<InB, InE>;
      float b = static_cast<float>(static_cast<double>(base));
      if (b <= 0.0f)
        return slim::expected<Out, errc>{slim::unexpected(errc::domain_error)};
      float r = detail::d_pow(b, static_cast<float>(static_cast<double>(exp)));
      if constexpr (!has_flag(BoundPolicy<Out>, clamp))   // clamp Out: saturate below
        if (r < static_cast<float>(static_cast<double>(Lower<Out>)) || r > static_cast<float>(static_cast<double>(Upper<Out>)))
          return slim::expected<Out, errc>{slim::unexpected(errc::overflow)};
      return slim::expected<Out, errc>{store<Out>(r)};
    }
  } // namespace flt
#endif // !BND_MATH_NO_FP
}


// ======================================================================
//  bound/formats.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// formats — predefined `bound` aliases mapping to hardware byte widths, so you
// can write `bnd::u8` / `bnd::unorm16` / `bnd::q8_8` directly.
//
// Reserved-top tradeoff: each storage type's extreme value is a sentinel slot
// (zero-overhead slim::optional<bound>), chosen with a strict `<` margin — so a
// full-width range like {0,255} would promote to uint16. To stay at native
// width these aliases stop one short: `u8` is [0, 254]. Q-format types already
// have headroom, so they keep full range with power-of-two notches.
//
// These default to `checked`; for wraparound/saturation declare your own (e.g.
// `bound<{0,254}, wrap>`).
//---------------------------------------------------------------------------
namespace bnd
{
  //-------------------------------------------------------------------------
  // Native integer widths — direct storage (Raw == value), `checked`.
  // Range is the native width minus the one reserved sentinel value.
  //-------------------------------------------------------------------------
  using u8  = bound<{0, 254}>;                          // uint8
  using u16 = bound<{0, 65534}>;                        // uint16
  using u32 = bound<{0, 4294967294}>;                   // uint32
  // u64 is intentionally absent: the library's internal value path is `imax`
  // (int64) — `to_value` returns `imax` — so unsigned values above 2^63-1
  // cannot round-trip through arithmetic/compare. Use `i64` or a hand-rolled
  // grid if you need 64-bit storage.

  using i8  = bound<{-127, 127}>;                        // int8
  using i16 = bound<{-32767, 32767}>;                    // int16
  using i32 = bound<{-2147483647, 2147483647}>;          // int32
  using i64 = bound<{-9223372036854775807, 9223372036854775807}>; // int64

  //-------------------------------------------------------------------------
  // Unsigned normalized (UNORM) — [0, 1] at N-bit resolution, `round_nearest`.
  // The notch denominator is one short of the type max so the index fits the
  // native width; both endpoints (0 and 1) are exactly representable.
  //-------------------------------------------------------------------------
  using unorm8  = bound<{{0, 1}, notch<1, 254>},        round_nearest>; // uint8
  using unorm16 = bound<{{0, 1}, notch<1, 65534>},      round_nearest>; // uint16
  using unorm32 = bound<{{0, 1}, notch<1, 4294967294>}, round_nearest>; // uint32

  //-------------------------------------------------------------------------
  // Q-format fixed-point — unsigned integer.fraction, power-of-two notch,
  // full natural range (already fits with headroom). `round_nearest`.
  //-------------------------------------------------------------------------
  using q4_4   = bound<{{0, 15},    notch<1, 16>},    round_nearest>; // uint8
  using q8_8   = bound<{{0, 255},   notch<1, 256>},   round_nearest>; // uint16
  using q16_16 = bound<{{0, 65535}, notch<1, 65536>}, round_nearest>; // uint32

  //-------------------------------------------------------------------------
  // Counters — a counter is a bound over [0, Max] whose overflow policy says
  // what `++` does at the ceiling (the boundary behavior is in the type).
  //-------------------------------------------------------------------------
  // Saturating counter: `++` caps at Max (never throws or wraps) — the honest
  // "count up to a ceiling / ≥Max" tally. `--` saturates at 0.
  template <umax Max> using counter      = bound<{0, Max}, clamp>;
  // Modular / ring counter: `++` wraps Max → 0 (sequence numbers, epochs).
  template <umax Max> using ring_counter = bound<{0, Max}, wrap>;

} // namespace bnd


#ifndef BND_NO_STRING

// ======================================================================
//  bound/io.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// io — ALL of the library's string / stream / std::format support, gathered
// into one opt-in header. The core (bound.hpp and everything it pulls) never
// includes this, so a freestanding / bare-metal build that never includes
// "bound/io.hpp" pays zero <string>/<ostream>/<format> cost. In the single-
// header amalgamation this whole region is wrapped in `#ifndef BND_NO_STRING`,
// so defining BND_NO_STRING drops it (and its heavy includes) wholesale.
//
// Provides:
//   * to_string(rational | interval | grid | boundable | arithmetic)
//   * to_string_debug(boundable)        — value + raw + raw-type + grid
//   * operator<<(std::ostream&, ...)
//   * std::formatter<bound<G,P>> / std::formatter<rational>   (when <format>)
//---------------------------------------------------------------------------


#include <string>
#include <string_view>
#include <ostream>
#include <version>

namespace bnd
{
  //-------------------------------------------------------------------------
  // to_string — pretty-prints `rational`, `interval`, `grid`, plus a fallback
  // for plain arithmetic types and the exact-rational form for boundables.
  //-------------------------------------------------------------------------
  inline std::string to_string(bnd::detail::rational r)
  {
    std::string str;
    if (r.Denominator < 0)
      str = "-";

    umax ad = detail::abs_den(r.Denominator);
    if (ad == 1)
      return str += std::to_string(r.Numerator);

    // power-of-2 or power-of-10: decimal output
    // find smallest 10^k divisible by ad
    umax pow10 = 1;
    unsigned digits = 0;
    bool is_decimal = false;
    for (unsigned k = 0; k < 20; ++k)
    {
      if (pow10 % ad == 0)
      { is_decimal = true; digits = k; break; }
      pow10 *= 10;
    }

    if (is_decimal)
    {
      umax scale = pow10 / ad;
      umax total;
      if (!mul_overflow(r.Numerator, scale, &total))
      {
        umax int_part = total / pow10;
        umax frac_part = total % pow10;
        str += std::to_string(int_part);
        if (digits > 0)
        {
          str += ".";
          auto frac_str = std::to_string(frac_part);
          // zero-pad
          for (unsigned i = 0; i < digits - frac_str.size(); ++i)
            str += "0";
          str += frac_str;
        }
        return str;
      }
      // Decimal expansion would overflow the umax scratch buffer. Fall back
      // silently to the mixed-number/fraction form — `to_string` must always
      // produce *some* readable output, never an error.
    }

    // mixed number for improper fractions
    umax int_part = r.Numerator / ad;
    umax remainder = r.Numerator % ad;
    if (int_part > 0)
    {
      str += std::to_string(int_part);
      if (remainder > 0)
      {
        str += " ";
        str += std::to_string(remainder);
        str += "/";
        str += std::to_string(ad);
      }
    }
    else
    {
      str += std::to_string(r.Numerator);
      str += "/";
      str += std::to_string(ad);
    }
    return str;
  }

  inline std::string to_string(interval ival)
  {
    std::string str{"["};

    str += bnd::to_string(ival.Lower);
    str += "..";
    str += bnd::to_string(ival.Upper);
    str += "]";
    return str;
  }

  inline std::string to_string(grid g)
  {
    std::string str{"{"};

    str += bnd::to_string(g.Interval);
    str += ", ";
    str += bnd::to_string(g.Notch);
    str += "}";
    return str;
  }

  // delegate to std::to_string
  template <typename V>
  auto to_string(V value)
  { return std::to_string(value); }

  // `real` (double-backed) and `exact` (rational-backed) bounds: render the
  // exact rational form. (Without this overload a real bound would fall to the
  // generic `std::to_string(double)` and print a lossy 6-digit form, and a
  // rational-raw bound has no std::to_string at all.) A continuous (Notch == 0)
  // real bound prints the double.
  template <boundable B>
    requires (detail::fp_raw<B> || detail::rational_raw<B>)
  inline std::string to_string(B b)
  {
    if constexpr (detail::fp_raw<B> && Notch<B> == bnd::detail::rational{0})
      return std::to_string(detail::as_double(b));
    else
      return to_string(bnd::detail::as_rational(b));
  }

  //-------------------------------------------------------------------------
  // type_name<T>() — short raw-type label for to_string_debug. Lives here (not
  // in the core math header) so the core never pulls <string_view>.
  //-------------------------------------------------------------------------
  namespace detail
  {
    template <typename T>
    constexpr std::string_view type_name()
    {
      if constexpr (std::is_same_v<T, std::uint8_t>)  return "uint8_t";
      if constexpr (std::is_same_v<T, std::uint16_t>) return "uint16_t";
      if constexpr (std::is_same_v<T, std::uint32_t>) return "uint32_t";
      if constexpr (std::is_same_v<T, std::uint64_t>) return "uint64_t";
      if constexpr (std::is_same_v<T, std::int8_t>)   return "int8_t";
      if constexpr (std::is_same_v<T, std::int16_t>)  return "int16_t";
      if constexpr (std::is_same_v<T, std::int32_t>)  return "int32_t";
      if constexpr (std::is_same_v<T, std::int64_t>)  return "int64_t";
      if constexpr (std::is_same_v<T, rational>) return "rational";
      return "unknown";
    }
  } // namespace detail

  //-------------------------------------------------------------------------
  // to_string / to_string_debug / operator<< for boundables and rational.
  // The debug form also prints the raw value, raw type, and grid — useful when
  // inspecting failing tests or storage choices.
  //-------------------------------------------------------------------------
  template <boundable B>
  inline std::string to_string(B b)
  { return bnd::to_string(detail::as_rational(b)); }

  template <boundable B>
  inline std::string to_string_debug(B b)
  {
    std::string str;
    str += bnd::to_string(detail::as_rational(b));
    str += " {";
    str += bnd::to_string(+b.raw());
    str += "[" + std::string(detail::type_name<detail::raw_t<B>>());
    str += " Max:" + bnd::to_string(+detail::NotchCount<B>) + "] ";
    str += bnd::to_string(Grid<B>);
    str += "}";
    return str;
  }

  inline std::ostream& operator<<(std::ostream& stream, bnd::detail::rational r)
  {
    stream << bnd::to_string(r);
    return stream;
  }

  template <boundable B>
  inline std::ostream& operator<<(std::ostream& stream, B b)
  {
    stream << bnd::to_string(b);
    return stream;
  }

} // namespace bnd

//---------------------------------------------------------------------------
// std::format integration is gated on a working <format> (libstdc++ ships it
// from GCC 13; GCC 12 / C++20 builds compile this as a no-op and rely on
// to_string()/operator<< instead).
//---------------------------------------------------------------------------
#ifdef __cpp_lib_format

#include <format>
#include <type_traits>

namespace bnd::detail
{
  // Shared spec handling: an empty `{}` is left to the derived format() (exact
  // to_string); a non-empty spec is parsed and applied by `numeric_`.
  template <class Inner>
  struct numeric_spec_formatter
  {
    Inner numeric_{};
    bool  has_spec_ = false;

    constexpr auto parse(std::format_parse_context& ctx)
    {
      auto it = ctx.begin();
      if (it != ctx.end() && *it != '}')
      {
        has_spec_ = true;
        return numeric_.parse(ctx);
      }
      return it;
    }
  };
}

template <bnd::grid G, bnd::policy_flag P>
struct std::formatter<bnd::bound<G, P>>
  : bnd::detail::numeric_spec_formatter<
      std::conditional_t<bnd::detail::IsIntegerAligned<bnd::bound<G, P>>,
                         std::formatter<bnd::imax>,
                         std::formatter<double>>>
{
  using B = bnd::bound<G, P>;
  static constexpr bool integer_path = bnd::detail::IsIntegerAligned<B>;

  template <typename Ctx>
  auto format(B const& b, Ctx& ctx) const
  {
    if constexpr (integer_path)
      return this->numeric_.format(bnd::detail::to_value(b), ctx);
    else if (this->has_spec_)
      return this->numeric_.format(
          static_cast<double>(bnd::detail::rational{b}), ctx);
    else
      return std::format_to(ctx.out(), "{}", bnd::to_string(b));
  }
};

template <>
struct std::formatter<bnd::detail::rational>
  : bnd::detail::numeric_spec_formatter<std::formatter<double>>
{
  template <typename Ctx>
  auto format(bnd::detail::rational const& r, Ctx& ctx) const
  {
    if (has_spec_)
      return numeric_.format(static_cast<double>(r), ctx);
    return std::format_to(ctx.out(), "{}", bnd::to_string(r));
  }
};

#endif // __cpp_lib_format


#endif // BND_NO_STRING

// ======================================================================
//  bound/numeric_limits.hpp
// ======================================================================
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// numeric_limits / hash — std:: specialisations for bound<G, P>. numeric_limits
// reports the *grid* bounds (Lower/Upper), not the raw type's limits. std::hash
// hashes the Raw member (rational raw: Numerator+Denominator, boost-style combine).
//---------------------------------------------------------------------------

template <bnd::grid G, bnd::policy_flag P>
struct std::numeric_limits<bnd::bound<G, P>>
{
  using B = bnd::bound<G, P>;

  static constexpr bool is_specialized = true;
  static constexpr bool is_signed      = (G.Interval.Lower < bnd::detail::rational{0});
  static constexpr bool is_integer     = (G.Notch == bnd::detail::rational{1});
  static constexpr bool is_exact       = true;     // rational + integer raw are both exact
  static constexpr bool is_bounded     = true;
  static constexpr bool is_modulo      = (P & bnd::wrap) != 0;
  static constexpr bool has_infinity   = false;
  static constexpr bool has_quiet_NaN  = false;
  static constexpr bool has_signaling_NaN = false;
  static constexpr bool traps          = (P & bnd::checked) != 0;
  static constexpr bool is_iec559      = false;
  static constexpr int  radix          = 2;
  static constexpr std::float_round_style round_style =
      ((P & bnd::round_nearest) == bnd::round_nearest) ? std::round_to_nearest
                                                       : std::round_toward_zero;

  // digits / digits10 forward to the raw type so generic algorithms see the
  // storage size, not the rational interval count.
  static constexpr int digits   = std::numeric_limits<bnd::detail::raw_t<B>>::digits;
  static constexpr int digits10 = std::numeric_limits<bnd::detail::raw_t<B>>::digits10;

  static constexpr B min()    noexcept { return B{G.Interval.Lower}; }
  static constexpr B max()    noexcept { return B{G.Interval.Upper}; }
  static constexpr B lowest() noexcept { return B{G.Interval.Lower}; }
  // Exact types have no rounding noise — epsilon and round_error are 0 when
  // 0 is on the grid (it always is when 0 ∈ interval, since the grid is
  // validated such that Lower is an integer multiple of Notch). When 0 is
  // outside the interval, fall back to the grid minimum — the closest
  // representable stand-in for "no error" the type can express.
  static constexpr B epsilon() noexcept
  {
    if constexpr (G.Interval.Lower <= bnd::detail::rational{0}
               && bnd::detail::rational{0} <= G.Interval.Upper)
      return B{bnd::detail::rational{0}};
    else
      return B{G.Interval.Lower};
  }
  static constexpr B round_error() noexcept { return epsilon(); }
};

template <bnd::grid G, bnd::policy_flag P>
struct std::hash<bnd::bound<G, P>>
{
  using B = bnd::bound<G, P>;

  constexpr std::size_t operator()(B const& b) const noexcept
  {
    if constexpr (bnd::detail::rational_raw<B>)
    {
      // Boost-style hash combine over (Numerator, Denominator).
      auto h1 = std::hash<bnd::umax>{}(b.raw().Numerator);
      auto h2 = std::hash<bnd::imax>{}(b.raw().Denominator);
      return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
    else
      return std::hash<bnd::detail::raw_t<B>>{}(b.raw());
  }
};


#endif // BND_SINGLE_HEADER_HPP
