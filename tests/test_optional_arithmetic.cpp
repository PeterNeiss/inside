#include "bound/bound.hpp"
#include "bound/rational.hpp"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using namespace bnd;

namespace
{
  using opt_r = slim::optional<rational>;
}

TEST_CASE("optional<rational> * optional<rational>", "[optional][rational][arith]")
{
  opt_r a{0.75_r};
  opt_r b{rational{2, 3}};

  auto r = a * b;
  REQUIRE(r.has_value());
  REQUIRE(*r == 0.5_r);

  opt_r empty{slim::nullopt};
  REQUIRE_FALSE((empty * b).has_value());
  REQUIRE_FALSE((a * empty).has_value());
  REQUIRE_FALSE((empty * empty).has_value());
}

TEST_CASE("optional<rational> + optional<rational>", "[optional][rational][arith]")
{
  opt_r a{0.25_r};
  opt_r b{0.5_r};

  auto r = a + b;
  REQUIRE(r.has_value());
  REQUIRE(*r == 0.75_r);

  opt_r empty{slim::nullopt};
  REQUIRE_FALSE((empty + a).has_value());
  REQUIRE_FALSE((a + empty).has_value());
}

TEST_CASE("optional<rational> - optional<rational>", "[optional][rational][arith]")
{
  opt_r a{0.75_r};
  opt_r b{0.5_r};
  auto r = a - b;
  REQUIRE(r.has_value());
  REQUIRE(*r == 0.25_r);
}

TEST_CASE("optional<rational> / optional<rational>", "[optional][rational][arith]")
{
  opt_r a{0.75_r};
  opt_r b{0.5_r};
  auto r = a / b;
  REQUIRE(r.has_value());
  REQUIRE(*r == 1.5_r);

  opt_r zero{rational{0}};
  REQUIRE_FALSE((a / zero).has_value());
}

TEST_CASE("optional<rational> op arithmetic and symmetric", "[optional][rational][arith]")
{
  opt_r a{0.5_r};

  REQUIRE(*(a + 1) == 1.5_r);
  REQUIRE(*(1 + a) == 1.5_r);
  REQUIRE(*(a - 1) == -0.5_r);
  REQUIRE(*(1 - a) == 0.5_r);
  REQUIRE(*(a * 2) == rational{1, 1});
  REQUIRE(*(2 * a) == rational{1, 1});
  REQUIRE(*(a / 2) == 0.25_r);
  REQUIRE(*(2 / a) == rational{4, 1});

  opt_r empty{slim::nullopt};
  REQUIRE_FALSE((empty + 1).has_value());
  REQUIRE_FALSE((1 + empty).has_value());
}

TEST_CASE("unary -optional<rational>", "[optional][rational][arith]")
{
  opt_r a{0.75_r};
  auto r = -a;
  REQUIRE(r.has_value());
  REQUIRE(*r == -0.75_r);

  opt_r empty{slim::nullopt};
  REQUIRE_FALSE((-empty).has_value());
}

TEST_CASE("bound construction from optional<rational> — sink unwrap", "[optional][bound][ctor]")
{
  using b_t = bound<{{0, 1}, notch<1, 16>}, round_nearest>;

  opt_r ok{0.5_r};
  b_t  v{ok};
  REQUIRE(rational{v} == 0.5_r);

  opt_r empty{slim::nullopt};
  REQUIRE_THROWS_AS(b_t{empty}, slim::bad_optional_access);
}

TEST_CASE("bound operator= from optional<rational>", "[optional][bound][assign]")
{
  using b_t = bound<{{0, 1}, notch<1, 16>}, round_nearest>;

  b_t v{0};
  opt_r ok{0.25_r};
  v = ok;
  REQUIRE(rational{v} == 0.25_r);

  opt_r empty{slim::nullopt};
  REQUIRE_THROWS_AS(v = empty, slim::bad_optional_access);
}

TEST_CASE("optional<bound> + rational propagates nullopt", "[optional][bound][rational][arith]")
{
  using b_t = bound<{{0, 1}, notch<1, 16>}, round_nearest>;
  slim::optional<b_t> some{b_t{0.5_r}};

  auto r = some + 0.25_r;
  REQUIRE(r.has_value());
  REQUIRE(*r == 0.75_r);

  slim::optional<b_t> empty{slim::nullopt};
  REQUIRE_FALSE((empty + 0.25_r).has_value());
  REQUIRE_FALSE((0.25_r + empty).has_value());
  REQUIRE_FALSE((empty - 0.25_r).has_value());
  REQUIRE_FALSE((empty * 0.25_r).has_value());
  REQUIRE_FALSE((empty / 0.25_r).has_value());
}
