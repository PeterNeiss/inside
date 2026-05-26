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
  using u100 = bound<{0, 100}>;
  STATIC_REQUIRE([]{ u100 a{50}, d{20}; a -= d; return a.value(); }() == 30);

  using ui = bound<{0, 100}, ignore_round>;
  STATIC_REQUIRE([]{ ui d{50}, two{2}; d *= two; return d.value(); }() == 100);
  STATIC_REQUIRE([]{ ui e{60}, three{3}; e /= three; return e.value(); }() == 20);
  STATIC_REQUIRE([]{ ui f{17}, five{5}; f %= five; return f.value(); }() == 2);
}

TEST_CASE("compound /= 0 reports error by default", "[bound][compound]")
{
  using u100 = bound<{0, 100}>;
  u100 a{50};
  REQUIRE_THROWS_AS(([&]{ a /= 0; }()), std::system_error);
  REQUIRE_THROWS_AS(([&]{ a %= 0; }()), std::system_error);
}

TEST_CASE("compound assignment: rational RHS", "[bound][compound][rational]")
{
  // round_nearest required so the bound's rational/float assignment paths
  // are available.
  using rn = bound<{{0, 100}, notch<1, 100>}, round_nearest>;

  rn a{0.5_r};
  a += 0.25_r;                       // 0.50 + 0.25 = 0.75
  REQUIRE(a == rn{0.75_r});

  a -= 0.25_r;                       // 0.75 − 0.25 = 0.50
  REQUIRE(a == rn{0.5_r});

  a *= 4_r;                                  // 0.50 × 4 = 2
  REQUIRE(a == rn{2_r});

  a /= 2_r;                                  // 2 / 2 = 1
  REQUIRE(a == rn{1_r});
}

TEST_CASE("compound assignment: floating-point RHS", "[bound][compound][float]")
{
  using rn = bound<{{-100, 100}, notch<1, 16>}, round_nearest>;

  rn a{1.0};
  a += 2.5;
  REQUIRE(a == rn{3.5});

  a -= 1.5;
  REQUIRE(a == rn{2.0});

  a *= 1.5;
  REQUIRE(a == rn{3.0});

  a /= 2.0;
  REQUIRE(a == rn{1.5});
}

TEST_CASE("compound /= 0 with real RHS reports error", "[bound][compound][real]")
{
  using rn = bound<{{0, 100}, notch<1, 100>}, round_nearest>;
  rn a{0.5_r};
  REQUIRE_THROWS_AS(([&]{ a /= 0.0; }()),          std::system_error);
  REQUIRE_THROWS_AS(([&]{ a /= 0_r; }()),  std::system_error);
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
