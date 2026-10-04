// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Fallible construction is `B::try_make(value)` → std::expected<B, errc>: a
// value the policy cannot bring into the grid is an error value, not a throw.
// (The former `inside(value, errc&)` constructor is gone; assigning with an error
// code stays `b.policy(ec) = value`.)

#include <beman/inside/inside.hpp>

#include <gtest/gtest.h>

using namespace beman::inside;
using namespace beman::inside::detail;

// out of range → overflow
TEST(TryMakeTest, out_of_range_is_overflow)
{
  auto r = inside<{0, 100}>::try_make(150);
  ASSERT_FALSE(r.has_value());
  EXPECT_EQ(r.error(), errc::overflow);
}

// in range → the value
TEST(TryMakeTest, in_range_yields_the_value)
{
  auto r = inside<{0, 100}>::try_make(42);
  ASSERT_TRUE(r.has_value());
  EXPECT_TRUE(static_cast<rational>(*r) == 42);
}

// clamp / wrap bring the value in: no error
TEST(TryMakeTest, clamp_and_wrap_are_not_errors)
{
  auto c = inside<{0, 100}, clamp>::try_make(150);
  ASSERT_TRUE(c.has_value());
  EXPECT_TRUE(static_cast<rational>(*c) == 100);
  auto w = inside<{0, 9}, wrap>::try_make(13);
  ASSERT_TRUE(w.has_value());
  EXPECT_TRUE(static_cast<rational>(*w) == 3);          // 13 mod 10
}

// try_make reports what policy(ec) assignment reports on the same input
TEST(TryMakeTest, matches_per_op_policy_ec_on_the_same_input)
{
  errc ec_op{};
  inside<{0, 100}> via_op{0};
  via_op.policy(ec_op) = 200;
  using pct = inside<{0, 100}>;
  EXPECT_EQ(pct::try_make(200).error(), ec_op);
}

#ifndef BEMAN_INSIDE_MATH_NO_FP
// fp storage goes through the same store as the constructors
TEST(TryMakeTest, fp_storage_reports_like_the_constructor)
{
  using F = inside<{{0, 1}, per<4>}, f64>;
  EXPECT_EQ(F::try_make(1.2).error(), errc::overflow);         // rounds to 1.25: outside
  ASSERT_TRUE(F::try_make(1.1).has_value());                   // rounds to 1.0
  EXPECT_EQ(F::try_make(1.1)->raw(), 1.0);
}
#endif
