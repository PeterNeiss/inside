#include "bound/bound.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <type_traits>

using namespace bnd;

TEST_CASE("grid construction and max_notch", "[grid]")
{
  STATIC_REQUIRE(grid{{0, 100}, 1}.max_notch() == 100);
  STATIC_REQUIRE(grid{{1, 5},   0.25}.max_notch() == 16);
  STATIC_REQUIRE(grid{{0, std::numeric_limits<umax>::max()}, 1}.max_notch()
                 == std::numeric_limits<umax>::max());
}

TEST_CASE("grid storage_min selection", "[grid][storage]")
{
  // Notch 0 -> rational
  STATIC_REQUIRE(std::is_same_v<storage_min<grid{{0,10}, 0}>, rational>);

  // Notch 1, lower 0 -> smallest unsigned that fits Upper
  STATIC_REQUIRE(std::is_same_v<storage_min<grid{0, 100,  1}>, std::uint8_t>);
  STATIC_REQUIRE(std::is_same_v<storage_min<grid{0, 1000, 1}>, std::uint16_t>);
  STATIC_REQUIRE(std::is_same_v<storage_min<grid{0, 100000, 1}>, std::uint32_t>);

  // Signed integer case (notch 1, negative lower)
  STATIC_REQUIRE(std::is_same_v<storage_min<grid{-127, 127, 1}>,    std::int8_t>);
  STATIC_REQUIRE(std::is_same_v<storage_min<grid{-32000, 32000, 1}>, std::int16_t>);
}

TEST_CASE("grid arithmetic", "[grid]")
{
  SECTION("add aligns notches via gcd")
  {
    grid a{{0, 10}, 1};
    grid b{{0,  5}, 1};
    auto r = a + b;
    REQUIRE(r.has_value());
    REQUIRE(r->Interval.Lower == 0);
    REQUIRE(r->Interval.Upper == 15);
    REQUIRE(r->Notch == rational{1u});
  }

  SECTION("multiply takes bounding box and lcm-ish notch")
  {
    grid a{{0, 10}, 1};
    grid b{{0,  5}, 1};
    auto r = a * b;
    REQUIRE(r.has_value());
    REQUIRE(r->Interval.Lower == 0);
    REQUIRE(r->Interval.Upper == 50);
  }

  SECTION("divide by zero-only interval yields nullopt")
  {
    grid a{{0, 10}, 1};
    grid zero{{0, 0}, 0};         // pure zero-point grid
    auto r = a / zero;
    REQUIRE_FALSE(r.has_value());
  }

  SECTION("divide by positive interval excluding zero")
  {
    grid a{{0, 10}, 1};
    grid b{{2,  5}, 1};
    auto r = a / b;
    REQUIRE(r.has_value());
  }
}

TEST_CASE("grid validate", "[grid]")
{
  STATIC_REQUIRE(grid::validate<grid{{0, 10},  1}>());
  STATIC_REQUIRE(grid::validate<grid{{0, 10},  rational{1u, 2}}>());
  STATIC_REQUIRE(grid::validate<grid{0_r}>());                   // point grid, notch=0
}

TEST_CASE("grid sentinel", "[grid][sentinel]")
{
  REQUIRE(slim::optional<grid>{}.has_value() == false);
  auto s = grid::make_sentinel();
  REQUIRE(s.Notch.Denominator == 0);
}
