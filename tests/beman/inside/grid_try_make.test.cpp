// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>

#include <gtest/gtest.h>

using namespace beman::inside;
using namespace beman::inside::detail;

// grid::try_make accepts well-formed grids
TEST(GridTryMakeTest, grid_try_make_accepts_well_formed_grids)
{
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
    SCOPED_TRACE("non-degenerate interval, zero notch (rational storage)");
    auto g = grid::try_make(interval{0, 1}, 0_r);
    ASSERT_TRUE(g.has_value());
  }

  {
    SCOPED_TRACE("fixed-point Q8.8 grid");
    auto g = grid::try_make(interval{0, 255}, 0x1p-8_r);
    ASSERT_TRUE(g.has_value());
  }
}

// grid::try_make rejects malformed grids with typed errors
TEST(GridTryMakeTest, grid_try_make_rejects_malformed_grids_with_typed_errors)
{
  {
    SCOPED_TRACE("Lower > Upper -> domain_error");
    auto g = grid::try_make(interval{10, 0}, 1_r);
    ASSERT_FALSE(g.has_value());
    ASSERT_EQ(g.error(), errc::domain_error);
  }

  {
    SCOPED_TRACE("notch does not divide interval evenly -> rounding_error");
    auto g = grid::try_make(interval{0, 10}, 3_r);   // 10 / 3 has remainder
    ASSERT_FALSE(g.has_value());
    ASSERT_EQ(g.error(), errc::rounding_error);
  }

  {
    SCOPED_TRACE("Lower / notch has non-unit denominator -> rounding_error");
    // notch 2, lower 1: lower/notch = 1/2, denominator != 1
    auto g = grid::try_make(interval{1, 11}, 2_r);
    ASSERT_FALSE(g.has_value());
    ASSERT_EQ(g.error(), errc::rounding_error);
  }
}

// grid::try_make rejects a negative notch (decoding would count downward)
TEST(GridTryMakeTest, grid_try_make_rejects_a_negative_notch)
{
  auto g = grid::try_make(interval{0, 10}, rational{-2});
  ASSERT_FALSE(g.has_value());
  EXPECT_EQ(g.error(), errc::domain_error);
}
