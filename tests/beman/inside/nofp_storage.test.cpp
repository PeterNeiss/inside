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

TEST(NoFpStorageTest, amp_output_reads_the_angle_by_value)
{
  using ang = inside<{{-4, 4}, per<1024>}, round_nearest | f64>;   // integer-backed under NO_FP
  EXPECT_EQ(rational{math::sin_into<math::amp<14>>(ang{0.5})}, (rational{1, 2}));   // sin 0.5 ≈ 0.479 → 7/14
  EXPECT_EQ(rational{math::sin_into<math::amp<14>>(ang{0})}, rational{0});
}
