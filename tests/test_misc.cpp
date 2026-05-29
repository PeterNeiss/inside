#include "bound/bound.hpp"
#include "bound/numeric_limits.hpp"
#include "bound/print.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bnd;

TEST_CASE("implicit cast to imax", "[bound][cast]")
{
  using idx = bound<{0, 9}>;
  idx a{5};
  imax val = a;
  REQUIRE(val == 5);

  using idx2 = bound<{1, 10}>;
  idx2 b{7};
  imax val2 = b;
  REQUIRE(val2 == 7);

  // array subscript without explicit cast
  int arr[] = {10, 20, 30, 40, 50};
  using ai = bound<{0, 4}>;
  ai c{3};
  REQUIRE(arr[c] == 40);
}

TEST_CASE("bound_range sequential", "[bound][range]")
{
  int count = 0;
  imax sum = 0;
  for (auto i : bound_range<{0, 9}>{})
  {
    imax v = i;        // implicit bound -> imax (direct-init picks imax over size_t)
    sum += v;
    ++count;
  }
  REQUIRE(count == 10);
  REQUIRE(sum   == 45);
}

TEST_CASE("bound_range wrapping start", "[bound][range][wrap]")
{
  int count = 0;
  imax first = -1, last = -1;
  for (auto i : bound_range<{0, 9}>{7})
  {
    if (count == 0) first = i;
    last = i;
    ++count;
  }
  REQUIRE(count == 10);
  REQUIRE(first ==  7);
  REQUIRE(last  ==  6);
}

TEST_CASE("bound_range over signed bound", "[bound][range][signed]")
{
  int count = 0;
  imax first = 0, last = 0;
  for (auto i : bound_range<{-2, 2}>{})
  {
    if (count == 0) first = i;
    last = i;
    ++count;
  }
  REQUIRE(count == 5);
  REQUIRE(first == -2);
  REQUIRE(last  ==  2);
}

TEST_CASE("bound_range size 1", "[bound][range][edge]")
{
  int count = 0;
  for (auto i : bound_range<{5, 5}>{})
  {
    REQUIRE(i == 5);
    ++count;
  }
  REQUIRE(count == 1);
}

TEST_CASE("bound_range size 1 with negative point", "[bound][range][edge]")
{
  int count = 0;
  for (auto i : bound_range<{-3, -3}>{})
  {
    REQUIRE(i == -3);
    ++count;
  }
  REQUIRE(count == 1);
}

TEST_CASE("bound<{x,x}> singleton round-trips its value",
          "[bound][edge][singleton]")
{
  using point_pos = bound<{5, 5}>;
  point_pos a{5};
  REQUIRE(a == 5);
  imax av = a;            // implicit
  REQUIRE(av == 5);

  using point_neg = bound<{-3, -3}>;
  point_neg b{-3};
  REQUIRE(b == -3);
  imax bv = b;
  REQUIRE(bv == -3);

  // Float assignment to single-value rational-storage grid (notch=0)
  using point_fp = bound<{2.5_r}>;
  point_fp c = 2.5;
  REQUIRE(c == 2.5_r);
}

TEST_CASE("default-constructed bound is well-formed", "[bound][default]")
{
  bound b;
  (void)b;
  SUCCEED("default ctor compiles");
}

TEST_CASE("numeric_limits epsilon / round_error report 0 for exact types",
          "[bound][numeric_limits]")
{
  // 0 is in the interval and on the grid → epsilon == 0.
  using u8 = bound<{0, 255}>;
  STATIC_REQUIRE(std::numeric_limits<u8>::epsilon()     == 0);
  STATIC_REQUIRE(std::numeric_limits<u8>::round_error() == 0);

  using i8 = bound<{-100, 100}>;
  STATIC_REQUIRE(std::numeric_limits<i8>::epsilon()     == 0);
  STATIC_REQUIRE(std::numeric_limits<i8>::round_error() == 0);

  using half = bound<{{-40, 60}, 0.5_r}>;
  STATIC_REQUIRE(std::numeric_limits<half>::epsilon()     == 0);
  STATIC_REQUIRE(std::numeric_limits<half>::round_error() == 0);

  // Rational raw (Notch = 0) — still exact, epsilon = 0.
  using r01 = bound<{{0_r, 1_r}, 0}>;
  STATIC_REQUIRE(std::numeric_limits<r01>::epsilon() == 0);

  // 0 is *outside* the interval — fall back to Lower (the closest representable).
  using above_zero = bound<{10, 100}>;
  STATIC_REQUIRE(std::numeric_limits<above_zero>::epsilon()     == 10);
  STATIC_REQUIRE(std::numeric_limits<above_zero>::round_error() == 10);
}
