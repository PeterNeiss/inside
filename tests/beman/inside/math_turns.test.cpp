// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// docs/math.md "Angles over many turns": a wrapping radians grid folds modulo
// span + notch (not 2π); an unwrapped radians angle and a turn phase converted
// at the call do not drift.
#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>

#include <gtest/gtest.h>

using namespace beman::inside;

TEST(MathTurnsTest, a_wrapping_radians_grid_folds_modulo_span_plus_notch) {
    using wrapped = inside<{{0, 6.28125}, per<64>}, wrap | round_nearest | f64>;
    using step_t  = inside<{{0, 8}, per<64>}, round_nearest | f64>;
    wrapped w{0};
    w += step_t{6.296875}; // span + notch: back to 0
    EXPECT_EQ(rational{w}, 0);
    wrapped v{0};
    v += step_t{6.28125}; // ≈ 2π is not a full fold
    EXPECT_EQ(rational{v}, (rational{201, 32}));
}

TEST(MathTurnsTest, a_turn_phase_wraps_exactly_and_converts_at_the_call) {
    using turn_t  = inside<{{0, 1 - 1.0 / 4096}, per<4096>}, wrap | round_nearest>;
    using angle_t = inside<{{0, 8}, per<16384>}, round_nearest | f64>;
    turn_t       phase{0};
    const turn_t step{rational{777, 4096}};
    for (int k = 0; k < 4096 * 3; ++k)
        phase += step;             // 777·3 whole turns
    EXPECT_EQ(rational{phase}, 0); // no drift

    turn_t     q{0.25};
    const auto s = math::sin_into<math::amp<16384>>(angle_t{q * math::two_pi});
    EXPECT_EQ(rational{s}, 1); // sin(π/2), on the 1/16384 grid
}

TEST(MathTurnsTest, an_unwrapped_radians_angle_stays_exact) {
    using big    = inside<{{0, 1 << 20}, per<64>}, round_nearest | f64>;
    using step_t = inside<{{0, 1}, per<64>}, round_nearest | f64>;
    big x{0};
    for (int n = 0; n < 100000; ++n)
        x += step_t{0.25};
    EXPECT_EQ(rational{x}, 25000); // exact after ~4000 turns
}
