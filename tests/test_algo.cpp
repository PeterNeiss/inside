#include "bound/bound.hpp"
#include "bound/print.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <numeric>
#include <ranges>
#include <vector>

namespace rng = std::ranges;
using namespace bnd;

namespace
{
  using u8  = bnd::bound<{0, 100, 0.5}>;
  using u16 = bnd::bound<{0, 1000, 0.5}>;

  slim::optional<rational> opt_div_zero()
  { return rational{1u} / 0; }
}

TEST_CASE("algo: optional rational from div by zero", "[algo][rational]")
{
  REQUIRE_FALSE(opt_div_zero().has_value());
}

TEST_CASE("algo: ranges algorithms over vector<bound>", "[algo][ranges]")
{
  std::vector<u8> v;
  v.resize(10);
  rng::generate(v, [counter = 7] mutable { return ++counter; });
  REQUIRE(v.size() == 10);

  // reduce/accumulate
  auto sum = std::reduce(v.begin(), v.end(), u16{0}, std::plus<>{});
  REQUIRE(sum == 8 + 9 + 10 + 11 + 12 + 13 + 14 + 15 + 16 + 17);

  // sort
  rng::sort(v);
  REQUIRE(rng::is_sorted(v));

  // find
  auto it = rng::find(v, u8{12});
  REQUIRE(it != v.end());

  // count_if
  auto above = rng::count_if(v, [](u8 x){ return x > 10; });
  REQUIRE(above == 7);

  // min/max
  REQUIRE(*rng::min_element(v) ==  8);
  REQUIRE(*rng::max_element(v) == 17);

  // transform
  std::vector<u16> shifted(v.size());
  rng::transform(v, shifted.begin(), [](u8 x){ return x + u8{1}; });
  REQUIRE(shifted.front() == 9);
  REQUIRE(shifted.back()  == 18);

  // copy_if
  std::vector<u8> filtered;
  rng::copy_if(v, std::back_inserter(filtered), [](u8 x){ return x > 12; });
  REQUIRE(filtered.size() == 5);

  REQUIRE(rng::all_of(v,  [](u8 x){ return x > 0;   }));
  REQUIRE(rng::any_of(v,  [](u8 x){ return x == 10; }));
  REQUIRE(rng::none_of(v, [](u8 x){ return x > 100; }));
}

TEST_CASE("algo: classic STL algorithms", "[algo][stl]")
{
  std::vector<u8> v = {50, 10, 30, 20, 40};

  std::sort(v.begin(), v.end());
  REQUIRE(v.front() == 10);
  REQUIRE(v.back()  == 50);

  std::stable_sort(v.begin(), v.end(), std::greater<>{});
  REQUIRE(v.front() == 50);

  std::vector<u8> v3 = {70, 10, 50, 30, 90};
  std::nth_element(v3.begin(), v3.begin() + 2, v3.end());
  REQUIRE(v3[2] == 50);

  std::vector<u8> v4 = {20, 80, 10, 60, 90, 40};
  std::partial_sort(v4.begin(), v4.begin() + 3, v4.end(), std::greater<>{});
  REQUIRE(v4[0] == 90);
  REQUIRE(v4[1] == 80);
  REQUIRE(v4[2] == 60);

  // accumulate
  std::vector<u8> v5 = {1, 2, 3, 4, 5};
  auto acc = std::accumulate(v5.begin(), v5.end(), u16{0}, std::plus<>{});
  REQUIRE(acc == 15);

  // reverse
  std::vector<u8> v6 = {1, 2, 3};
  std::reverse(v6.begin(), v6.end());
  REQUIRE(v6[0] == 3);
  REQUIRE(v6[2] == 1);

  // rotate
  std::vector<u8> v7 = {1, 2, 3, 4, 5};
  std::rotate(v7.begin(), v7.begin() + 2, v7.end());
  REQUIRE(v7[0] == 3);
  REQUIRE(v7.back() == 2);

  // partition
  std::vector<u8> v8 = {10, 50, 20, 60, 30, 70};
  auto pivot = std::partition(v8.begin(), v8.end(), [](u8 x){ return x > 30; });
  for (auto p = v8.begin(); p != pivot; ++p) REQUIRE(*p > 30);
  for (auto p = pivot;      p != v8.end(); ++p) REQUIRE(*p <= 30);

  // equal / mismatch
  std::vector<u8> a = {10, 20, 30};
  std::vector<u8> b = {10, 20, 30};
  REQUIRE(std::equal(a.begin(), a.end(), b.begin()));
  b[1] = 99;
  auto mm = std::mismatch(a.begin(), a.end(), b.begin());
  REQUIRE(*mm.first  == 20);
  REQUIRE(*mm.second == 99);

  // iota
  std::vector<u8> iota_v(5);
  std::iota(iota_v.begin(), iota_v.end(), u8{10});
  REQUIRE(iota_v.front() == 10);
  REQUIRE(iota_v.back()  == 14);
}
