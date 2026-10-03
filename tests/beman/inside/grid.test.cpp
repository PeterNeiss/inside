// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::detail;

// grid structured binding
TEST(GridTest, grid_structured_binding)
{
  static_assert(std::tuple_size_v<grid> == 2);
  static_assert(std::is_same_v<std::tuple_element_t<0, grid>, interval>);
  static_assert(std::is_same_v<std::tuple_element_t<1, grid>, rational>);

  grid g{{0, 100}, 2};
  auto [iv, notch] = g;
  ASSERT_EQ(iv.Lower, 0);
  ASSERT_EQ(iv.Upper, 100);
  ASSERT_EQ(notch, 2);
}

// grid construction and max_notch
TEST(GridTest, grid_construction_and_max_notch)
{
  static_assert(grid{{0, 100}, 1}.max_notch() == 100);
  static_assert(grid{{1, 5},   0.25}.max_notch() == 16);
  static_assert(grid{{0, std::numeric_limits<umax>::max()}, 1}.max_notch()
                 == std::numeric_limits<umax>::max());
}

// just<>/inside values work as grid corners
TEST(GridTest, just_inside_values_work_as_grid_corners)
{
  // Corners are `rational`, so an inside converts via its implicit operator rational().
  static_assert(grid{0, just<30>}            == grid{0, 30});
  static_assert(grid{just<5>, just<10>}      == grid{5, 10});
  static_assert(grid{just<2>}                == grid{2});       // 1-arg point grid
  static_assert(grid{just<0>, just<8>, just<2>} == grid{0, 8, 2});
  // ...and as inside<> grid-spec corners (the rifle.cpp use case).
  static_assert(Grid<inside<{0, just<30>}, wrap>> == Grid<inside<{0, 30}, wrap>>);
}

// grid storage_min selection
TEST(GridTest, grid_storage_min_selection)
{
  // Notch 0 -> rational
  static_assert(std::is_same_v<storage_min<grid{{0,10}, 0}>, rational>);

  // Notch 1, lower 0 -> smallest unsigned that fits Upper
  static_assert(std::is_same_v<storage_min<grid{0, 100,  1}>, std::uint8_t>);
  static_assert(std::is_same_v<storage_min<grid{0, 1000, 1}>, std::uint16_t>);
  static_assert(std::is_same_v<storage_min<grid{0, 100000, 1}>, std::uint32_t>);

  // Signed integer case (notch 1, negative lower)
  static_assert(std::is_same_v<storage_min<grid{-127, 127, 1}>,    std::int8_t>);
  static_assert(std::is_same_v<storage_min<grid{-32000, 32000, 1}>, std::int16_t>);
}

// grid arithmetic
TEST(GridTest, grid_arithmetic)
{
  {
    SCOPED_TRACE("add aligns notches via gcd");
    grid a{{0, 10}, 1};
    grid b{{0,  5}, 1};
    auto r = a + b;
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->Interval.Lower == 0);
    ASSERT_TRUE(r->Interval.Upper == 15);
    ASSERT_TRUE(r->Notch == 1);
  }

  {
    SCOPED_TRACE("multiply takes bounding box and lcm-ish notch");
    grid a{{0, 10}, 1};
    grid b{{0,  5}, 1};
    auto r = a * b;
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->Interval.Lower == 0);
    ASSERT_TRUE(r->Interval.Upper == 50);
  }

  {
    SCOPED_TRACE("divide by zero-only interval yields nullopt");
    grid a{{0, 10}, 1};
    grid zero{{0, 0}, 0};         // pure zero-point grid
    auto r = a / zero;
    ASSERT_FALSE(r.has_value());
  }

  {
    SCOPED_TRACE("divide by positive interval excluding zero");
    grid a{{0, 10}, 1};
    grid b{{2,  5}, 1};
    auto r = a / b;
    ASSERT_TRUE(r.has_value());
  }

  {
    SCOPED_TRACE("divide by interval [0, U] excludes zero from divisor by stepping in by notch");
    grid a{{0, 10}, 1};
    grid b{{0, 5}, 1};   // includes zero, positive upper
    auto r = a / b;
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->Interval.Lower == 0);
    ASSERT_TRUE(r->Interval.Upper == 10);    // 10 / 1 = 10
    ASSERT_TRUE(r->Notch == 0);
  }

  {
    SCOPED_TRACE("divide by interval [L, 0] (negative side only)");
    grid a{{0, 10}, 1};
    grid b{{-5, 0}, 1};   // includes zero, negative lower
    auto r = a / b;
    ASSERT_TRUE(r.has_value());
    // dividing positives by negatives gives non-positive result
    ASSERT_TRUE(r->Interval.Upper <= 0);
    ASSERT_TRUE(r->Notch == 0);
  }

  {
    SCOPED_TRACE("divide by interval that straddles zero");
    grid a{{1, 10}, 1};
    grid b{{-5, 5}, 1};   // straddles zero
    auto r = a / b;
    ASSERT_TRUE(r.has_value());
    // result must span both signs once we exclude zero
    ASSERT_TRUE(r->Interval.Lower < 0);
    ASSERT_TRUE(r->Interval.Upper > 0);
    ASSERT_TRUE(r->Notch == 0);
  }

  {
    SCOPED_TRACE("divide by zero-notch interval that straddles zero");
    grid a{{1, 10}, 1};
    grid b{{-5, 5}, 0};   // notch=0, so step defaults to 1
    auto r = a / b;
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->Interval.Lower < 0);
    ASSERT_TRUE(r->Interval.Upper > 0);
  }
}

// grid validate
TEST(GridTest, grid_validate)
{
  static_assert(grid::validate<grid{{0, 10},  1}>());
  static_assert(grid::validate<grid{{0, 10},  rational{1u, 2}}>());
  static_assert(grid::validate<grid{0_r}>());                   // point grid, notch=0
}

// grid sentinel
TEST(GridTest, grid_sentinel)
{
  ASSERT_FALSE(slim::optional<grid>{}.has_value());
  auto s = grid::make_sentinel();
  ASSERT_EQ(s.Notch.Denominator, 0);
}
