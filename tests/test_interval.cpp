#include "bound/bound.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bnd;
using namespace bnd::detail;

TEST_CASE("interval structured binding", "[interval][structured_binding]")
{
  STATIC_REQUIRE(std::tuple_size_v<interval> == 2);
  STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<0, interval>, rational>);

  interval iv{-3, 5};
  auto [lo, hi] = iv;
  REQUIRE(lo == -3);
  REQUIRE(hi ==  5);

  // Mutating through structured binding writes back to source.
  auto& [mlo, mhi] = iv;
  mlo = -7;
  REQUIRE(iv.Lower == -7);
}

TEST_CASE("interval construction and predicates", "[interval]")
{
  SECTION("point interval")
  {
    constexpr interval p{*(3_r/4), *(3_r/4)};
    STATIC_REQUIRE(p.Lower == p.Upper);
    STATIC_REQUIRE(includes(p, *(3_r/4)));
    STATIC_REQUIRE_FALSE(includes(p, 0));
  }

  SECTION("includes / excludes / overlaps")
  {
    constexpr interval a{0, 10};
    constexpr interval b{5, 15};
    constexpr interval c{20, 30};

    STATIC_REQUIRE(includes(a, 5));
    STATIC_REQUIRE(includes(a, 0));
    STATIC_REQUIRE(includes(a, 10));
    STATIC_REQUIRE_FALSE(includes(a, 11));

    STATIC_REQUIRE(overlaps(a, b));
    STATIC_REQUIRE_FALSE(excludes(a, b));
    STATIC_REQUIRE(excludes(a, c));
    STATIC_REQUIRE_FALSE(overlaps(a, c));
  }

  SECTION("includes whole sub-interval")
  {
    constexpr interval outer{0, 100};
    constexpr interval inner{10, 90};
    STATIC_REQUIRE(includes(outer, inner));
    STATIC_REQUIRE_FALSE(includes(inner, outer));
  }
}

TEST_CASE("interval arithmetic via lift", "[interval]")
{
  constexpr interval a{0, 10};
  constexpr interval b{2, 5};

  SECTION("addition")
  {
    constexpr auto r = a + b;
    STATIC_REQUIRE(r.has_value());
    STATIC_REQUIRE(r->Lower == 2);
    STATIC_REQUIRE(r->Upper == 15);
  }

  SECTION("subtraction propagates via -rhs")
  {
    constexpr auto r = a - b;
    STATIC_REQUIRE(r.has_value());
    STATIC_REQUIRE(r->Lower == -5);
    STATIC_REQUIRE(r->Upper == 8);
  }

  SECTION("multiplication takes the bounding box")
  {
    constexpr interval x{-2, 3};
    constexpr interval y{-4, 5};
    constexpr auto r = x * y;
    STATIC_REQUIRE(r.has_value());
    STATIC_REQUIRE(r->Lower == -12);     // -2 * 6 ... actually min(-2*-4, -2*5, 3*-4, 3*5) = min(8,-10,-12,15) = -12
    STATIC_REQUIRE(r->Upper == 15);
  }

  SECTION("division excludes zero in divisor")
  {
    constexpr interval c{-1, 1};            // contains 0
    constexpr auto r = a / c;
    STATIC_REQUIRE_FALSE(r.has_value());
  }

  SECTION("division by positive interval")
  {
    constexpr interval pos{2, 5};           // strictly positive
    constexpr auto r = a / pos;
    STATIC_REQUIRE(r.has_value());
  }
}

TEST_CASE("interval ordering", "[interval][ordering]")
{
  constexpr interval a{0, 10};
  constexpr interval b{20, 30};
  constexpr interval c{5, 15};
  constexpr interval d{0, 10};

  STATIC_REQUIRE(a < b);
  STATIC_REQUIRE(b > a);
  STATIC_REQUIRE(a == d);
  // Overlapping but non-equal → unordered
  STATIC_REQUIRE((a <=> c) == std::partial_ordering::unordered);
}

TEST_CASE("interval / rational notch", "[interval][grid]")
{
  constexpr interval a{0, 10};

  SECTION("divides_evenly")
  {
    STATIC_REQUIRE(a.divides_evenly(1_r));
    STATIC_REQUIRE(a.divides_evenly(0.5_r));
    STATIC_REQUIRE_FALSE(a.divides_evenly(3_r));
  }

  SECTION("operator/ rational gives notch count")
  {
    constexpr auto r = a / 1_r;
    STATIC_REQUIRE(r.has_value());
    STATIC_REQUIRE(*r == 10);

    constexpr auto r2 = a / 0.5_r;
    STATIC_REQUIRE(r2.has_value());
    STATIC_REQUIRE(*r2 == 20);
  }
}

TEST_CASE("interval sentinel", "[interval][sentinel]")
{
  // sentinel_traits<interval> reports sentinel via sentinel_traits<rational>
  REQUIRE_FALSE(slim::optional<interval>{}.has_value());
}
