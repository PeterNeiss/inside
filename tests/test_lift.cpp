#include "bound/bound.hpp"

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

using namespace bnd;

TEST_CASE("lift basics: pure values", "[lift]")
{
  auto plus = [](int a, int b) { return a + b; };
  STATIC_REQUIRE(*lift(plus, 2, 3) == 5);
}

TEST_CASE("lift with optional arg(s)", "[lift][optional]")
{
  auto plus = [](int a, int b) { return a + b; };

  SECTION("(opt-engaged, value)")
  {
    slim::optional<int> a{2};
    auto r = lift(plus, a, 3);
    REQUIRE(r.has_value());
    REQUIRE(*r == 5);
  }

  SECTION("(opt-empty, value) -> nullopt")
  {
    slim::optional<int> a{slim::nullopt};
    REQUIRE_FALSE(lift(plus, a, 3).has_value());
  }

  SECTION("(value, opt-empty) -> nullopt")
  {
    slim::optional<int> b{slim::nullopt};
    REQUIRE_FALSE(lift(plus, 2, b).has_value());
  }

  SECTION("(opt, opt) both engaged")
  {
    slim::optional<int> a{2}, b{3};
    auto r = lift(plus, a, b);
    REQUIRE(r.has_value());
    REQUIRE(*r == 5);
  }
}

TEST_CASE("lift auto-flatten when op returns optional", "[lift][flatten]")
{
  auto opt_div = [](int a, int b) -> slim::optional<int>
  { return (b == 0) ? slim::optional<int>{slim::nullopt} : slim::optional<int>{a / b}; };

  auto r = lift(opt_div, 6, 2);
  STATIC_REQUIRE(std::is_same_v<decltype(r), slim::optional<int>>);
  REQUIRE(r.has_value());
  REQUIRE(*r == 3);

  REQUIRE_FALSE(lift(opt_div, 6, 0).has_value());
}

TEST_CASE("lift with three args", "[lift][variadic]")
{
  auto sum3 = [](int a, int b, int c) { return a + b + c; };

  // all values
  REQUIRE(*lift(sum3, 1, 2, 3) == 6);

  // one optional empty -> nullopt
  slim::optional<int> mid{slim::nullopt};
  REQUIRE_FALSE(lift(sum3, 1, mid, 3).has_value());

  // all optionals engaged
  slim::optional<int> a{1}, b{2}, c{3};
  auto r = lift(sum3, a, b, c);
  REQUIRE(r.has_value());
  REQUIRE(*r == 6);
}

TEST_CASE("lift propagates exceptions thrown by op", "[lift][exception]")
{
  auto thrower = [](int) -> int { throw 42; };
  REQUIRE_THROWS_AS(lift(thrower, 1), int);
}
