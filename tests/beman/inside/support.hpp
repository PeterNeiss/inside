// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Helpers shared by the test files.
#ifndef BEMAN_INSIDE_TESTS_SUPPORT_HPP
#define BEMAN_INSIDE_TESTS_SUPPORT_HPP

#include <gtest/gtest.h>

#include <expected>
#include <type_traits>

#include "exact.hpp"

namespace beman::inside::test {
// v, hidden from the optimizer: what follows computes at run time.
template <class T>
T at_run_time(T v) {
    if constexpr (std::is_scalar_v<T>) {
        volatile T x = v;
        return x;
    } else {
        asm volatile("" : : "r"(&v) : "memory"); // (a class: opaque to the optimizer)
        return v;
    }
}

// f()'s result as an expected: the expected a call that can fail returns,
// or the value of one that cannot.
template <class F>
auto reported(F&& f) {
    auto r = f();
    if constexpr (detail::is_expected_v<decltype(r)>)
        return r;
    else
        return std::expected<decltype(r), error>{r};
}

// v, or the value an expected v holds (a test that knows it has one).
template <class T>
auto unwrap(const T& v) {
    if constexpr (detail::is_expected_v<T>)
        return *v;
    else
        return v;
}

// Whether a store or compound op compiles (a requires-expression outside a
// template does not SFINAE: these probe through a concept).
template <class T, class V>
concept can_store = requires(T& t, const V& v) { t = v; };
template <class T, class V>
concept can_add = requires(T& t, const V& v) { t += v; };
template <class T, class V>
concept can_sub = requires(T& t, const V& v) { t -= v; };
template <class T, class V>
concept can_div = requires(T& t, const V& v) { t /= v; };
template <class T, class V>
concept can_mod = requires(T& t, const V& v) { t %= v; };
template <class T>
concept can_step = requires(T& t) { ++t; };

// v stored into T at run time under T's policy and the flags F (they replace
// T's of the same kind, as T{v, F}), reporting into an error: the stored T,
// with a test failure if the store failed.
template <class T, policy_flag F = detail::none, class V>
T store(const V& v) {
    error ec{};
    T     r{v, make_policy<F>(ec)};
    EXPECT_EQ(ec, error{}) << "the store failed";
    return r;
}

// The error a run-time store of v into T (under F) reports, error{} for none.
template <class T, policy_flag F = detail::none, class V>
error store_error(const V& v) {
    error ec{};
    T     r{v, make_policy<F>(ec)};
    static_cast<void>(r);
    return ec;
}

// Division by the rounding mode the operands' types share (the operators `/`
// and `%` are exact and truncating whatever the types round): what a store
// into such a type, or div(a, b, mode), computes.
template <class A, class B>
inline constexpr policy_flag modes_of = (policy_of<A> | policy_of<B>)&(detail::mode_bits | detail::snap);
template <class A, class B>
constexpr auto rdiv(const A& a, const B& b) {
    return div(a, b, make_policy<modes_of<A, B>>());
}
template <class A, class B>
constexpr auto rmod(const A& a, const B& b) {
    return mod(a, b, make_policy<modes_of<A, B>>());
}
template <class A, class B>
constexpr auto rdivmod(const A& a, const B& b) {
    return divmod(a, b, make_policy<modes_of<A, B>>());
}
} // namespace beman::inside::test

// An exhaustive sweep runs in optimized builds; an unoptimized one skips it
// (the release cells of every CI run cover it).
#ifdef __OPTIMIZE__
    #define BEMAN_INSIDE_RELEASE_ONLY() static_cast<void>(0)
#else
    #define BEMAN_INSIDE_RELEASE_ONLY() GTEST_SKIP() << "an exhaustive sweep: runs in optimized builds"
#endif

#endif // BEMAN_INSIDE_TESTS_SUPPORT_HPP
