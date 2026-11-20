// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// STL and ranges algorithms on insides and quantities: every value with a meaning carries its unit,
// so a predicate compares with a quantity of its kind, never with a bare number.

#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>
#include <numeric>
#include <ranges>
#include <vector>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

namespace rng = std::ranges;
using namespace ins;

namespace
{
  inline constexpr auto DN  = base_unit<"DN", "pixel_value">; // a digital number: a raw pixel value
  inline constexpr auto pts = base_unit<"pts", "score">;
} // namespace

constexpr auto deg_C = si::degree_Celsius;
constexpr auto m     = si::metre;

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
  using channel = quantity<DN, inside<{0, 255}, clamp>>;
  std::cout << "--- fill / generate ---\n";
  {
    std::vector<channel> v(8);
    rng::fill(v, channel{128, DN});
    show("fill", v);

    rng::generate(v, [n = 0]() mutable { return channel{n += 32, DN}; }); // 256 clamps to 255
    show("generate", v);
  }

  // Readings are points on the ℃ scale: they sort and search, and two of them subtract to a difference.
  using celsius = quantity_point<deg_C, si::ice_point, inside<{{-40, 60}, 0.5}, round_half_away>>;
  std::cout << "\n--- sort ---\n";
  std::vector<celsius> temps = {{21.5, deg_C}, {-5.0, deg_C}, {37.0, deg_C}, {0.0, deg_C},
                                {15.5, deg_C}, {-20.0, deg_C}, {42.0, deg_C}, {8.5, deg_C}};
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
    std::vector<channel> colors = {{10, DN}, {50, DN}, {128, DN}, {200, DN}, {50, DN}, {255, DN}, {50, DN}};
    auto it{rng::find(colors, channel{128, DN})};
    if (it != colors.end())
      std::cout << "found 128 DN at index " << (it - colors.begin()) << "\n";

    auto n{rng::count(colors, channel{50, DN})};
    std::cout << "count of 50 DN: " << n << "\n";

    auto bright{rng::count_if(colors, [](channel c) { return c > just<128> * DN; })};
    std::cout << "channels > 128 DN: " << bright << "\n";

    bool found{rng::binary_search(temps, celsius{15.5, deg_C})}; // temps is sorted
    std::cout << "binary_search 15.5 ℃: " << (found ? "yes" : "no") << "\n";

    auto lb{rng::lower_bound(temps, celsius{0.0, deg_C})};
    std::cout << "lower_bound 0 ℃: " << *lb << "\n";
  }

  using percentage = quantity<percent, inside<{0, 100}, clamp>>;
  std::cout << "\n--- predicates ---\n";
  {
    std::vector<percentage> shares = {{25, percent}, {50, percent}, {75, percent}, {100, percent}};
    std::cout << "all > 0 %:    " << rng::all_of (shares, [](percentage p) { return p > just<0> * percent; })
              << "\n";
    std::cout << "any == 100 %: " << rng::any_of (shares, [](percentage p) { return p == just<100> * percent; })
              << "\n";
    std::cout << "none > 100 %: " << rng::none_of(shares, [](percentage p) { return p > just<100> * percent; })
              << "\n";
  }

  std::cout << "\n--- transform ---\n";
  {
    std::vector<channel> src = {{200, DN}, {100, DN}, {50, DN}, {255, DN}, {0, DN}};
    std::vector<channel> dst(src.size());

    rng::transform(src, dst.begin(),
                   [](channel c) // darken by 50 DN: clamp stops at 0
                   {
                     auto d{c};
                     d -= just<50> * DN;
                     return d;
                   });
    show("original", src);
    show("darkened", dst);
  }

  using score  = quantity<pts, inside<{0, 1000}>>;
  using weight = inside<{0, 10}>; // a factor: a plain number
  std::cout << "\n--- reduce ---\n";
  {
    // sum<Target>: an exact total with one range check,
    // an expected (the count is a run-time number).
    using score_total = quantity<pts, inside<{0, 100'000}>>;
    std::vector<score> scores = {{100, pts}, {250, pts}, {500, pts}, {750, pts}, {1000, pts}};
    auto total{sum<score_total>(scores)};
    if (!total)
      return 1;
    std::cout << "total: " << *total << "\n";

    // A weighted sum: the products, then sum<Target> with its one check.
    // (std::transform_reduce would store every prefix back into a quantity,
    // which the grids cannot prove.)
    using weighted_total = quantity<pts, inside<{0, 1'000'000}>>;
    std::vector<weight> weights = {2, 3, 5};
    auto products{std::views::zip_transform([](score s, weight w) { return s * w; }, scores, weights)};
    auto dot     {sum<weighted_total>(products)};
    if (!dot)
      return 1;
    std::cout << "weighted sum(first 3): " << *dot << "\n";
  }

  using altitude = quantity<si::metre, inside<{-500, 9000}>>;
  std::cout << "\n--- filter ---\n";
  {
    std::vector<altitude> data = {{-200, m}, {500, m}, {-50, m}, {3000, m}, {0, m}, {8000, m}};
    std::vector<altitude> positive;
    rng::copy_if(data, std::back_inserter(positive), [](altitude a) { return a > just<0> * m; });
    show("positive", positive);

    auto [first, last] = rng::remove_if(data, [](altitude a) { return a < just<0> * m; }); // below sea level
    data.erase(first, last);
    show("above sea level", data);
  }

  std::cout << "\n--- unique / adjacent_find ---\n";
  {
    std::vector<score> v = {{100, pts}, {100, pts}, {200, pts}, {300, pts}, {300, pts}, {300, pts}, {500, pts}};
    auto adj{rng::adjacent_find(v)};
    if (adj != v.end())
      std::cout << "first adjacent dup: " << *adj << "\n";

    auto [uf, ul] = rng::unique(v);
    v.erase(uf, ul);
    show("unique", v);
  }

  using digit = inside<{0, 9}>;
  std::cout << "\n--- classic STL ---\n";
  {
    std::vector<score> v = {{500, pts}, {100, pts}, {800, pts}, {300, pts}, {700, pts}};
    std::sort(v.begin(), v.end());
    show("sort", v);

    std::stable_sort(v.begin(), v.end(), std::greater<>{});
    show("stable_sort desc", v);

    std::vector<altitude> alt = {{3000, m}, {-200, m}, {500, m}, {8000, m}, {100, m}, {-50, m}, {1500, m}};
    auto mid{alt.begin() + std::ssize(alt) / 2};
    std::nth_element(alt.begin(), mid, alt.end()); // the median
    std::cout << "median: " << *mid << "\n";

    std::vector<score> scores = {{200, pts}, {800, pts}, {100, pts}, {600, pts}, {900, pts}, {400, pts}, {700, pts}};
    std::partial_sort(scores.begin(), scores.begin() + 3, scores.end(), std::greater<>{}); // the top 3
    std::cout << "top 3: " << scores[0] << " " << scores[1] << " " << scores[2] << "\n";

    // std::accumulate stores each prefix back into its accumulator, which the grids cannot prove:
    // the accumulator names what happens past its range
    // (clamp; sum<Target> checks the total once instead).
    using brightness_total = quantity<DN, inside<{0, 100'000}, clamp>>;
    std::vector<channel> rgb = {{200, DN}, {150, DN}, {100, DN}};
    auto brightness{std::accumulate(rgb.begin(), rgb.end(), brightness_total{0, DN}, std::plus<>{})};
    std::cout << "accumulate brightness: " << brightness << "\n";

    using dot_total = quantity<pts, inside<{0, 1'000'000}, clamp>>; // as accumulate's
    std::vector<score>  a = {{10, pts}, {20, pts}, {30, pts}};
    std::vector<weight> b = {3, 2, 1};
    auto dot{std::inner_product(a.begin(), a.end(), b.begin(), dot_total{0, pts}, std::plus<>{},
                                [](score s, weight w) { return s * w; })};
    std::cout << "inner_product: " << dot << "\n";

    // Neighbouring readings subtract to differences, ℃ quantities spanning twice the range.
    // (std::adjacent_difference would copy the first reading into them as if it were a difference.)
    std::vector<celsius> readings = {{-5.0, deg_C}, {0.0, deg_C}, {3.5, deg_C}, {8.0, deg_C}, {6.5, deg_C}};
    using celsius_delta = decltype(celsius{} - celsius{});
    std::vector<celsius_delta> deltas(readings.size() - 1);
    std::transform(readings.begin() + 1, readings.end(), readings.begin(), deltas.begin(), std::minus<>{});
    show("differences", deltas);

    std::cout << "for_each: ";
    std::for_each(rgb.begin(), rgb.end(), [](channel c) { std::cout << c << " "; });
    std::cout << "\n";

    std::vector<percentage> pcts = {{10, percent}, {30, percent}, {50, percent}, {70, percent}, {90, percent}};
    std::reverse(pcts.begin(), pcts.end());
    show("reverse", pcts);

    std::vector<digit> seq = {1, 2, 3, 4, 5};
    std::rotate(seq.begin(), seq.begin() + 2, seq.end());
    show("rotate", seq);

    std::vector<altitude> mixed = {{-100, m}, {500, m}, {-30, m}, {2000, m}, {0, m}, {-400, m}, {100, m}};
    auto pivot{std::partition(mixed.begin(), mixed.end(), [](altitude x) { return x >= just<0> * m; })};
    std::cout << "partition (>= 0 m): ";
    for (auto it = mixed.begin(); it != pivot; ++it)
      std::cout << *it << " ";
    std::cout << "| ";
    for (auto it = pivot; it != mixed.end(); ++it)
      std::cout << *it << " ";
    std::cout << "\n";

    std::vector<channel> x = {{10, DN}, {20, DN}, {30, DN}};
    std::vector<channel> y = x;
    std::cout << "equal: " << std::equal(x.begin(), x.end(), y.begin()) << "\n";

    y[2] = channel{99, DN};
    auto mm{std::mismatch(x.begin(), x.end(), y.begin())};
    std::cout << "mismatch at: " << *mm.first << " vs " << *mm.second << "\n";

    // includes needs sorted ranges.
    std::vector<score> all = {{100, pts}, {200, pts}, {300, pts}, {400, pts}, {500, pts}};
    std::vector<score> sub = {{200, pts}, {400, pts}};
    std::cout << "includes: " << std::includes(all.begin(), all.end(), sub.begin(), sub.end()) << "\n";

    std::vector<channel> iota_v(10);
    std::iota(iota_v.begin(), iota_v.end(), channel{0, DN});
    show("iota", iota_v);
  }

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
