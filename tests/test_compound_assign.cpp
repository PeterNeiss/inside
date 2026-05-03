#include "bound/bound.hpp"
#include "bound/print.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bnd;

TEST_CASE("compound assignment: int RHS", "[bound][compound]")
{
  using u100 = bound<{0, 100}>;
  u100 a{50};

  a -= 10;
  REQUIRE(static_cast<rational>(a) == 40);

  a *= 2;
  REQUIRE(static_cast<rational>(a) == 80);

  a /= 3;
  REQUIRE(static_cast<rational>(a) == 26);   // truncation toward zero

  a /= 2;
  REQUIRE(static_cast<rational>(a) == 13);

  u100 b{17};
  b %= 5;
  REQUIRE(static_cast<rational>(b) == 2);

  u100 c{100};
  c %= 10;
  REQUIRE(static_cast<rational>(c) == 0);
}

TEST_CASE("compound assignment: boundable RHS", "[bound][compound]")
{
  using u100 = bound<{0, 100}>;
  u100 a{50};
  u100 delta{20};
  a -= delta;
  REQUIRE(static_cast<rational>(a) == 30);

  using ui = bound<{0, 100}, ignore_round>;
  ui d{50}, two{2};
  d *= two;
  REQUIRE(static_cast<rational>(d) == 100);

  ui e{60}, three{3};
  e /= three;
  REQUIRE(static_cast<rational>(e) == 20);

  ui f{17}, five{5};
  f %= five;
  REQUIRE(static_cast<rational>(f) == 2);
}

TEST_CASE("compound /= 0 reports error by default", "[bound][compound]")
{
  using u100 = bound<{0, 100}>;
  u100 a{50};
  REQUIRE_THROWS_AS(([&]{ a /= 0; }()), std::system_error);
  REQUIRE_THROWS_AS(([&]{ a %= 0; }()), std::system_error);
}

TEST_CASE("increment / decrement", "[bound][compound][inc]")
{
  using u10 = bound<{0, 10}>;
  u10 a{5};
  ++a;  REQUIRE(static_cast<rational>(a) == 6);
  a++;  REQUIRE(static_cast<rational>(a) == 7);
  --a;  REQUIRE(static_cast<rational>(a) == 6);
  a--;  REQUIRE(static_cast<rational>(a) == 5);
}
