#include "bound/bound.hpp"
#include "bound/format.hpp"
#include "bound/formatter.hpp"
#include "bound/print.hpp"

#include <catch2/catch_test_macros.hpp>

#include <format>
#include <limits>

using namespace bnd;

TEST_CASE("rational to_string: decimal forms", "[format][rational]")
{
  REQUIRE(bnd::to_string(rational{1u, 2})    == "0.5");
  REQUIRE(bnd::to_string(rational{43u, 2})   == "21.5");
  REQUIRE(bnd::to_string(rational{1u, 4})    == "0.25");
  REQUIRE(bnd::to_string(rational{3u, 8})    == "0.375");
  REQUIRE(bnd::to_string(rational{1u, 10})   == "0.1");
  REQUIRE(bnd::to_string(rational{15u, 100}) == "0.15");

  // negative
  REQUIRE(bnd::to_string(rational{1, -2})   == "-0.5");
  REQUIRE(bnd::to_string(rational{43, -2})  == "-21.5");
}

TEST_CASE("rational to_string: mixed-number forms", "[format][rational]")
{
  REQUIRE(bnd::to_string(rational{7u, 3})  == "2 1/3");
  REQUIRE(bnd::to_string(rational{22u, 7}) == "3 1/7");
  REQUIRE(bnd::to_string(rational{1u, 3})  == "1/3");
  REQUIRE(bnd::to_string(rational{2u, 7})  == "2/7");
  REQUIRE(bnd::to_string(rational{5u, 1})  == "5");
  REQUIRE(bnd::to_string(rational{0u})     == "0");

  REQUIRE(bnd::to_string(rational{7,  -3}) == "-2 1/3");
  REQUIRE(bnd::to_string(rational{1,  -3}) == "-1/3");
}

TEST_CASE("rational to_string: overflow boundary falls back to fraction",
          "[format][rational][overflow]")
{
  constexpr umax M = std::numeric_limits<umax>::max();
  REQUIRE(bnd::to_string(rational{M, 2})     == "9223372036854775807 1/2");
  REQUIRE(bnd::to_string(rational{M, -2})    == "-9223372036854775807 1/2");
  REQUIRE(bnd::to_string(rational{M / 5, 2}) == "1844674407370955161.5");
}

TEST_CASE("rational max as integer formats without 1/", "[format][rational]")
{
  constexpr umax M = std::numeric_limits<umax>::max();
  REQUIRE(bnd::to_string(rational{M, 1}) == std::to_string(M));
}

TEST_CASE("std::format integration", "[format][std_format]")
{
  REQUIRE(std::format("{}",      bound<{0, 99}>{42})    == "42");
  REQUIRE(std::format("[{:>6}]", bound<{0, 99}>{42})    == "[    42]");
  REQUIRE(std::format("[{:<6}]", bound<{0, 99}>{42})    == "[42    ]");
  REQUIRE(std::format("[{:0>4}]",bound<{0, 99}>{42})    == "[0042]");
  REQUIRE(std::format("{}",      rational{1u, 2})       == "0.5");
}

TEST_CASE("interval and grid to_string", "[format][interval][grid]")
{
  REQUIRE(bnd::to_string(interval{0, 10})           == "[0..10]");
  REQUIRE(bnd::to_string(grid{interval{0, 10}, 1_r}) == "{[0..10], 1}");
}
