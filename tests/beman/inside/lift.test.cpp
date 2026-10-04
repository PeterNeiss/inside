// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>

#include <gtest/gtest.h>

#include <expected>
#include <type_traits>

using namespace beman::inside;
using beman::inside::detail::lift;

namespace
{
  // Fallible-argument stand-ins: std::expected is passed as an argument, never
  // stored, so the tests produce it from these functions.
  constexpr std::expected<int, errc> good(int v) { return v; }
  constexpr std::expected<int, errc> bad(errc e = errc::overflow) { return std::unexpected{e}; }
}

// lift basics: pure values
TEST(LiftTest, lift_basics_pure_values)
{
  auto plus = [](int a, int b) { return a + b; };
  static_assert(*lift(plus, 2, 3) == 5);
  static_assert(std::is_same_v<decltype(lift(plus, 2, 3)), std::expected<int, errc>>);
}

// lift with expected arg(s) / (good, value)
TEST(LiftTest, lift_with_expected_arg_good_value)
{
  auto plus = [](int a, int b) { return a + b; };
  auto r = lift(plus, good(2), 3);
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(*r, 5);
}

// lift with expected arg(s) / (error, value) -> error
TEST(LiftTest, lift_with_expected_arg_error_value_to_error)
{
  auto plus = [](int a, int b) { return a + b; };
  auto r = lift(plus, bad(errc::domain_error), 3);
  ASSERT_FALSE(r.has_value());
  ASSERT_EQ(r.error(), errc::domain_error);
}

// lift with expected arg(s) / (value, error) -> error
TEST(LiftTest, lift_with_expected_arg_value_error_to_error)
{
  auto plus = [](int a, int b) { return a + b; };
  auto r = lift(plus, 2, bad(errc::division_by_zero));
  ASSERT_FALSE(r.has_value());
  ASSERT_EQ(r.error(), errc::division_by_zero);
}

// lift with expected arg(s) / (good, good)
TEST(LiftTest, lift_with_expected_arg_good_good)
{
  auto plus = [](int a, int b) { return a + b; };
  auto r = lift(plus, good(2), good(3));
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(*r, 5);
}

// lift: the leftmost error wins
TEST(LiftTest, lift_leftmost_error_wins)
{
  auto plus = [](int a, int b) { return a + b; };
  auto r = lift(plus, bad(errc::domain_error), bad(errc::overflow));
  ASSERT_FALSE(r.has_value());
  ASSERT_EQ(r.error(), errc::domain_error);
}

// lift auto-flatten when op returns expected
TEST(LiftTest, lift_auto_flatten_when_op_returns_expected)
{
  auto checked_div = [](int a, int b) -> std::expected<int, errc>
  {
    if (b == 0) return std::unexpected{errc::division_by_zero};
    return a / b;
  };

  auto r = lift(checked_div, 6, 2);
  static_assert(std::is_same_v<decltype(r), std::expected<int, errc>>);
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(*r, 3);

  auto z = lift(checked_div, 6, 0);
  ASSERT_FALSE(z.has_value());
  ASSERT_EQ(z.error(), errc::division_by_zero);
}

// lift with three args
TEST(LiftTest, lift_with_three_args)
{
  auto sum3 = [](int a, int b, int c) { return a + b + c; };

  // all values
  ASSERT_EQ((*lift(sum3, 1, 2, 3)), 6);

  // one error -> error
  ASSERT_FALSE((lift(sum3, 1, bad(), 3).has_value()));

  // all good
  auto r = lift(sum3, good(1), good(2), good(3));
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(*r, 6);
}

// lift propagates exceptions thrown by op
TEST(LiftTest, lift_propagates_exceptions_thrown_by_op)
{
  auto thrower = [](int) -> int { throw 42; };
  ASSERT_THROW((void)(lift(thrower, 1)), int);
}
