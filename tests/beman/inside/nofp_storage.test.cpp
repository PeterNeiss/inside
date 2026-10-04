// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// BEMAN_INSIDE_MATH_NO_FP alone (not BEMAN_INSIDE_MATH_CORDIC): fp storage flags
// fall back to integer storage, so storage and the FP-free math agree.
#define BEMAN_INSIDE_MATH_NO_FP

#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>

#include <gtest/gtest.h>

using namespace beman::inside;
using namespace beman::inside::detail;

TEST(NoFpStorageTest, f64_flag_is_integer_backed)
{
  using F = inside<{{0, 4}, per<4>}, f64>;
  static_assert(!fp_raw<F>);
  const F f{rational{3, 4}};
  EXPECT_EQ(rational{f}, (rational{3, 4}));
}

TEST(NoFpStorageTest, circle_trig_reads_the_angle_by_value)
{
  math::circle<1024> a{90}, b{30};
  const auto s = math::sin(a);                               // auto: amp<1024>
  const auto t = math::sin_into<math::amp<14>>(b);           // 30° snaps to slot 85 (29.88°)
  EXPECT_EQ(rational{s}, rational{1});
  EXPECT_EQ(rational{t}, (rational{1, 2}));                  // on the 1/14 amplitude grid
}
