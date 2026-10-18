// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Grids as ranges: the values of a type are a sequence you can pipe and search.
//   1. `values<T>` — every value of T, a random-access sized view: views
//      compose on it, and `[i]`, `size()`, `back()` cost nothing.
//   2. Search without a table — `partition_point` bisects the grid itself:
//      O(log n) evaluations, nothing materialized.
//   3. `inside_range<G>{start}` — the same values from a mid-grid start,
//      wrapping once past the top: a ring buffer read in age order.
//   4. `cursor<T, Step>` — an iterator with `end(t)` as its sentinel; a
//      `subrange` of the two is a view, so it pipes too.

#include <algorithm>
#include <array>
#include <iostream>
#include <ranges>
#include <vector>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/units.hpp>

namespace rng   = std::ranges;
namespace views = std::views;
using namespace beman::inside;
using namespace beman::inside::units;

template <rng::input_range R>
void show(std::string_view label, R&& r) {
    std::cout << label;
    for (const auto& x : r)
        std::cout << ' ' << x;
    std::cout << '\n';
}

int main() {
    // 1. values<T> in a pipeline.
    using digit = inside<{0, 9}>;
    static_assert(rng::random_access_range<decltype(values<digit>)>);
    static_assert(rng::sized_range<decltype(values<digit>)>);

    show("digits:      ", values<digit>);
    show("odd squares: ", values<digit> | views::filter([](digit d) {
                              return d % 2_ins == 1;
                          }) | views::transform([](digit d) { return d * d; })); // inside<{0, 81}>: cannot overflow
    show("last three:  ", values<digit> | views::reverse | views::take(3));
    std::cout << "size:         " << values<digit>.size() << ", [7] = " << values<digit>[7]
              << ", back() = " << values<digit>.back() << '\n';

    // A fractional notch walks the same way: eighths of a unit, exactly.
    using eighth = inside<{{0, 1}, per<8>}>;
    show("eighths:     ", values<eighth> | views::drop(5));

    // Index another container: every index is in range by construction.
    constexpr std::array<char, values<digit>.size()> names{'z', 'o', 't', 't', 'f', 'f', 's', 's', 'e', 'n'};
    show("initials:    ", values<digit> | views::transform([&](digit d) { return names[d]; }));

    // 2. Bisect a grid: the first tenth of a degree Celsius past 212 ℉, in 11
    //    exact comparisons out of 2001 values. Each value is a ℃ point, and
    //    its ℉ point is an exact shift and scale of the grid (units.hpp).
    using celsius   = inside<{{-50, 150}, per<10>}>;
    auto fahrenheit = [](celsius c) {
        return point<si::degree_Celsius>(c).point_in(international::degree_Fahrenheit);
    };
    const auto boiling = point<international::degree_Fahrenheit>(212_ins);
    const auto boil    = rng::partition_point(values<celsius>, [&](celsius c) { return fahrenheit(c) <= boiling; });
    std::cout << "above 212 ℉: " << point<si::degree_Celsius>(*boil) << " = " << fahrenheit(*boil) << "\n";
    if (*boil != 100.1_ins || to_string(fahrenheit(*boil)) != "212.18 ℉")
        return 1;

    // The first thousandth whose square exceeds 2: sqrt(2), rounded up.
    using x_t        = inside<{{0, 2}, per<1000>}>;
    const auto root2 = rng::partition_point(values<x_t>, [](x_t x) { return x * x <= 2_ins; });
    std::cout << "sqrt(2) <=    " << *root2 << '\n';
    if (*root2 != 1.415_ins)
        return 1;

    // 3. A ring buffer read oldest first: the write head is the next slot to
    //    overwrite, so the oldest entry sits right there and the walk wraps.
    using slot = inside<{0, 7}>;
    const std::array<char, values<slot>.size()> ring{'i', 'j', 'c', 'd', 'e', 'f', 'g', 'h'};
    const slot                                  head{2};
    show("oldest first:", inside_range<grid_of<slot>>{head} | views::transform([&](slot s) { return ring[s]; }));

    // 4. A cursor and its end as a view: the quarter hours of a day, 0 through
    //    1440 with both ends, and the ones in office hours.
    using minute = inside<{0, 24 * 60}>;
    cursor<minute, 15> c;
    rng::subrange      quarters{c, end(c)};
    auto               office = quarters | views::filter([](minute m) { return m >= 9 * 60 && m < 17 * 60; });
    std::cout << "quarters:     " << rng::distance(quarters) << ", in office hours: " << rng::distance(office) << " ("
              << rng::min(office) << " … " << rng::max(office) << ")\n";
    if (rng::distance(quarters) != 97 || rng::distance(office) != 32)
        return 1;

    return 0;
}
