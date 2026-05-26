#include "bound/bound.hpp"
#include "bound/print.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bnd;

//---------------------------------------------------------------------------
// with_clamp
//---------------------------------------------------------------------------
TEST_CASE("with_clamp: integer rhs, both directions", "[bound][with][clamp]")
{
  using u100 = bound<{0, 100}>;
  u100 x{50};

  x.with_clamp() = 150;     REQUIRE(x == 100);
  x.with_clamp() = -50;     REQUIRE(x == 0);
  x.with_clamp() = 75;      REQUIRE(x == 75);   // in-range passes through
}

TEST_CASE("with_clamp: float rhs", "[bound][with][clamp]")
{
  using u10 = bound<{{0, 10}, 0.5}>;
  u10 x{5};

  x.with_clamp() = 100.0;   REQUIRE(x == 10);
  x.with_clamp() = -5.0;    REQUIRE(x == 0);
  x.with_clamp() = 3.5;     REQUIRE(x == 3.5_r);
}

TEST_CASE("with_clamp: bound rhs", "[bound][with][clamp][bound2bound]")
{
  using wide   = bound<{0, 200}>;
  using narrow = bound<{0, 100}>;
  narrow n{0};
  wide   over{150};

  n.with_clamp() = over;    REQUIRE(n == 100);

  wide under_zero{0};
  n.with_clamp() = under_zero;  REQUIRE(n == 0);
}

TEST_CASE("with_clamp: signed bound", "[bound][with][clamp][signed]")
{
  using s = bound<{-100, 100}>;
  s x{0};

  x.with_clamp() = 200;     REQUIRE(x == 100);
  x.with_clamp() = -200;    REQUIRE(x == -100);
}

//---------------------------------------------------------------------------
// with_wrap
//---------------------------------------------------------------------------
TEST_CASE("with_wrap: integer rhs, both directions", "[bound][with][wrap]")
{
  using u10 = bound<{0, 9}>;
  u10 x{5};

  x.with_wrap() = 13;       REQUIRE(x == 3);   // 13 % 10
  x.with_wrap() = 23;       REQUIRE(x == 3);   // 23 % 10
  x.with_wrap() = -1;       REQUIRE(x == 9);   // -1 wraps to top
  x.with_wrap() = -11;      REQUIRE(x == 9);
  x.with_wrap() = 5;        REQUIRE(x == 5);   // in-range
}

TEST_CASE("with_wrap: signed bound", "[bound][with][wrap][signed]")
{
  using s = bound<{-5, 5}>;   // 11 values
  s x{0};

  // 6 ≡ -5 (mod 11) within [-5, 5]
  x.with_wrap() = 6;        REQUIRE(x == -5);
  // 17 = 6 + 11 ≡ -5 (mod 11)
  x.with_wrap() = 17;       REQUIRE(x == -5);
  // -6 ≡ 5 (mod 11)
  x.with_wrap() = -6;       REQUIRE(x == 5);
}

TEST_CASE("with_wrap: bound rhs", "[bound][with][wrap][bound2bound]")
{
  using src = bound<{0, 100}>;
  using dst = bound<{0, 9}>;
  dst d{0};

  src high{45};
  d.with_wrap() = high;     REQUIRE(d == 5);   // 45 % 10
}

//---------------------------------------------------------------------------
// with_truncate (truncates toward zero)
//---------------------------------------------------------------------------
TEST_CASE("with_truncate: bound rhs with finer notch", "[bound][with][truncate]")
{
  using coarse = bound<{{0, 10}, 2}>;
  using fine   = bound<{{0, 10}, 1}>;

  coarse c{0};
  c.with_truncate() = fine{3};   REQUIRE(c == 2);   // 3 truncates toward zero -> 2
  c.with_truncate() = fine{4};   REQUIRE(c == 4);   // exact
  c.with_truncate() = fine{5};   REQUIRE(c == 4);   // 5 -> 4
  c.with_truncate() = fine{7};   REQUIRE(c == 6);   // 7 -> 6
}

TEST_CASE("with_truncate: float rhs not on notch", "[bound][with][truncate]")
{
  using coarse = bound<{{0, 10}, 2}>;
  coarse c{0};

  c.with_truncate() = 3.0;       REQUIRE(c == 2);   // 3 truncates to 2
  c.with_truncate() = 3.99;      REQUIRE(c == 2);
  c.with_truncate() = 4.0;       REQUIRE(c == 4);   // exact
}

//---------------------------------------------------------------------------
// with_round_nearest (round half away)
//---------------------------------------------------------------------------
TEST_CASE("with_round_nearest: float rhs", "[bound][with][round_nearest]")
{
  using coarse = bound<{{0, 10}, 2}>;
  coarse c{0};

  c.with_round_nearest() = 3.0;    REQUIRE(c == 4);   // halfway-up: 3 -> 4
  c.with_round_nearest() = 2.99;   REQUIRE(c == 2);
  c.with_round_nearest() = 3.01;   REQUIRE(c == 4);
  c.with_round_nearest() = 4.0;    REQUIRE(c == 4);   // exact

  using half = bound<{{0, 10}, 0.5}>;
  half h;
  h.with_round_nearest() = 3.3;    REQUIRE(h == 3.5_r);  // 3.5
  h.with_round_nearest() = 3.2;    REQUIRE(h == 3);                // 3.0
}

TEST_CASE("with_round_nearest: bound rhs incompatible notches",
          "[bound][with][round_nearest][bound2bound]")
{
  using coarse = bound<{{0, 10}, 2}>;
  using fine   = bound<{{0, 10}, 1}>;

  coarse c{0};
  c.with_round_nearest() = fine{3};   REQUIRE(c == 4);   // 3 -> 4 (round half up)
  c.with_round_nearest() = fine{5};   REQUIRE(c == 6);   // 5 -> 6
  c.with_round_nearest() = fine{4};   REQUIRE(c == 4);   // exact
}

//---------------------------------------------------------------------------
// with_truncate vs with_round_nearest divergence
//---------------------------------------------------------------------------
TEST_CASE("with_truncate vs with_round_nearest disagree on halfway",
          "[bound][with][truncate]")
{
  using coarse = bound<{{0, 10}, 2}>;
  coarse c{0};

  c.with_truncate()         = 3.0;   REQUIRE(c == 2);   // toward zero
  c.with_round_nearest() = 3.0;   REQUIRE(c == 4);   // half away from zero
}
