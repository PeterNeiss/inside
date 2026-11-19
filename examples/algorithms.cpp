// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// STL and ranges algorithms on inside types.

#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <ranges>
#include <vector>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>

namespace rng = std::ranges;
using namespace ins;

template <typename T>
void show(std::string_view label, const std::vector<T>& v)
{
  std::cout << label << ": ";
  for (const auto& x : v)
    std::cout << x << " ";
  std::cout << "\n";
}

int main()
{
  using channel = inside<{0, 255}, clamp>;
  std::cout << "--- fill / generate ---\n";
  {
    std::vector<channel> v(8);
    rng::fill(v, channel{128});
    show("fill", v);

    rng::generate(v, [n = 0]() mutable { return n += 32; });
    show("generate", v);
  }

  using celsius = inside<{{-40, 60}, 0.5}, round_half_away>;
  std::cout << "\n--- sort ---\n";
  std::vector<celsius> temps = {21.5, -5.0, 37.0, 0.0, 15.5, -20.0, 42.0, 8.5};
  {
    show("before", temps);
    rng::sort(temps);
    show("sorted", temps);

    rng::sort(temps, std::greater<>{});
    show("descending", temps);
  }

  std::cout << "\n--- min / max ---\n";
  {
    rng::sort(temps);
    auto [lo, hi] = rng::minmax_element(temps);
    std::cout << "min: " << *lo << "  max: " << *hi << "\n";
  }

  std::cout << "\n--- search ---\n";
  {
    std::vector<channel> colors = {10, 50, 128, 200, 50, 255, 50};
    auto it{rng::find(colors, channel{128})};
    if (it != colors.end())
      std::cout << "found 128 at index " << (it - colors.begin()) << "\n";

    auto n{rng::count(colors, channel{50})};
    std::cout << "count of 50: " << n << "\n";

    auto bright{rng::count_if(colors, [](channel c) { return c > 128; })};
    std::cout << "channels > 128: " << bright << "\n";

    bool found{rng::binary_search(temps, celsius{15.5})}; // temps is sorted
    std::cout << "binary_search 15.5: " << (found ? "yes" : "no") << "\n";

    auto lb{rng::lower_bound(temps, celsius{0.0})};
    std::cout << "lower_bound 0: " << *lb << "\n";
  }

  using percent = inside<{0, 100}, clamp>;
  std::cout << "\n--- predicates ---\n";
  {
    std::vector<percent> values = {25, 50, 75, 100};
    std::cout << "all > 0:    " << rng::all_of (values, [](percent p) { return p > 0; })    << "\n";
    std::cout << "any == 100: " << rng::any_of (values, [](percent p) { return p == 100; }) << "\n";
    std::cout << "none > 100: " << rng::none_of(values, [](percent p) { return p > 100; })  << "\n";
  }

  std::cout << "\n--- transform ---\n";
  {
    std::vector<channel> src = {200, 100, 50, 255, 0};
    std::vector<channel> dst(src.size());

    rng::transform(src, dst.begin(),
                   [](channel c) // darken by 50: clamp stops at 0
                   {
                     auto d{c};
                     d -= 50;
                     return d;
                   });
    show("original", src);
    show("darkened", dst);
  }

  using score = inside<{0, 1000}>;
  std::cout << "\n--- reduce ---\n";
  {
    // sum<Target>: an exact total with one range check,
    // an expected (the count is a run-time number).
    using score_total = inside<{0, 100'000}>;
    std::vector<score> scores = {100, 250, 500, 750, 1000};
    auto total{sum<score_total>(scores)};
    if (!total)
      return 1;
    std::cout << "total: " << *total << "\n";

    // A weighted sum: the products, then sum<Target> with its one check.
    // (std::transform_reduce would store every prefix back into an inside,
    // which the grids cannot prove.)
    using weighted_total = inside<{0, 1'000'000}>;
    std::vector<score> weights = {2, 3, 5};
    auto products{std::views::zip_transform([](score a, score b) { return a * b; }, scores, weights)};
    auto dot     {sum<weighted_total>(products)};
    if (!dot)
      return 1;
    std::cout << "weighted sum(first 3): " << *dot << "\n";
  }

  using altitude = inside<{-500, 9000}>;
  std::cout << "\n--- filter ---\n";
  {
    std::vector<altitude> data = {-200, 500, -50, 3000, 0, 8000};
    std::vector<altitude> positive;
    rng::copy_if(data, std::back_inserter(positive), [](altitude a) { return a > 0; });
    show("positive", positive);

    auto [first, last] = rng::remove_if(data, [](altitude a) { return a < 0; }); // below sea level
    data.erase(first, last);
    show("above sea level", data);
  }

  std::cout << "\n--- unique / adjacent_find ---\n";
  {
    std::vector<score> v = {100, 100, 200, 300, 300, 300, 500};
    auto adj{rng::adjacent_find(v)};
    if (adj != v.end())
      std::cout << "first adjacent dup: " << *adj << "\n";

    auto [uf, ul] = rng::unique(v);
    v.erase(uf, ul);
    show("unique", v);
  }

  std::cout << "\n--- classic STL ---\n";
  {
    std::vector<score> v = {500, 100, 800, 300, 700};
    std::sort(v.begin(), v.end());
    show("sort", v);

    std::stable_sort(v.begin(), v.end(), std::greater<>{});
    show("stable_sort desc", v);

    std::vector<altitude> alt = {3000, -200, 500, 8000, 100, -50, 1500};
    auto mid{alt.begin() + std::ssize(alt) / 2};
    std::nth_element(alt.begin(), mid, alt.end()); // the median
    std::cout << "median: " << *mid << "\n";

    std::vector<score> scores = {200, 800, 100, 600, 900, 400, 700};
    std::partial_sort(scores.begin(), scores.begin() + 3, scores.end(), std::greater<>{}); // the top 3
    std::cout << "top 3: " << scores[0] << " " << scores[1] << " " << scores[2] << "\n";

    // std::accumulate stores each prefix back into its accumulator, which the grids cannot prove:
    // the accumulator names what happens past its range
    // (clamp; sum<Target> checks the total once instead).
    using brightness_total = inside<{0, 100'000}, clamp>;
    std::vector<channel> rgb = {200, 150, 100};
    auto brightness{std::accumulate(rgb.begin(), rgb.end(), brightness_total{0}, std::plus<>{})};
    std::cout << "accumulate brightness: " << brightness << "\n";

    using dot_total = inside<{0, 1'000'000}, clamp>; // as accumulate's
    std::vector<score> a = {10, 20, 30};
    std::vector<score> b = {3 , 2 , 1 };
    auto dot{std::inner_product(a.begin(), a.end(), b.begin(), dot_total{0}, std::plus<>{},
                                [](score x, score y) { return x * y; })};
    std::cout << "inner_product: " << dot << "\n";

    using celsius_delta = decltype(celsius{} - celsius{}); // a difference spans twice the range
    std::vector<celsius>       readings = {-5.0, 0.0, 3.5, 8.0, 6.5};
    std::vector<celsius_delta> deltas(readings.size());
    std::adjacent_difference(readings.begin(), readings.end(), deltas.begin(),
                             [](celsius k, celsius l) { return k - l; });
    show("adj_diff", deltas);

    std::cout << "for_each: ";
    std::for_each(rgb.begin(), rgb.end(), [](channel c) { std::cout << c << " "; });
    std::cout << "\n";

    std::vector<percent> pcts = {10, 30, 50, 70, 90};
    std::reverse(pcts.begin(), pcts.end());
    show("reverse", pcts);

    std::vector<channel> seq = {1, 2, 3, 4, 5};
    std::rotate(seq.begin(), seq.begin() + 2, seq.end());
    show("rotate", seq);

    std::vector<altitude> mixed = {-100, 500, -30, 2000, 0, -400, 100};
    auto pivot{std::partition(mixed.begin(), mixed.end(), [](altitude x) { return x >= 0; })};
    std::cout << "partition (>=0): ";
    for (auto it = mixed.begin(); it != pivot; ++it)
      std::cout << *it << " ";
    std::cout << "| ";
    for (auto it = pivot; it != mixed.end(); ++it)
      std::cout << *it << " ";
    std::cout << "\n";

    std::vector<channel> x = {10, 20, 30};
    std::vector<channel> y = {10, 20, 30};
    std::cout << "equal: " << std::equal(x.begin(), x.end(), y.begin()) << "\n";

    y[2] = 99;
    auto mm{std::mismatch(x.begin(), x.end(), y.begin())};
    std::cout << "mismatch at: " << *mm.first << " vs " << *mm.second << "\n";

    std::vector<score> all = {100, 200, 300, 400, 500}; // includes needs sorted ranges
    std::vector<score> sub = {200, 400};
    std::cout << "includes: " << std::includes(all.begin(), all.end(), sub.begin(), sub.end()) << "\n";

    std::vector<channel> iota_v(10);
    std::iota(iota_v.begin(), iota_v.end(), channel{0});
    show("iota", iota_v);
  }

  using digit = inside<{0, 9}>;
  std::cout << "\n--- values<T> / inside_range / cursor ---\n";
  {
    std::cout << "values<digit>:   ";
    for (digit i : values<digit>)
      std::cout << i << " ";
    std::cout << "\n";

    std::cout << "wrap from 7:     ";
    for (digit i : inside_range<grid_of<digit>>{digit{7}})
      std::cout << i << " ";
    std::cout << "\n";

    // A cursor is an iterator and end(c) its sentinel: every third digit.
    std::vector<digit> thirds;
    cursor<digit, 3>   c;
    rng::copy(c, end(c), std::back_inserter(thirds));
    show("cursor<digit, 3>", thirds);
    std::cout << "count > 4:       " << rng::count_if(values<digit>, [](digit i) { return i > 4; }) << "\n";
  }

  return 0;
}
