// STL and ranges algorithm examples with bound types.

#include <algorithm>
#include <iostream>
#include <numeric>
#include <vector>

#include "bound/bound.hpp"
#include "bound/print.hpp"

namespace rng = std::ranges;
using namespace bnd;

using celsius   = bound<{{-40, 60}, 0.5}, round_nearest>;
using channel   = bound<{0, 255}, clamp>;
using pct       = bound<{0, 100}, clamp>;
using score     = bound<{0, 1000}>;
using altitude  = bound<{-500, 9000}>;

template <typename T>
void print(std::string_view label, std::vector<T> const& v)
{
  std::cout << label << ": ";
  for (auto const& x : v) std::cout << x << " ";
  std::cout << "\n";
}

int main()
{
  // --- fill / generate ---
  std::cout << "--- fill / generate ---\n";
  {
    std::vector<channel> v(8);
    rng::fill(v, channel{128});
    print("fill", v);

    rng::generate(v, [n = 0]() mutable { return n += 32; });
    print("generate", v);
  }

  // --- sort ---
  std::cout << "\n--- sort ---\n";
  std::vector<celsius> temps = {21.5, -5.0, 37.0, 0.0, 15.5, -20.0, 42.0, 8.5};
  {
    print("before", temps);
    rng::sort(temps);
    print("sorted", temps);

    rng::sort(temps, std::greater<>{});
    print("descending", temps);
  }

  // --- min / max ---
  std::cout << "\n--- min / max ---\n";
  {
    rng::sort(temps);
    auto [lo, hi] = rng::minmax_element(temps);
    std::cout << "min: " << *lo << "  max: " << *hi << "\n";
  }

  // --- search ---
  std::cout << "\n--- search ---\n";
  {
    std::vector<channel> colors = {10, 50, 128, 200, 50, 255, 50};
    auto it = rng::find(colors, channel{128});
    if (it != colors.end())
      std::cout << "found 128 at index " << (it - colors.begin()) << "\n";

    auto n = rng::count(colors, channel{50});
    std::cout << "count of 50: " << n << "\n";

    auto bright = rng::count_if(colors, [](channel c) { return c > 128; });
    std::cout << "channels > 128: " << bright << "\n";

    // binary_search on sorted temps
    bool found = rng::binary_search(temps, celsius{15.5});
    std::cout << "binary_search 15.5: " << (found ? "yes" : "no") << "\n";

    auto lb = rng::lower_bound(temps, celsius{0.0});
    std::cout << "lower_bound 0: " << *lb << "\n";
  }

  // --- predicates ---
  std::cout << "\n--- predicates ---\n";
  {
    std::vector<pct> values = {25, 50, 75, 100};
    std::cout << "all > 0:    " << rng::all_of(values, [](pct p) { return p > 0; }) << "\n";
    std::cout << "any == 100: " << rng::any_of(values, [](pct p) { return p == 100; }) << "\n";
    std::cout << "none > 100: " << rng::none_of(values, [](pct p) { return p > 100; }) << "\n";
  }

  // --- transform ---
  std::cout << "\n--- transform ---\n";
  {
    std::vector<channel> src = {200, 100, 50, 255, 0};
    std::vector<channel> dst(src.size());

    // darken: subtract 50 from each channel (clamp prevents underflow)
    rng::transform(src, dst.begin(), [](channel c) {
      auto d = c;
      d -= 50;
      return d;
    });
    print("original", src);
    print("darkened", dst);
  }

  // --- reduce ---
  std::cout << "\n--- reduce ---\n";
  {
    using wide_score = bound<{0, 100'000}>;
    std::vector<score> scores = {100, 250, 500, 750, 1000};
    auto total = std::reduce(scores.begin(), scores.end(), wide_score{0}, std::plus<>{});
    std::cout << "total: " << total << "\n";

    // transform_reduce: weighted sum
    std::vector<score> weights = {2, 3, 5};
    using wide = bound<{0, 1'000'000}>;
    auto dot = std::transform_reduce(
      scores.begin(), scores.begin() + 3, weights.begin(), wide{0},
      std::plus<>{}, [](score a, score b) { return a * b; });
    std::cout << "weighted sum(first 3): " << dot << "\n";
  }

  // --- filter ---
  std::cout << "\n--- filter ---\n";
  {
    std::vector<altitude> data = {-200, 500, -50, 3000, 0, 8000};
    std::vector<altitude> positive;
    rng::copy_if(data, std::back_inserter(positive),
                 [](altitude a) { return a > 0; });
    print("positive", positive);

    // erase-remove: remove values below sea level
    auto [first, last] = rng::remove_if(data, [](altitude a) { return a < 0; });
    data.erase(first, last);
    print("above sea level", data);
  }

  // --- unique / adjacent_find ---
  std::cout << "\n--- unique / adjacent_find ---\n";
  {
    std::vector<score> v = {100, 100, 200, 300, 300, 300, 500};
    auto adj = rng::adjacent_find(v);
    if (adj != v.end())
      std::cout << "first adjacent dup: " << *adj << "\n";

    auto [uf, ul] = rng::unique(v);
    v.erase(uf, ul);
    print("unique", v);
  }

  // --- classic STL (iterator-based) ---
  std::cout << "\n--- classic STL ---\n";
  {
    // std::sort with custom comparator
    std::vector<score> v = {500, 100, 800, 300, 700};
    std::sort(v.begin(), v.end());
    print("sort", v);

    // std::stable_sort descending
    std::stable_sort(v.begin(), v.end(), std::greater<>{});
    print("stable_sort desc", v);

    // std::nth_element — find median
    std::vector<altitude> alt = {3000, -200, 500, 8000, 100, -50, 1500};
    auto mid = alt.begin() + static_cast<long>(alt.size()) / 2;
    std::nth_element(alt.begin(), mid, alt.end());
    std::cout << "median: " << *mid << "\n";

    // std::partial_sort — top 3
    std::vector<score> scores = {200, 800, 100, 600, 900, 400, 700};
    std::partial_sort(scores.begin(), scores.begin() + 3, scores.end(), std::greater<>{});
    std::cout << "top 3: " << scores[0] << " " << scores[1] << " " << scores[2] << "\n";

    // std::accumulate
    std::vector<channel> rgb = {200, 150, 100};
    using wide_ch = bound<{0, 100'000}>;
    auto brightness = std::accumulate(rgb.begin(), rgb.end(), wide_ch{0}, std::plus<>{});
    std::cout << "accumulate brightness: " << brightness << "\n";

    // std::inner_product
    std::vector<score> a = {10, 20, 30};
    std::vector<score> b = {3, 2, 1};
    using wide_score = bound<{0, 1'000'000}>;
    auto dot = std::inner_product(a.begin(), a.end(), b.begin(), wide_score{0},
                                  std::plus<>{}, [](score x, score y) { return x * y; });
    std::cout << "inner_product: " << dot << "\n";

    // std::adjacent_difference
    std::vector<celsius> readings = {-5.0, 0.0, 3.5, 8.0, 6.5};
    std::vector<celsius> deltas(readings.size());
    std::adjacent_difference(readings.begin(), readings.end(), deltas.begin(),
      [](celsius a, celsius b) { return celsius{static_cast<double>(a) - static_cast<double>(b)}; });
    print("adj_diff", deltas);

    // std::for_each with side effect
    std::cout << "for_each: ";
    std::for_each(rgb.begin(), rgb.end(), [](channel c) { std::cout << c << " "; });
    std::cout << "\n";

    // std::reverse
    std::vector<pct> pcts = {10, 30, 50, 70, 90};
    std::reverse(pcts.begin(), pcts.end());
    print("reverse", pcts);

    // std::rotate
    std::vector<channel> seq = {1, 2, 3, 4, 5};
    std::rotate(seq.begin(), seq.begin() + 2, seq.end());
    print("rotate", seq);

    // std::partition
    std::vector<altitude> mixed = {-100, 500, -30, 2000, 0, -400, 100};
    auto pivot = std::partition(mixed.begin(), mixed.end(),
                                [](altitude a) { return a >= 0; });
    std::cout << "partition (>=0): ";
    for (auto it = mixed.begin(); it != pivot; ++it)
      std::cout << *it << " ";
    std::cout << "| ";
    for (auto it = pivot; it != mixed.end(); ++it)
      std::cout << *it << " ";
    std::cout << "\n";

    // std::equal
    std::vector<channel> x = {10, 20, 30};
    std::vector<channel> y = {10, 20, 30};
    std::cout << "equal: " << std::equal(x.begin(), x.end(), y.begin()) << "\n";

    // std::mismatch
    y[2] = 99;
    auto mm = std::mismatch(x.begin(), x.end(), y.begin());
    std::cout << "mismatch at: " << *mm.first << " vs " << *mm.second << "\n";

    // std::includes on sorted ranges
    std::vector<score> all = {100, 200, 300, 400, 500};
    std::vector<score> sub = {200, 400};
    std::cout << "includes: " << std::includes(all.begin(), all.end(),
                                                sub.begin(), sub.end()) << "\n";

    // std::iota (from <numeric>)
    std::vector<channel> iota_v(10);
    std::iota(iota_v.begin(), iota_v.end(), channel{0});
    print("iota", iota_v);
  }

  // --- bound_range ---
  std::cout << "\n--- bound_range ---\n";
  {
    std::cout << "for_each [0..9]: ";
    for (auto i : bound_range<{0, 9}>{})
      std::cout << i << " ";
    std::cout << "\n";

    // wrap-around range starting at 7
    std::cout << "wrap from 7:    ";
    using idx = bound<{0, 9}>;
    for (auto i : bound_range<{0, 9}>{idx{7}})
      std::cout << i << " ";
    std::cout << "\n";
  }

  return 0;
}
