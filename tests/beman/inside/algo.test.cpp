// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <numeric>
#include <ranges>
#include <vector>

namespace rng = std::ranges;
using namespace beman::inside;
using namespace beman::inside::detail;

namespace
{
  using cell  = beman::inside::inside<{0, 100, 0.5}>;
  using accum = beman::inside::inside<{0, 1000, 0.5}>;

  slim::optional<rational> opt_div_zero()
  { return 1_r / 0; }
}

// algo: optional rational from div by zero
TEST(AlgoTest, algo_optional_rational_from_div_by_zero)
{
  ASSERT_FALSE(opt_div_zero().has_value());
}

// algo: ranges algorithms over vector<inside>
TEST(AlgoTest, algo_ranges_algorithms_over_vector_inside)
{
  std::vector<cell> v;
  v.resize(10);
  rng::generate(v, [counter = 7]() mutable { return ++counter; });
  ASSERT_EQ(v.size(), 10);

  // reduce/accumulate
  auto sum = std::reduce(v.begin(), v.end(), accum{0}, std::plus<>{});
  ASSERT_EQ(sum, 8 + 9 + 10 + 11 + 12 + 13 + 14 + 15 + 16 + 17);

  // sort
  rng::sort(v);
  ASSERT_TRUE(rng::is_sorted(v));

  // find
  auto it = rng::find(v, cell{12});
  ASSERT_TRUE(it != v.end());

  // count_if
  auto above = rng::count_if(v, [](cell x){ return x > 10; });
  ASSERT_EQ(above, 7);

  // min/max
  ASSERT_EQ(*rng::min_element(v), 8);
  ASSERT_EQ(*rng::max_element(v), 17);

  // transform
  std::vector<accum> shifted(v.size());
  rng::transform(v, shifted.begin(), [](cell x){ return x + cell{1}; });
  ASSERT_EQ(shifted.front(), 9);
  ASSERT_EQ(shifted.back(), 18);

  // copy_if
  std::vector<cell> filtered;
  rng::copy_if(v, std::back_inserter(filtered), [](cell x){ return x > 12; });
  ASSERT_EQ(filtered.size(), 5);

  ASSERT_TRUE((rng::all_of(v,  [](cell x){ return x > 0;   })));
  ASSERT_TRUE((rng::any_of(v,  [](cell x){ return x == 10; })));
  ASSERT_TRUE((rng::none_of(v, [](cell x){ return x > 100; })));
}

// algo: classic STL algorithms
TEST(AlgoTest, algo_classic_stl_algorithms)
{
  std::vector<cell> v = {50, 10, 30, 20, 40};

  std::sort(v.begin(), v.end());
  ASSERT_EQ(v.front(), 10);
  ASSERT_EQ(v.back(), 50);

  std::stable_sort(v.begin(), v.end(), std::greater<>{});
  ASSERT_EQ(v.front(), 50);

  std::vector<cell> v3 = {70, 10, 50, 30, 90};
  std::nth_element(v3.begin(), v3.begin() + 2, v3.end());
  ASSERT_EQ(v3[2], 50);

  std::vector<cell> v4 = {20, 80, 10, 60, 90, 40};
  std::partial_sort(v4.begin(), v4.begin() + 3, v4.end(), std::greater<>{});
  ASSERT_EQ(v4[0], 90);
  ASSERT_EQ(v4[1], 80);
  ASSERT_EQ(v4[2], 60);

  // accumulate
  std::vector<cell> v5 = {1, 2, 3, 4, 5};
  auto acc = std::accumulate(v5.begin(), v5.end(), accum{0}, std::plus<>{});
  ASSERT_EQ(acc, 15);

  // reverse
  std::vector<cell> v6 = {1, 2, 3};
  std::reverse(v6.begin(), v6.end());
  ASSERT_EQ(v6[0], 3);
  ASSERT_EQ(v6[2], 1);

  // rotate
  std::vector<cell> v7 = {1, 2, 3, 4, 5};
  std::rotate(v7.begin(), v7.begin() + 2, v7.end());
  ASSERT_EQ(v7[0], 3);
  ASSERT_EQ(v7.back(), 2);

  // partition
  std::vector<cell> v8 = {10, 50, 20, 60, 30, 70};
  auto pivot = std::partition(v8.begin(), v8.end(), [](cell x){ return x > 30; });
  for (auto p = v8.begin(); p != pivot; ++p) ASSERT_TRUE(*p > 30);
  for (auto p = pivot;      p != v8.end(); ++p) ASSERT_TRUE(*p <= 30);

  // equal / mismatch
  std::vector<cell> a = {10, 20, 30};
  std::vector<cell> b = {10, 20, 30};
  ASSERT_TRUE((std::equal(a.begin(), a.end(), b.begin())));
  b[1] = 99;
  auto mm = std::mismatch(a.begin(), a.end(), b.begin());
  ASSERT_EQ(*mm.first, 20);
  ASSERT_EQ(*mm.second, 99);

  // iota
  std::vector<cell> iota_v(5);
  std::iota(iota_v.begin(), iota_v.end(), cell{10});
  ASSERT_EQ(iota_v.front(), 10);
  ASSERT_EQ(iota_v.back(), 14);
}

// beman::inside::min / max / midpoint over bounds
TEST(AlgoTest, beman_inside_min_max_midpoint_over_bounds)
{
  using pct = inside<{0, 100}>;
  pct a{30}, b{70};
  ASSERT_EQ((beman::inside::min(a, b)), 30);
  ASSERT_EQ((beman::inside::max(a, b)), 70);
  // midpoint is exact: (30+70)/2 = 50, lands on a refined grid.
  ASSERT_EQ((rational{beman::inside::midpoint(a, b)}), 50);

  // Exact midpoint of integer endpoints whose average is fractional.
  pct c{0}, d{1};
  ASSERT_EQ((rational{beman::inside::midpoint(c, d)}), (rational{1, 2}));  // no rounding, no overflow

  // ADL: unqualified call resolves to beman::inside::min for inside arguments.
  ASSERT_EQ((min(a, b)), 30);

  // Fractional grid.
  using s = inside<{{-1, 1}, notch<1, 16384>}, round_nearest>;
  s x{rational{1, 4}}, y{rational{3, 4}};
  ASSERT_EQ((rational{beman::inside::midpoint(x, y)}), (rational{1, 2}));
  ASSERT_EQ((beman::inside::max(x, y)), (rational{3, 4}));
}
