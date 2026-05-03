#include "bound/bound.hpp"
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
    sum += static_cast<imax>(i);
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
    if (count == 0) first = static_cast<imax>(i);
    last = static_cast<imax>(i);
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
    if (count == 0) first = static_cast<imax>(i);
    last = static_cast<imax>(i);
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
    REQUIRE(static_cast<imax>(i) == 5);
    ++count;
  }
  REQUIRE(count == 1);
}

TEST_CASE("bound_range size 1 with negative point", "[bound][range][edge]")
{
  int count = 0;
  for (auto i : bound_range<{-3, -3}>{})
  {
    REQUIRE(static_cast<imax>(i) == -3);
    ++count;
  }
  REQUIRE(count == 1);
}

TEST_CASE("bound<{x,x}> singleton round-trips its value",
          "[bound][edge][singleton]")
{
  using point_pos = bound<{5, 5}>;
  point_pos a{5};
  REQUIRE(static_cast<rational>(a) == 5);
  REQUIRE(static_cast<imax>(a) == 5);

  using point_neg = bound<{-3, -3}>;
  point_neg b{-3};
  REQUIRE(static_cast<rational>(b) == -3);
  REQUIRE(static_cast<imax>(b) == -3);

  // Float assignment to single-value rational-storage grid (notch=0)
  using point_fp = bound<{2.5_r}>;
  point_fp c = 2.5;
  REQUIRE(static_cast<rational>(c) == rational{5u, 2});
}

TEST_CASE("default-constructed bound is well-formed", "[bound][default]")
{
  bound b;
  (void)b;
  SUCCEED("default ctor compiles");
}
