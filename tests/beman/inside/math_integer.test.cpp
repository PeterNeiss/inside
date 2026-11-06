// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Counting and number theory (math/integer.hpp): factorial, binomial,
// falling_factorial, rising_factorial, multinomial, gcd and lcm. Their result
// grids, every value on small ranges against plain loops and the identities,
// values past 64 bits, the _into forms and compile-time evaluation. Inputs
// pass through a volatile, so the run-time paths run.

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/math.hpp>

#include <gtest/gtest.h>

#include <numeric>
#include <type_traits>

using namespace beman::inside;
using detail::big_int;
using detail::grid_rational;

namespace {
int at_run_time(int v) {
    volatile int x = v;
    return x;
}

umax factorial_ref(umax n) {
    umax f = 1;
    for (umax i = 2; i <= n; ++i)
        f *= i;
    return f;
}
umax binomial_ref(umax n, umax k) {
    if (k > n)
        return 0;
    umax c = 1;
    for (umax i = 1; i <= k; ++i)
        c = c * (n - k + i) / i;
    return c;
}
umax falling_ref(umax n, umax k) {
    if (k > n)
        return 0;
    umax p = 1;
    for (umax i = 0; i < k; ++i)
        p *= n - i;
    return p;
}

consteval grid_rational g(big_int v) { return grid_rational{v}; }
} // namespace

TEST(MathInteger, result_grids) {
    using n20 = inside<{0, 20}>;
    static_assert(grid_of<decltype(math::factorial(n20{}))> == grid{1, 2432902008176640000});
    static_assert(grid_of<decltype(math::factorial(inside<{3, 5}>{}))> == grid{6, 120});
    static_assert(grid_of<decltype(math::factorial(just<5>))> == grid{120});
    static_assert(sizeof(math::factorial(n20{})) == 8);

    // falling: 0 where k can pass n; P(10, 5) at the top.
    static_assert(grid_of<decltype(math::falling_factorial(inside<{0, 10}>{}, inside<{0, 5}>{}))> == grid{0, 30240});
    static_assert(grid_of<decltype(math::falling_factorial(inside<{5, 10}>{}, inside<{0, 3}>{}))> == grid{1, 720});
    // rising: 0 when n can be 0 with k ≥ 1.
    static_assert(grid_of<decltype(math::rising_factorial(inside<{0, 10}>{}, inside<{0, 5}>{}))> == grid{0, 240240});
    static_assert(grid_of<decltype(math::rising_factorial(inside<{2, 4}>{}, inside<{1, 3}>{}))> == grid{2, 120});
    // binomial: the peak at the k nearest n/2, the least at an end of k's range.
    static_assert(grid_of<decltype(math::binomial(inside<{0, 60}>{}, inside<{0, 60}>{}))> ==
                  grid{0, 118264581564861424});
    static_assert(grid_of<decltype(math::binomial(inside<{10, 20}>{}, inside<{0, 5}>{}))> == grid{1, 15504});
    static_assert(grid_of<decltype(math::binomial(inside<{10, 20}>{}, inside<{3, 4}>{}))> == grid{120, 4845});
    static_assert(grid_of<decltype(math::multinomial(inside<{0, 2}>{}, inside<{0, 2}>{}))> == grid{1, 6});

    // gcd and lcm keep the lattice their operands share.
    static_assert(grid_of<decltype(math::gcd(inside<{-30, 30}>{}, inside<{-30, 30}>{}))> == grid{0, 30});
    static_assert(grid_of<decltype(math::gcd(inside<{{0, 100}, 2}>{}, inside<{{0, 100}, 4}>{}))> == grid{{0, 100}, 2});
    static_assert(grid_of<decltype(math::gcd(inside<{1, 30}>{}, inside<{1, 30}>{}))> == grid{1, 30});
    static_assert(grid_of<decltype(math::gcd(just<12>, just<18>))> == grid{6});
    static_assert(grid_of<decltype(math::lcm(inside<{1, 30}>{}, inside<{1, 30}>{}))> == grid{1, 900});
    static_assert(grid_of<decltype(math::lcm(inside<{0, 5}>{}, inside<{1, 5}>{}))> == grid{0, 25});
    static_assert(grid_of<decltype(math::lcm(inside<{{2, 30}, 2}>{}, inside<{{3, 30}, 3}>{}))> == grid{{6, 900}, 6});
    static_assert(grid_of<decltype(math::lcm(just<4>, just<6>))> == grid{12});
}

TEST(MathInteger, compile_time) {
    static_assert(math::factorial(just<10>) == 3628800);
    static_assert(math::binomial(just<49>, just<6>) == 13983816);
    static_assert(math::falling_factorial(just<8>, just<3>) == 336);
    static_assert(math::rising_factorial(just<5>, just<3>) == 210);
    static_assert(math::multinomial(1_ins, 4_ins, 4_ins, 2_ins) == 34650); // MISSISSIPPI
    static_assert(math::gcd(just<-12>, just<18>) == 6 && math::lcm(just<-4>, just<6>) == 12);
    static_assert(math::binomial(inside<{0, 60}>{60}, inside<{0, 60}>{30}) == 118264581564861424);
}

TEST(MathInteger, small_ranges_against_loops) {
    using n60 = inside<{0, 60}>;
    for (int n = 0; n <= 20; ++n)
        EXPECT_TRUE(math::factorial(inside<{0, 20}>{at_run_time(n)}) == factorial_ref(static_cast<umax>(n))) << n;
    for (int n = 0; n <= 60; ++n)
        for (int k = 0; k <= 60; ++k) {
            const auto c = math::binomial(n60{at_run_time(n)}, n60{at_run_time(k)});
            EXPECT_TRUE(c == binomial_ref(static_cast<umax>(n), static_cast<umax>(k))) << n << " " << k;
            if (k <= n) { // symmetry
                EXPECT_TRUE(c == math::binomial(n60{n}, n60{n - k}));
            }
        }
    using n20 = inside<{0, 20}>;
    for (int n = 0; n <= 20; ++n)
        for (int k = 0; k <= 20; ++k) {
            const auto p = math::falling_factorial(n20{at_run_time(n)}, n20{at_run_time(k)});
            EXPECT_TRUE(p == falling_ref(static_cast<umax>(n), static_cast<umax>(k))) << n << " " << k;
            // C(n, k)·k! = P(n, k)
            EXPECT_TRUE(math::binomial(n20{n}, n20{k}) * math::factorial(n20{k}) == p);
        }
    using n12 = inside<{0, 12}>;
    for (int n = 0; n <= 12; ++n)
        for (int k = 0; k <= 12; ++k) {
            const auto r    = math::rising_factorial(n12{at_run_time(n)}, n12{at_run_time(k)});
            const umax want = k == 0   ? 1
                              : n == 0 ? 0
                                       : falling_ref(static_cast<umax>(n + k - 1), static_cast<umax>(k));
            EXPECT_TRUE(r == want) << n << " " << k;
        }
    using n8 = inside<{0, 8}>;
    for (int a = 0; a <= 8; ++a)
        for (int b = 0; b <= 8; ++b)
            for (int c = 0; c <= 8; ++c) {
                const auto m    = math::multinomial(n8{at_run_time(a)}, n8{at_run_time(b)}, n8{at_run_time(c)});
                const umax want = binomial_ref(static_cast<umax>(a + b), static_cast<umax>(b)) *
                                  binomial_ref(static_cast<umax>(a + b + c), static_cast<umax>(c));
                EXPECT_TRUE(m == want) << a << " " << b << " " << c;
            }
    using z = inside<{-30, 30}>;
    for (int a = -30; a <= 30; ++a)
        for (int b = -30; b <= 30; ++b) {
            EXPECT_TRUE(math::gcd(z{at_run_time(a)}, z{at_run_time(b)}) == std::gcd(a, b)) << a << " " << b;
            EXPECT_TRUE(math::lcm(z{at_run_time(a)}, z{at_run_time(b)}) == std::lcm(a, b)) << a << " " << b;
        }
}

TEST(MathInteger, past_64_bits) {
    const auto f25 = math::factorial(inside<{0, 25}>{at_run_time(25)});
    EXPECT_EQ(to_string(f25), "15511210043330985984000000");
    const auto f100 = math::factorial(inside<{0, 100}>{at_run_time(100)});
    static_assert(detail::wide_index_storage<std::remove_const_t<decltype(f100)>>);
    EXPECT_EQ(to_string(f100),
              "93326215443944152681699238856266700490715968264381621468592963895217599993229915608941"
              "463976156518286253697920827223758251185210916864000000000000000000000000");
    using n100 = inside<{0, 100}>;
    EXPECT_EQ(to_string(math::binomial(n100{at_run_time(100)}, n100{at_run_time(50)})),
              "100891344545564193334812497256");
    // Pascal at C(1000, 500), 300 digits.
    using n1000   = inside<{0, 1000}>;
    using n999    = inside<{0, 999}>;
    const auto c  = math::binomial(n1000{at_run_time(1000)}, n1000{at_run_time(500)});
    const auto c1 = math::binomial(n999{at_run_time(999)}, n999{at_run_time(499)});
    const auto c2 = math::binomial(n999{at_run_time(999)}, n999{at_run_time(500)});
    EXPECT_TRUE(c == c1 + c2);
    EXPECT_EQ(to_string(c).size(), 300u);
    // P(30, 25) = 30!/5!
    EXPECT_TRUE(math::falling_factorial(inside<{0, 30}>{at_run_time(30)}, inside<{0, 30}>{at_run_time(25)}) ==
                math::factorial(inside<{0, 30}>{30}) / just<120>);

    // gcd and lcm of wide values: 2^150·3 and 2^120·9 share 2^120·3.
    using wide         = inside<{0, g(big_int{1} << 160)}>;
    constexpr auto a   = g((big_int{1} << 150) * big_int{3});
    constexpr auto b   = g((big_int{1} << 120) * big_int{9});
    const wide     wa  = a;
    const wide     wb  = b;
    const auto     gab = math::gcd(wa, wb);
    EXPECT_TRUE(gab == just<g((big_int{1} << 120) * big_int{3})>);
    EXPECT_TRUE(math::lcm(wa, wb) == just<g((big_int{1} << 150) * big_int{9})>);
    // An odd exponent and a power of two are coprime (RSA's e and φ, smaller).
    EXPECT_TRUE(math::gcd(inside<{0, 65537}>{65537}, wide{just<g(big_int{1} << 130)>}) == 1);
}

TEST(MathInteger, into_forms) {
    using n10 = inside<{0, 10}>;
    EXPECT_TRUE((math::factorial_into<inside<{0, 1000}, clamp>>(n10{at_run_time(7)})) == 1000); // 5040 saturates
    using checked_out = inside<{0, 1000}>;
    EXPECT_THROW((void)math::factorial_into<checked_out>(n10{at_run_time(7)}), inside_error);
    EXPECT_TRUE((math::binomial_into<inside<{0, 255}>>(n10{at_run_time(10)}, n10{at_run_time(5)})) == 252);
    EXPECT_TRUE((math::gcd_into<inside<{0, 100}>>(n10{at_run_time(4)}, n10{at_run_time(6)})) == 2);
    EXPECT_TRUE((math::lcm_into<inside<{0, 100}>>(n10{at_run_time(4)}, n10{at_run_time(6)})) == 12);
    EXPECT_TRUE((math::multinomial_into<inside<{0, 100}>>(n10{at_run_time(2)}, n10{at_run_time(2)})) == 6);
    EXPECT_TRUE((math::falling_factorial_into<inside<{0, 100}>>(n10{at_run_time(5)}, n10{at_run_time(2)})) == 20);
    EXPECT_TRUE((math::rising_factorial_into<inside<{0, 100}>>(n10{at_run_time(5)}, n10{at_run_time(2)})) == 30);
}
