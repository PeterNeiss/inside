// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// BEMAN_INSIDE_MATH_INSTRUMENT: the driver counts its integer-path calls, deep
// steps and guesses (results still undecided at the cap), and IntegerOnly
// sends every call to the integer path.

#define BEMAN_INSIDE_MATH_INSTRUMENT 1
#define BEMAN_INSIDE_MATH_TABLE_SLOTS 0
#include <beman/inside/cmath.hpp>

#include <gtest/gtest.h>

using namespace beman::inside;
namespace engine = beman::inside::math::detail::engine;
namespace am     = beman::inside::math::adaptive;
using beman::inside::math::driver;
using detail::wide_rational;
using detail::wide_sint;

namespace {
using eighths = inside<{{0, 1}, rational{1, 8}}, round_nearest>;

// 1/16, a tie between 0 and 1/8, within ±4 units at every precision: never
// decided. With deep_extra past the cap (4·24 bits), the driver runs it once
// more at 24 + deep_extra bits.
struct never_decided {
    template <int W>
    constexpr engine::approx<8> run() const {
        return {engine::to_q<W, 8>(detail::exact_of<8>(rational{1, 16})), W, 4};
    }
};
struct never_decided_deep : never_decided {
    static constexpr int deep_extra = 200;
};

// 1/16 + 2^-40: decided once W passes 40.
struct near_tie {
    template <int W>
    constexpr engine::approx<8> run() const {
        const auto v = detail::exact_of<8>(rational{1, 16}) + wide_rational<8>{wide_sint<8>{1}, wide_sint<8>{1} << 40};
        return {engine::to_q<W, 8>(v), W, 4};
    }
};

struct counts {
    std::uint64_t Deep, Guesses;
};
counts read() { return {driver.Deep.load(), driver.Guesses.load()}; }
} // namespace

TEST(MathInstrumentTest, a_decided_result_counts_nothing) {
    const counts before = read();
    EXPECT_EQ((engine::evaluate<eighths, 24>(near_tie{})), eighths{0.125});
    EXPECT_EQ(read().Deep, before.Deep);
    EXPECT_EQ(read().Guesses, before.Guesses);
}

TEST(MathInstrumentTest, an_undecided_result_counts_one_guess) {
    const counts before = read();
    // The guess: the slot nearest the midpoint, 1/16 rounded away from zero.
    EXPECT_EQ((engine::evaluate<eighths, 24>(never_decided{})), eighths{0.125});
    EXPECT_EQ(read().Deep, before.Deep);
    EXPECT_EQ(read().Guesses, before.Guesses + 1);
}

TEST(MathInstrumentTest, the_deep_step_counts_and_then_the_guess) {
    const counts before = read();
    EXPECT_EQ((engine::evaluate<eighths, 24>(never_decided_deep{})), eighths{0.125});
    EXPECT_EQ(read().Deep, before.Deep + 1);
    EXPECT_EQ(read().Guesses, before.Guesses + 1);
}

TEST(MathInstrumentTest, constant_evaluation_gives_the_same_guess) {
    static_assert(engine::evaluate<eighths, 24>(never_decided{}) == eighths{0.125});
    static_assert(engine::evaluate<eighths, 24>(never_decided_deep{}) == eighths{0.125});
}

TEST(MathInstrumentTest, integer_only_skips_the_fast_tiers) {
    using in         = inside<{{-4, 4}, per<128>}, round_nearest>; // 1025 slots
    using out        = inside<{{-1, 1}, per<1024>}, round_nearest>;
    const auto sweep = [] {
        const std::uint64_t before = driver.IntegerRuns.load();
        for (int i = -512; i <= 512; ++i)
            (void)am::sin_into<out>(in{rational{i, 128}});
        return driver.IntegerRuns.load() - before;
    };
    driver.IntegerOnly = true;
    EXPECT_EQ(sweep(), 1025u);
    driver.IntegerOnly = false;
#ifndef BEMAN_INSIDE_MATH_NO_FP
    EXPECT_LT(sweep(), 1025u); // the double tier decides most of them
#else
    EXPECT_EQ(sweep(), 1025u);
#endif
}
