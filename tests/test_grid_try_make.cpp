#include "bound/bound.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bnd;

TEST_CASE("grid::try_make accepts well-formed grids", "[grid][try_make]")
{
  SECTION("integer interval, unit notch")
  {
    auto g = grid::try_make(interval{0, 100}, 1_r);
    REQUIRE(g.has_value());
    REQUIRE(g->Interval == interval{0, 100});
    REQUIRE(g->Notch == 1);
  }

  SECTION("degenerate interval, zero notch (rational-raw shape)")
  {
    auto g = grid::try_make(interval{0, 0}, 0_r);
    REQUIRE(g.has_value());
    REQUIRE(g->Notch == 0);
  }

  SECTION("non-degenerate interval, zero notch (rational storage)")
  {
    auto g = grid::try_make(interval{0, 1}, 0_r);
    REQUIRE(g.has_value());
  }

  SECTION("fixed-point Q8.8 grid")
  {
    auto g = grid::try_make(interval{0, 255}, (1_r / 256_r).value());
    REQUIRE(g.has_value());
  }
}

TEST_CASE("grid::try_make rejects malformed grids with typed errors", "[grid][try_make]")
{
  SECTION("Lower > Upper -> domain_error")
  {
    auto g = grid::try_make(interval{10, 0}, 1_r);
    REQUIRE_FALSE(g.has_value());
    REQUIRE(g.error() == errc::domain_error);
  }

  SECTION("notch does not divide interval evenly -> rounding_error")
  {
    auto g = grid::try_make(interval{0, 10}, 3_r);   // 10 / 3 has remainder
    REQUIRE_FALSE(g.has_value());
    REQUIRE(g.error() == errc::rounding_error);
  }

  SECTION("Lower / notch has non-unit denominator -> rounding_error")
  {
    // notch 2, lower 1: lower/notch = 1/2, denominator != 1
    auto g = grid::try_make(interval{1, 11}, 2_r);
    REQUIRE_FALSE(g.has_value());
    REQUIRE(g.error() == errc::rounding_error);
  }
}
