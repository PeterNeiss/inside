// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>

#include <gtest/gtest.h>

#include <type_traits>

using namespace beman::inside;

// lift basics: pure values
TEST(LiftTest, lift_basics_pure_values)
{
  auto plus = [](int a, int b) { return a + b; };
  static_assert(*lift(plus, 2, 3) == 5);
}

// lift with optional arg(s) / (opt-engaged, value)
TEST(LiftTest, lift_with_optional_arg_s__opt_engaged_value)
{
  auto plus = [](int a, int b) { return a + b; };

  {
    SCOPED_TRACE("(opt-engaged, value)");
    slim::optional<int> a{2};
    auto r = lift(plus, a, 3);
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(*r, 5);
  }

}

// lift with optional arg(s) / (opt-empty, value) -> nullopt
TEST(LiftTest, lift_with_optional_arg_s__opt_empty_value_to_nullopt)
{
  auto plus = [](int a, int b) { return a + b; };

  {
    SCOPED_TRACE("(opt-empty, value) -> nullopt");
    slim::optional<int> a{slim::nullopt};
    ASSERT_FALSE((lift(plus, a, 3).has_value()));
  }

}

// lift with optional arg(s) / (value, opt-empty) -> nullopt
TEST(LiftTest, lift_with_optional_arg_s__value_opt_empty_to_nullopt)
{
  auto plus = [](int a, int b) { return a + b; };

  {
    SCOPED_TRACE("(value, opt-empty) -> nullopt");
    slim::optional<int> b{slim::nullopt};
    ASSERT_FALSE((lift(plus, 2, b).has_value()));
  }

}

// lift with optional arg(s) / (opt, opt) both engaged
TEST(LiftTest, lift_with_optional_arg_s__opt_opt_both_engaged)
{
  auto plus = [](int a, int b) { return a + b; };

  {
    SCOPED_TRACE("(opt, opt) both engaged");
    slim::optional<int> a{2}, b{3};
    auto r = lift(plus, a, b);
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(*r, 5);
  }
}

// lift auto-flatten when op returns optional
TEST(LiftTest, lift_auto_flatten_when_op_returns_optional)
{
  auto opt_div = [](int a, int b) -> slim::optional<int>
  { return (b == 0) ? slim::optional<int>{slim::nullopt} : slim::optional<int>{a / b}; };

  auto r = lift(opt_div, 6, 2);
  static_assert(std::is_same_v<decltype(r), slim::optional<int>>);
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(*r, 3);

  ASSERT_FALSE((lift(opt_div, 6, 0).has_value()));
}

// lift with three args
TEST(LiftTest, lift_with_three_args)
{
  auto sum3 = [](int a, int b, int c) { return a + b + c; };

  // all values
  ASSERT_EQ((*lift(sum3, 1, 2, 3)), 6);

  // one optional empty -> nullopt
  slim::optional<int> mid{slim::nullopt};
  ASSERT_FALSE((lift(sum3, 1, mid, 3).has_value()));

  // all optionals engaged
  slim::optional<int> a{1}, b{2}, c{3};
  auto r = lift(sum3, a, b, c);
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(*r, 6);
}

// lift propagates exceptions thrown by op
TEST(LiftTest, lift_propagates_exceptions_thrown_by_op)
{
  auto thrower = [](int) -> int { throw 42; };
  ASSERT_THROW((void)(lift(thrower, 1)), int);
}
