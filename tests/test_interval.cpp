#include "bound/bound.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bnd;

TEST_CASE("interval construction and predicates", "[interval]")
{
  SECTION("point interval")
  {
    constexpr interval p{*(3_r/4), *(3_r/4)};
    REQUIRE(p.Lower == p.Upper);
    REQUIRE(p.includes(*(3_r/4)));
    REQUIRE_FALSE(p.includes(0));
  }

  SECTION("includes / excludes / overlaps")
  {
    interval a{0, 10};
    interval b{5, 15};
    interval c{20, 30};

    REQUIRE(a.includes(5));
    REQUIRE(a.includes(0));
    REQUIRE(a.includes(10));
    REQUIRE_FALSE(a.includes(11));

    REQUIRE(a.overlaps(b));
    REQUIRE_FALSE(a.excludes(b));
    REQUIRE(a.excludes(c));
    REQUIRE_FALSE(a.overlaps(c));
  }

  SECTION("includes whole sub-interval")
  {
    interval outer{0, 100};
    interval inner{10, 90};
    REQUIRE(outer.includes(inner));
    REQUIRE_FALSE(inner.includes(outer));
  }
}

TEST_CASE("interval arithmetic via lift", "[interval]")
{
  interval a{0, 10};
  interval b{2, 5};

  SECTION("addition")
  {
    auto r = a + b;
    REQUIRE(r.has_value());
    REQUIRE(r->Lower == 2);
    REQUIRE(r->Upper == 15);
  }

  SECTION("subtraction propagates via -rhs")
  {
    auto r = a - b;
    REQUIRE(r.has_value());
    REQUIRE(r->Lower == -5);
    REQUIRE(r->Upper == 8);
  }

  SECTION("multiplication takes the bounding box")
  {
    interval x{-2, 3};
    interval y{-4, 5};
    auto r = x * y;
    REQUIRE(r.has_value());
    REQUIRE(r->Lower == -12);     // -2 * 6 ... actually min(-2*-4, -2*5, 3*-4, 3*5) = min(8,-10,-12,15) = -12
    REQUIRE(r->Upper == 15);
  }

  SECTION("division excludes zero in divisor")
  {
    interval c{-1, 1};            // contains 0
    auto r = a / c;
    REQUIRE_FALSE(r.has_value());
  }

  SECTION("division by positive interval")
  {
    interval pos{2, 5};           // strictly positive
    auto r = a / pos;
    REQUIRE(r.has_value());
  }
}

TEST_CASE("interval ordering", "[interval][ordering]")
{
  interval a{0, 10};
  interval b{20, 30};
  interval c{5, 15};
  interval d{0, 10};

  REQUIRE(a < b);
  REQUIRE(b > a);
  REQUIRE(a == d);
  // Overlapping but non-equal → unordered
  auto cmp = (a <=> c);
  REQUIRE(cmp == std::partial_ordering::unordered);
}

TEST_CASE("interval / rational notch", "[interval][grid]")
{
  interval a{0, 10};

  SECTION("divides_evenly")
  {
    REQUIRE(a.divides_evenly(rational{1u}));
    REQUIRE(a.divides_evenly(rational{1u, 2}));
    REQUIRE_FALSE(a.divides_evenly(rational{3u}));
  }

  SECTION("operator/ rational gives notch count")
  {
    auto r = a / rational{1u};
    REQUIRE(r.has_value());
    REQUIRE(*r == rational{10u});

    auto r2 = a / rational{1u, 2};
    REQUIRE(r2.has_value());
    REQUIRE(*r2 == rational{20u});
  }
}

TEST_CASE("interval sentinel", "[interval][sentinel]")
{
  // sentinel_traits<interval> reports sentinel via sentinel_traits<rational>
  REQUIRE(slim::optional<interval>{}.has_value() == false);
}
