// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A recipe in mixed units: pounds, ounces, grams, litres and millilitres add exactly, scaling is exact,
// oven temperatures are points (℉ ↔ ℃ is an exact shift of the grid),
// and a mass never adds to a volume.

#include <iostream>

#include <inside/inside.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;

namespace
{
  constexpr auto ml = si::milli(si::litre);
} // namespace

int main()
{
  // 1. Flour 1 lb + 200 g, butter 4 oz, milk 0.5 L + 150 mL: each sum runs in the common unit, exactly.
  const auto flour {international::pound + 200_q * si::gram};
  const auto butter{4_q * international::ounce};
  const auto milk  {0.5_q * si::litre + 150_q * ml};
  std::cout << "flour  " << flour.in(si::gram) << ", butter " << butter.in(si::gram) << ", milk " << milk << '\n';
  if (to_string(flour.in(si::gram)) != "653.59237 g" || to_string(butter.in(si::gram)) != "113.3980925 g"
      || to_string(milk) != "650 mL")
    return 1;

  // 2. Six servings instead of four: everything times 3/2, still exact.
  constexpr auto scale{1.5_q};
  std::cout << "for 6:  flour " << (flour * scale).in(si::gram) << ", milk " << milk * scale << '\n';
  if (to_string((flour * scale).in(si::gram)) != "980.388555 g" || to_string(milk * scale) != "975 mL")
    return 1;

  // 3. 350 ℉ is the point (350 − 32)·5/9 = 530/3 ℃ exactly, and a whole-degree dial rounds it once;
  // 25 ℉ more is a difference.
  // The dial's [0, 300] ℃ may not hold the oven, so value_cast is an expected.
  using oven_dial = id<"[0..300] round_half_away">;
  const auto oven   {point<international::degree_Fahrenheit>(id<"[0..600]">{350})};
  const auto oven_c {oven.point_in(si::degree_Celsius)};
  const auto dial   {value_cast<oven_dial>(oven_c)};
  const auto hotter {oven + delta<international::degree_Fahrenheit>(25_q)};
  const auto dial_up{value_cast<oven_dial>(hotter.point_in(si::degree_Celsius))};
  if (!dial || !dial_up)
    return 1;
  std::cout << "oven   " << oven << " = " << oven_c << " ≈ " << *dial << "; " << hotter << " ≈ " << *dial_up
            << '\n';
  if (to_string(oven_c) != "530/3 ℃" || to_string(*dial) != "177 ℃" || to_string(hotter) != "375 ℉"
      || to_string(*dial_up) != "191 ℃")
    return 1;

  // 4. 250 mL of flour at 0.53 g/mL: density·volume is a mass.
  // A mass and a volume never add:
  //   auto bad = flour + milk; // does not compile: another dimension
  using grams_per_ml = id<"[0..2] 1/100 [g/mL]{mass_density}">;
  using millilitres  = id<"[0..1000] [mL]{volume}">;
  using grams        = id<"[0..2000] 1/100 [g]{mass}">;

  const grams_per_ml density{0.53_q * isq::mass_density[si::gram / ml]};
  const millilitres  cup    {250, isq::volume[ml]};
  const grams        cup_of_flour = density * cup;
  std::cout << "cup    " << cup << " of flour = " << cup_of_flour << '\n';
  if (to_string(cup_of_flour) != "132.50 g")
    return 1;

  // 5. A flour jar that holds 2 kg, to the gram, starting at 1.5 kg:
  // a pound fits (rounded to the gram), and its clamp policy stops the second pound at 2 kg.
  using jar_kilograms = id<"[0..2] 1/1000 [kg] clamp|round_half_away">;
  jar_kilograms jar{1.5_q * si::kilogram};
  jar += international::pound;
  const auto after_one{to_string(jar)};
  jar += international::pound;
  std::cout << "jar    " << after_one << ", then " << jar << '\n';
  if (after_one != "1.954 kg" || to_string(jar) != "2.000 kg")
    return 1;
  return 0;
}
