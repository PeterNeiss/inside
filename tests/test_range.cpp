#include "bound/range.hpp"
#include "bound/io.hpp"

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

//---------------------------------------------------------------------------
// storage-kind decode — operator* takes integer fast arms for index/value
// raws (see range.hpp); every arm must agree with the analytic
// Lower + index·Notch, and the ctor must invert it. [perf-paths] pins which
// arm each representative type dispatches to.
//---------------------------------------------------------------------------
namespace
{
  template <typename RangeType>
  void require_decodes_analytically()
  {
    using value_type = typename RangeType::value_type;
    RangeType r;
    std::size_t position = 0;
    for (auto b : r)
    {
      rational expected = (Lower<value_type>
          + (rational{position} * Notch<value_type>).value()).value();
      REQUIRE(as_rational(b) == expected);
      ++position;
    }
    REQUIRE(position == r.size());
  }
}

TEST_CASE("bound_range: decode agrees with Lower + i*Notch on every storage kind",
          "[range][perf-paths]")
{
  require_decodes_analytically<bound_range<{0, 999}>>();                    // value raw
  require_decodes_analytically<bound_range<{-500, 500}>>();                 // value raw, signed
  require_decodes_analytically<bound_range<{{0, 4}, notch<1, 256>}>>();     // index raw
  require_decodes_analytically<bound_range<{{-2, 2}, notch<1, 4>}>>();      // index raw, offset Lower
  require_decodes_analytically<bound_range<{0, 100}, sentinel>>();          // sentinel slot excluded
  require_decodes_analytically<bound_range<{{0, 2}, notch<1, 3>}, exact>>();          // rational raw fallback
#ifndef BND_MATH_FIXED   // under BND_MATH_FIXED the real storage arm is elided
  require_decodes_analytically<bound_range<{{0, 4}, notch<1, 256>}, real | round_nearest>>(); // fp raw fallback
#endif
}

TEST_CASE("bound_range: start ctor inverts the decode on every storage kind",
          "[range][perf-paths]")
{
  auto first_equals_start = [](auto range_tag, auto start_value) {
    using RangeType = decltype(range_tag);
    typename RangeType::value_type start{start_value};
    RangeType r{start};
    REQUIRE(as_rational(*r.begin()) == as_rational(start));
  };
  first_equals_start(bound_range<{0, 999}>{}, 500);
  first_equals_start(bound_range<{{0, 4}, notch<1, 256>}>{}, 2);
  first_equals_start(bound_range<{{0, 2}, notch<1, 3>}, exact>{}, 1);
}

TEST_CASE("bound_range: sentinel policy never yields the sentinel slot", "[range][perf-paths]")
{
  for (auto b : bound_range<{0, 100}, sentinel>{})
    REQUIRE(!b.is_sentinel());
}

TEST_CASE("bound_range: fast decode arms engage (dispatch pins)", "[range][perf-paths]")
{
  // The operator* fast arms are gated on the storage kind; these pins fail if
  // a storage-selection change silently reroutes a type to another arm.
  STATIC_REQUIRE(value_raw<bound_range<{0, 999}>::value_type>);
  STATIC_REQUIRE(index_raw<bound_range<{{0, 4}, notch<1, 256>}>::value_type>);
  STATIC_REQUIRE(index_raw<bound_range<{{-2, 2}, notch<1, 4>}>::value_type>);
  STATIC_REQUIRE(rational_raw<bound_range<{{0, 2}, notch<1, 3>}, exact>::value_type>);
#ifndef BND_MATH_FIXED   // under BND_MATH_FIXED the real storage arm is elided
  STATIC_REQUIRE(fp_raw<bound_range<{{0, 4}, notch<1, 256>}, real | round_nearest>::value_type>);
#endif
}
