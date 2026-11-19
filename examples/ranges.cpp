// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Grids as ranges: the values of a type are a sequence you can pipe, index and search without building a table,
// from a mid-grid start that wraps, or by a cursor and its end.

#include <algorithm>
#include <array>
#include <iostream>
#include <ranges>
#include <vector>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

namespace rng = std::ranges;
namespace views = std::views;
using namespace ins;

template <rng::input_range R>
void show(std::string_view label, R&& r) {
    std::cout << label;
    for (const auto& x : r)
        std::cout << ' ' << x;
    std::cout << '\n';
}

int main() {
    // 1. values<T>: every value of T as a random-access sized view, so views compose on it and [i], size(), back()
    //    cost nothing.
    using digit = inside<{0, 9}>; // an index: a container takes it as it is
    static_assert(rng::random_access_range<decltype(values<digit>)>);
    static_assert(rng::sized_range<decltype(values<digit>)>);

    show("digits:      ", values<digit>);
    show("odd squares: ", values<digit>
                              | views::filter([](digit d) { return d % 2_ins == 1; })
                              | views::transform([](digit d) { return d * d; })); // inside<{0, 81}>: cannot overflow
    show("last three:  ", values<digit> | views::reverse | views::take(3));
    std::cout << "size:         " << values<digit>.size() << ", [7] = " << values<digit>[7]
              << ", back() = " << values<digit>.back() << '\n';

    using eighth = inside<{{0, 1}, per<8>}>; // a fractional notch walks the same way, exactly
    show("eighths:     ", values<eighth> | views::drop(5));

    // Every digit indexes another container in range, by construction.
    constexpr std::array<char, values<digit>.size()> names{'z', 'o', 't', 't', 'f', 'f', 's', 's', 'e', 'n'};
    show("initials:    ", values<digit> | views::transform([&](digit d) { return names[d]; }));

    // 2. partition_point bisects the grid itself: the first tenth of a ℃ past 212 ℉ in 11 exact comparisons out of
    //    2001 values (a ℃ point's ℉ point is an exact shift and scale of the grid).
    using celsius = inside<{{-50, 150}, per<10>}>;
    auto fahrenheit = [](celsius c) {
        return point<si::degree_Celsius>(c).point_in(international::degree_Fahrenheit);
    };
    const auto boiling = point<international::degree_Fahrenheit>(212_ins);
    const auto boil = rng::partition_point(values<celsius>, [&](celsius c) { return fahrenheit(c) <= boiling; });
    std::cout << "above 212 ℉: " << point<si::degree_Celsius>(*boil) << " = " << fahrenheit(*boil) << "\n";
    if (*boil != 100.1_ins || to_string(fahrenheit(*boil)) != "212.18 ℉")
        return 1;

    // The first thousandth whose square exceeds 2: sqrt(2), rounded up.
    using thousandths = inside<{{0, 2}, per<1000>}>;
    const auto root2 = rng::partition_point(values<thousandths>, [](thousandths x) { return x * x <= 2; });
    std::cout << "sqrt(2) <=    " << *root2 << '\n';
    if (*root2 != 1.415_ins)
        return 1;

    // 3. A ring buffer read oldest first: inside_range<G>{head} starts at the next slot to overwrite and wraps once.
    using slot = inside<{0, 7}>;
    const std::array<char, values<slot>.size()> ring{'i', 'j', 'c', 'd', 'e', 'f', 'g', 'h'};
    const slot head{2};
    show("oldest first:", inside_range<grid_of<slot>>{head} | views::transform([&](slot s) { return ring[s]; }));

    // 4. A cursor and its end as a subrange, which pipes too: the quarter hours of a day (0 … 1440, both ends) and
    //    the ones in office hours.
    using minute_of_day = inside<{0, 24 * 60}>;
    cursor<minute_of_day, 15> c;
    rng::subrange quarters{c, end(c)};
    auto office = quarters | views::filter([](minute_of_day m) { return m >= 9 * 60 && m < 17 * 60; });
    std::cout << "quarters:     " << rng::distance(quarters) << ", in office hours: " << rng::distance(office)
              << " (" << rng::min(office) << " … " << rng::max(office) << ")\n";
    if (rng::distance(quarters) != 97 || rng::distance(office) != 32)
        return 1;
    return 0;
}
