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
    volatile T x = v;
    return x;
}

// f(), or the errc of the inside_error it throws (a failed store under the
// default handler), as an expected.
template <class F>
auto caught(F&& f) -> std::expected<std::invoke_result_t<F&>, errc> {
    try {
        return f();
    } catch (const inside_error& e) {
        return std::unexpected{e.Code};
    }
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
