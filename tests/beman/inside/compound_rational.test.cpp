// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// x += r and x -= r with a rational r: on a Q-format grid (notch 1/K) a
// whole number of notches adds to the raw directly, without the exact sum.
// That count needs no division where K is a power of two. Whatever path a
// value takes, the result is the exact sum rational{x} ± r stored by x's
// policy: the same value, or the same error. Checked here for every value
// of small grids (K a power of two, 10, 12), every policy kind, and
// denominators that do and do not divide K, including unreduced ones.

#include <beman/inside/inside.hpp>

#include <gtest/gtest.h>

#include <expected>

using namespace beman::inside;
using namespace beman::inside::detail;

namespace {
template <class T, class Fn>
std::expected<T, errc> outcome(Fn&& f) {
    try {
        return f();
    } catch (const inside_error& e) {
        return std::unexpected{e.Code};
    }
}

template <class T>
T added(T y, rational r) {
    y += r;
    return y;
}
template <class T>
T subtracted(T y, rational r) {
    y -= r;
    return y;
}

template <class T>
void expect_compound_matches_exact_sum() {
    const imax dens[] = {1, 2, 3, 4, 5, 8, 10, 12, 16, 20, 24, 64, 256, 512, 1000};
    for (const T x : values<T>)
        for (const imax d : dens)
            for (imax n = -3 * d; n <= 3 * d; n += (d > 24 ? d / 8 : 1)) {
                const rational r{n, d};
                ASSERT_EQ(outcome<T>([&] { return added(x, r); }), outcome<T>([&] { return T{*(rational{x} + r)}; }))
                    << "raw " << x.raw() << " + " << n << "/" << d;
                ASSERT_EQ(outcome<T>([&] { return subtracted(x, r); }),
                          outcome<T>([&] { return T{*(rational{x} - r)}; }))
                    << "raw " << x.raw() << " - " << n << "/" << d;
            }
    // A hand-built rational that is not in lowest terms: 2/512 is 1/256.
    for (const T x : values<T>)
        ASSERT_EQ(outcome<T>([&] { return added(x, rational_from_parts(2, 512)); }),
                  outcome<T>([&] { return T{*(rational{x} + rational{1, 256})}; }))
            << "raw " << x.raw();
}

template <grid G>
void expect_all_policies() {
    expect_compound_matches_exact_sum<inside<G>>();
    expect_compound_matches_exact_sum<inside<G, clamp>>();
    expect_compound_matches_exact_sum<inside<G, wrap>>();
    expect_compound_matches_exact_sum<inside<G, round_nearest>>();
    expect_compound_matches_exact_sum<inside<G, clamp | round_floor>>();
}
} // namespace

TEST(CompoundRationalTest, binary_q_format) { expect_all_policies<grid{{-2, 2}, per<16>}>(); }
TEST(CompoundRationalTest, binary_q_format_unsigned) { expect_all_policies<grid{{0, 0.25_r}, per<256>}>(); }
TEST(CompoundRationalTest, decimal_q_format) { expect_all_policies<grid{{-3, 3}, per<10>}>(); }
TEST(CompoundRationalTest, duodecimal_q_format) { expect_all_policies<grid{{0, 5}, per<12>}>(); }
