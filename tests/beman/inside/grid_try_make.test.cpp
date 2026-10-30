// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>

#include <gtest/gtest.h>

using namespace beman::inside;
using namespace beman::inside::detail;

// grid::try_make accepts well-formed grids
TEST(GridTryMakeTest, grid_try_make_accepts_well_formed_grids) {
    {
        SCOPED_TRACE("integer interval, unit notch");
        auto g = grid::try_make(interval{0, 100}, 1_r);
        ASSERT_TRUE(g.has_value());
        ASSERT_TRUE((g->Interval == interval{0, 100}));
        ASSERT_TRUE(g->Notch == 1);
    }

    {
        SCOPED_TRACE("degenerate interval, zero notch (rational-raw shape)");
        auto g = grid::try_make(interval{0, 0}, 0_r);
        ASSERT_TRUE(g.has_value());
        ASSERT_TRUE(g->Notch == 0);
    }

    {
        SCOPED_TRACE("continuous: a denominator bound (rational storage)");
        auto g = grid::try_make(interval{0, 1}, max_denominator{1000});
        ASSERT_TRUE(g.has_value());
        ASSERT_TRUE(g->Notch == 0 && g->MaxDenominator == 1000);
    }

    {
        SCOPED_TRACE("fixed-point Q8.8 grid");
        auto g = grid::try_make(interval{0, 255}, 0x1p-8_r);
        ASSERT_TRUE(g.has_value());
    }
}

// grid::try_make rejects malformed grids with typed errors
TEST(GridTryMakeTest, grid_try_make_rejects_malformed_grids_with_typed_errors) {
    {
        SCOPED_TRACE("Lower > Upper -> domain_error");
        auto g = grid::try_make(interval{10, 0}, 1_r);
        ASSERT_FALSE(g.has_value());
        ASSERT_EQ(g.error(), errc::domain_error);
    }

    {
        SCOPED_TRACE("notch does not divide interval evenly -> rounding_error");
        auto g = grid::try_make(interval{0, 10}, 3_r); // 10 / 3 has remainder
        ASSERT_FALSE(g.has_value());
        ASSERT_EQ(g.error(), errc::rounding_error);
    }

    {
        SCOPED_TRACE("continuous without a denominator bound -> domain_error");
        EXPECT_EQ(grid::try_make(interval{0, 1}, 0_r).error(), errc::domain_error);
        EXPECT_EQ(grid::try_make(interval{0, 1}, max_denominator{rational{1, 2}}).error(), errc::domain_error);
        EXPECT_EQ(grid::try_make(interval{0, rational{1, 3}}, max_denominator{2}).error(), errc::domain_error);
        EXPECT_EQ(grid::try_make(interval{1, 1}, max_denominator{10}).error(), errc::domain_error); // a point
    }
}

// A Lower off the notch's multiples is fine: the lattice is 1, 3, …, 11.
TEST(GridTryMakeTest, grid_try_make_accepts_an_unanchored_lattice) {
    auto g = grid::try_make(interval{1, 11}, 2_r);
    ASSERT_TRUE(g.has_value());
    EXPECT_FALSE(g->anchored());
    EXPECT_EQ(g->step_count(), detail::grid_int{5});
}

// grid::try_make rejects a negative notch (decoding would count downward)
TEST(GridTryMakeTest, grid_try_make_rejects_a_negative_notch) {
    auto g = grid::try_make(interval{0, 10}, rational{-2});
    ASSERT_FALSE(g.has_value());
    EXPECT_EQ(g.error(), errc::domain_error);
}

// grid::try_make moves an open limit in by one notch, like the ctors.
TEST(GridTryMakeTest, grid_try_make_with_bounds) {
    EXPECT_EQ(*grid::try_make(interval{0, 360}, 1_r, bounds::half_open), (grid{0, 359}));
    EXPECT_EQ(*grid::try_make(interval{0, 10}, 1_r, bounds::open), (grid{1, 9}));
    EXPECT_EQ(*grid::try_make(interval{0, 10}, 1_r), (grid{0, 10}));
    // a continuous grid has no next point; an open {0, 1} on 1 holds none
    EXPECT_EQ(grid::try_make(interval{0, 1}, 0_r, bounds::half_open).error(), errc::domain_error);
    EXPECT_EQ(grid::try_make(interval{0, 1}, 1_r, bounds::open).error(), errc::domain_error);
}
