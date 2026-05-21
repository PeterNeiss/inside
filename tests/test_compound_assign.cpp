#include "bound/bound.hpp"
#include "bound/print.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bnd;

TEST_CASE("compound assignment: int RHS", "[bound][compound]")
{
  using u100 = bound<{0, 100}>;

  // Promote each chain to a constexpr lambda — every step is in range, so
  // the consteval throw branch in assignment::assign is dead code.
  STATIC_REQUIRE([]{ u100 a{50}; a -= 10; return a.value(); }() == 40);
  STATIC_REQUIRE([]{ u100 a{40}; a *=  2; return a.value(); }() == 80);
  STATIC_REQUIRE([]{ u100 a{80}; a /=  3; return a.value(); }() == 26);  // trunc toward zero
  STATIC_REQUIRE([]{ u100 a{26}; a /=  2; return a.value(); }() == 13);

  STATIC_REQUIRE([]{ u100 b{17};  b %=  5; return b.value(); }() ==  2);
  STATIC_REQUIRE([]{ u100 c{100}; c %= 10; return c.value(); }() ==  0);
}

TEST_CASE("compound assignment: boundable RHS", "[bound][compound]")
{
  // These operate on a widened intermediate (e.g. u100 * u100 has interval
  // [0, 10000]) so the assignment-back step trips the consteval guard
  // `not Interval<L>.includes(Interval<R>)` even when the runtime value is
  // in range. Runtime-only.
  using u100 = bound<{0, 100}>;
  u100 a{50};
  u100 delta{20};
  a -= delta;
  REQUIRE(a == 30);

  using ui = bound<{0, 100}, ignore_round>;
  ui d{50}, two{2};
  d *= two;
  REQUIRE(d == 100);

  ui e{60}, three{3};
  e /= three;
  REQUIRE(e == 20);

  ui f{17}, five{5};
  f %= five;
  REQUIRE(f == 2);
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
  STATIC_REQUIRE([]{ u10 a{5}; ++a; return a.value(); }() == 6);
  STATIC_REQUIRE([]{ u10 a{5}; a++; return a.value(); }() == 6);
  STATIC_REQUIRE([]{ u10 a{5}; --a; return a.value(); }() == 4);
  STATIC_REQUIRE([]{ u10 a{5}; a--; return a.value(); }() == 4);

  // post-inc/dec returns the old value
  STATIC_REQUIRE([]{ u10 a{5}; auto r = a++; return r.value(); }() == 5);
  STATIC_REQUIRE([]{ u10 a{5}; auto r = a--; return r.value(); }() == 5);
}
