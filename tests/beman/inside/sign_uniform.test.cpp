// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// math::sign / math::copysign (algebraic tier) and uniform<B>(rng) (random.hpp).
#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>
#include <beman/inside/random.hpp>

#include <gtest/gtest.h>

#include <random>
#include <set>

using namespace beman::inside;

TEST(SignTest, sign_grid_follows_the_input_signs) {
    using S = inside<{-50, 50}>;
    static_assert(grid_of<decltype(math::sign(S{0}))> == grid{-1, 1});
    EXPECT_EQ(math::sign(S{-7}), -1);
    EXPECT_EQ(math::sign(S{0}), 0);
    EXPECT_EQ(math::sign(S{7}), 1);

    using U = inside<{0, 100}>;
    static_assert(grid_of<decltype(math::sign(U{0}))> == grid{0, 1});
    using P = inside<{{1, 4}, per<4>}>;
    static_assert(grid_of<decltype(math::sign(P{2}))> == grid{1, 1});
    using N = inside<{{-3, -0.5}, per<2>}>;
    EXPECT_EQ(math::sign(N{-0.5}), -1);
}

TEST(SignTest, copysign_takes_the_magnitude_and_the_sign) {
    using M = inside<{{-4, 2}, per<2>}>;
    using S = inside<{-1, 1}>;
    using R = decltype(math::copysign(M{0}, S{0}));
    static_assert(grid_of<R> == grid{{-4, 4}, per<2>});
    EXPECT_EQ((rational{math::copysign(M{-1.5}, S{1})}), (rational{3, 2}));
    EXPECT_EQ((rational{math::copysign(M{1.5}, S{-1})}), (rational{-3, 2}));
    EXPECT_EQ((rational{math::copysign(M{-1.5}, S{0})}), (rational{3, 2})); // 0 counts as +

    using Pos = inside<{2, 5}>; // |mag| ∈ [2, 5]
    using Neg = inside<{-3, -1}>;
    static_assert(grid_of<decltype(math::copysign(Pos{2}, Neg{-1}))> == grid{-5, -2});
}

namespace {
template <class B>
std::set<rational> sample(int n) {
    std::mt19937_64    rng{12345};
    std::set<rational> seen;
    for (int i = 0; i < n; ++i) {
        const B        b = uniform<B>(rng);
        const rational v = b;
        EXPECT_TRUE(v >= lower_of<B> && v <= upper_of<B>);
        seen.insert(v);
    }
    return seen;
}
} // namespace

TEST(UniformTest, uniform_covers_every_slot_and_stays_on_the_grid) {
    using ints    = inside<{0, 9}>;
    using idx     = inside<{-5, 5}, indexed>;
    using quarter = inside<{{-1, 1}, per<4>}>;
    using thirds  = inside<{{0, 1}, per<3>}, exact>;
    EXPECT_EQ(sample<ints>(2000).size(), 10u);
    EXPECT_EQ(sample<idx>(2000).size(), 11u);
    const auto q = sample<quarter>(2000);
    EXPECT_EQ(q.size(), 9u);
    for (auto v : q) {
        const rational slots = ((v + 1).value() / rational{1, 4}).value();
        EXPECT_EQ(detail::abs_den(slots.Denominator), 1u); // on the 1/4 lattice
    }
    EXPECT_EQ(sample<thirds>(2000).size(), 4u);
    EXPECT_EQ(sample<decltype(5_ins)>(10).size(), 1u); // a point
}
