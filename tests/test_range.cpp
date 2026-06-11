#include "bound/range.hpp"
#include "bound/print.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <vector>

using namespace bnd;
using namespace bnd::detail;

namespace
{
  using small_grid = bound_range<{0, 4}>;        // 5 slots: 0,1,2,3,4
  using frac_grid  = bound_range<{{0, 2}, 0.5}>; // 5 slots: 0,0.5,1,1.5,2
  using small_it   = small_grid::iterator;
}

//---------------------------------------------------------------------------
// concept conformance
//---------------------------------------------------------------------------
static_assert(std::input_or_output_iterator<small_it>);
static_assert(std::forward_iterator<small_it>);
static_assert(std::bidirectional_iterator<small_it>);
static_assert(std::random_access_iterator<small_it>);

static_assert(std::ranges::range<small_grid>);
static_assert(std::ranges::forward_range<small_grid>);
static_assert(std::ranges::random_access_range<small_grid>);
static_assert(std::ranges::sized_range<small_grid>);

static_assert(std::ranges::random_access_range<frac_grid>);

//---------------------------------------------------------------------------
// basic iteration
//---------------------------------------------------------------------------
TEST_CASE("bound_range: default iteration walks every slot once", "[range]")
{
  small_grid r;
  std::vector<int> seen;
  for (auto b : r) seen.push_back(int(to_value(b)));
  REQUIRE(seen == std::vector<int>{0, 1, 2, 3, 4});
}

TEST_CASE("bound_range: mid-range start wraps around", "[range]")
{
  small_grid r{small_grid::value_type{2}};
  std::vector<int> seen;
  for (auto b : r) seen.push_back(int(to_value(b)));
  REQUIRE(seen == std::vector<int>{2, 3, 4, 0, 1});
}

TEST_CASE("bound_range: fractional notch iterates exact values", "[range]")
{
  frac_grid r;
  std::vector<rational> seen;
  for (auto b : r) seen.push_back(b);  // bound -> rational is implicit
  REQUIRE(seen == std::vector<rational>{0_r, 0.5_r, 1_r, 1.5_r, 2_r});
}

//---------------------------------------------------------------------------
// random-access arithmetic
//---------------------------------------------------------------------------
TEST_CASE("bound_range: iterator arithmetic", "[range][random_access]")
{
  small_grid r;
  auto b = r.begin();
  auto e = r.end();

  REQUIRE(e - b == 5);
  REQUIRE((b + 5) == e);
  REQUIRE((e - 5) == b);
  REQUIRE(int(to_value(*(b + 3))) == 3);
  REQUIRE(int(to_value(b[2]))     == 2);

  auto it = b + 2;
  REQUIRE(int(to_value(*it)) == 2);
  it += 2;
  REQUIRE(int(to_value(*it)) == 4);
  it -= 3;
  REQUIRE(int(to_value(*it)) == 1);
  REQUIRE(it - b == 1);
}

TEST_CASE("bound_range: ordering and equality", "[range][random_access]")
{
  small_grid r;
  auto b = r.begin();
  auto m = b + 2;
  auto e = r.end();

  REQUIRE(b == b);
  REQUIRE(b != m);
  REQUIRE(b <  m);
  REQUIRE(m <= m);
  REQUIRE(e >  m);
}

//---------------------------------------------------------------------------
// std::ranges interop
//---------------------------------------------------------------------------
TEST_CASE("bound_range: std::ranges::size", "[range][ranges]")
{
  small_grid r;
  REQUIRE(std::ranges::size(r) == 5);
}

TEST_CASE("bound_range: std::views::take", "[range][ranges]")
{
  small_grid r;
  std::vector<int> seen;
  for (auto b : r | std::views::take(3))
    seen.push_back(int(to_value(b)));
  REQUIRE(seen == std::vector<int>{0, 1, 2});
}

TEST_CASE("bound_range: std::views::reverse", "[range][ranges]")
{
  small_grid r;
  std::vector<int> seen;
  for (auto b : r | std::views::reverse)
    seen.push_back(int(to_value(b)));
  REQUIRE(seen == std::vector<int>{4, 3, 2, 1, 0});
}

TEST_CASE("bound_range: backwards iteration via operator--", "[range]")
{
  small_grid r;
  auto it = r.end();
  std::vector<int> seen;
  while (it != r.begin())
  {
    --it;
    seen.push_back(int(to_value(*it)));
  }
  REQUIRE(seen == std::vector<int>{4, 3, 2, 1, 0});
}

TEST_CASE("bound_range: postfix ++ / -- behave standard", "[range]")
{
  small_grid r;
  auto it = r.begin();
  auto snap = it++;
  REQUIRE(int(to_value(*snap)) == 0);
  REQUIRE(int(to_value(*it))   == 1);

  auto snap2 = it--;
  REQUIRE(int(to_value(*snap2)) == 1);
  REQUIRE(int(to_value(*it))    == 0);
}

TEST_CASE("bound_range::indexed pairs each value with its position", "[range][indexed]")
{
  small_grid r;
  std::vector<std::pair<imax, imax>> seen;
  for (auto [i, v] : r.indexed())
    seen.emplace_back(i, v);
  REQUIRE(seen == std::vector<std::pair<imax, imax>>{
    {0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}
  });
}

TEST_CASE("bound_range works with std::views::reverse", "[range][reverse]")
{
  small_grid r;   // 0,1,2,3,4
  std::vector<imax> seen;
  for (auto v : std::views::reverse(r))
    seen.push_back(v);
  REQUIRE(seen == std::vector<imax>{4, 3, 2, 1, 0});
}

TEST_CASE("bound_range::strided visits every step-th value", "[range][strided]")
{
  small_grid r;   // 0,1,2,3,4
  auto collect = [&](std::size_t step) {
    std::vector<imax> seen;
    for (auto v : r.strided(step)) seen.push_back(v);
    return seen;
  };
  REQUIRE(collect(1) == std::vector<imax>{0, 1, 2, 3, 4});
  REQUIRE(collect(2) == std::vector<imax>{0, 2, 4});
  REQUIRE(collect(3) == std::vector<imax>{0, 3});
  REQUIRE(collect(5) == std::vector<imax>{0});

  // Fractional grid strides over notch values too.
  using frac_grid = bound_range<{{0, 1}, notch<1, 4>}>;  // 0,.25,.5,.75,1
  frac_grid f;
  std::vector<rational> fseen;
  for (auto v : f.strided(2)) fseen.push_back(rational{v});
  REQUIRE(fseen == std::vector<rational>{rational{0}, rational{1, 2}, rational{1}});
}
