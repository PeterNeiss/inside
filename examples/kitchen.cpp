// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A recipe with units on insides: pounds, ounces, grams, litres and
// millilitres mix exactly, scaling a recipe is exact, oven temperatures are
// points (℉ ↔ ℃ is an exact shift of the grid), and a mass never adds to a
// volume.
//   1. The ingredients in mixed units, summed exactly.
//   2. The recipe scaled from 4 to 6 servings by 3/2, exactly.
//   3. The oven: 350 ℉ as a point, in ℃ exactly (530/3), then rounded onto
//      whole degrees; raised by 25 ℉ (a difference, not a point).
//   4. A volume of flour to a mass through its density: density·volume is a
//      mass by its recipe.
//   5. A flour jar that holds 2 kg: its clamp policy keeps the jar from
//      overflowing when a pound is added in pounds.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units_io.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
constexpr auto ml = si::milli(si::litre);
} // namespace

int main() {
    // 1. Flour: 1 lb plus 200 g; butter: 4 oz; milk: 0.5 L plus 150 mL. Each
    //    sum runs in the common unit of its two units, exactly.
    const auto flour  = just<1> * international::pound + just<200> * si::gram;
    const auto butter = just<4> * international::ounce;
    const auto milk   = just<1, 2> * si::litre + just<150> * ml;
    std::cout << "flour  " << flour.in(si::gram) << ", butter " << butter.in(si::gram) << ", milk " << milk << '\n';
    if (to_string(flour.in(si::gram)) != "653.59237 g" || to_string(butter.in(si::gram)) != "113.3980925 g" ||
        to_string(milk) != "650 mL")
        return 1;

    // 2. Six servings instead of four: everything times 3/2, still exact.
    constexpr auto scale = just<3, 2>;
    std::cout << "for 6:  flour " << (flour * scale).in(si::gram) << ", milk " << milk * scale << '\n';
    if (to_string((flour * scale).in(si::gram)) != "980.388555 g" || to_string(milk * scale) != "975 mL")
        return 1;

    // 3. 350 ℉ is a point; in ℃ it is (350 − 32)·5/9 = 530/3, exactly, on a
    //    grid shifted and scaled from the ℉ one. A whole-degree oven dial
    //    rounds it once. Raising the oven by 25 ℉ adds a difference.
    const auto oven    = point<international::degree_Fahrenheit>(inside<{0, 600}>{350});
    const auto oven_c  = oven.point_in(si::degree_Celsius);
    const auto dial    = value_cast<inside<{0, 300}, round_nearest>>(oven_c);
    const auto hotter  = oven + delta<international::degree_Fahrenheit>(just<25>);
    const auto dial_up = value_cast<inside<{0, 300}, round_nearest>>(hotter.point_in(si::degree_Celsius));
    std::cout << "oven   " << oven << " = " << oven_c << " ≈ " << dial << "; " << hotter << " ≈ " << dial_up << '\n';
    if (to_string(oven_c) != "530/3 ℃" || to_string(dial) != "177 ℃" || to_string(hotter) != "375 ℉" ||
        to_string(dial_up) != "191 ℃")
        return 1;

    // 4. 250 mL of flour at 0.53 g/mL: density·volume is a mass. A mass and a
    //    volume never add:
    //      auto bad = flour + milk;   // does not compile: another dimension
    const quantity<isq::mass_density[si::gram / ml], inside<{{0, 2}, per<100>}>> density{rational{53, 100}};
    const quantity<isq::volume[ml], inside<{0, 1000}>>                           cup{250};
    const quantity<isq::mass[si::gram], inside<{{0, 2000}, per<100>}>>           cup_of_flour = density * cup;
    std::cout << "cup    " << cup << " of flour = " << cup_of_flour << '\n';
    if (to_string(cup_of_flour) != "132.50 g")
        return 1;

    // 5. The jar holds 2 kg, to the gram; it starts with 1.5 kg. A pound fits
    //    (rounded to the gram); the second one is clamped at 2 kg.
    quantity<si::kilogram, inside<{{0, 2}, per<1000>}, clamp | round_nearest>> jar{rational{3, 2}};
    jar += just<1> * international::pound;
    const auto after_one = to_string(jar);
    jar += just<1> * international::pound;
    std::cout << "jar    " << after_one << ", then " << jar << '\n';
    if (after_one != "1.954 kg" || to_string(jar) != "2.000 kg")
        return 1;
    return 0;
}
