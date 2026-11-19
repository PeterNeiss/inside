// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A game character's health with fixed-point damage multipliers.
// Health is a base dimension of its own, in HP,
// so a multiplier stays a plain number and HP never adds to a count of anything else. (rifle.cpp: wrap.)

#include <iostream>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;

namespace
{
  inline constexpr auto HP = base_unit<"HP", "health">;
} // namespace

int main()
{
  using hit_points = quantity<HP, inside<{0, 100}, clamp>>; // saturates at 0 and 100
  hit_points hp{75, HP};
  std::cout << "starting health: " << hp << "\n";

  // One hit with three multipliers in [0, 4] at 1/16: mul_all folds them, the grid widening at each step,
  // and mul<dealt_damage> scales the base damage and rounds once to whole HP.
  using base_damage  = quantity<HP, inside<{0, 50  }>>;
  using multiplier   = inside<{{0, 4}, per<16>}, round_half_away>;
  using dealt_damage = quantity<HP, inside<{0, 3200}, round_half_away>>;

  base_damage base{10, HP};
  multiplier  crit{2.0};  // 2x on a crit
  multiplier  vuln{1.25}; // 1.25x on a vulnerable target
  multiplier  pen {1.5};  // 1.5x armor penetration

  auto       chain{mul_all(crit, vuln, pen)}; // [0, 64] at 1/4096
  const auto dealt{mul<dealt_damage>(base, chain)};
  std::cout << "damage chain (2.0 * 1.25 * 1.5) on base 10 HP = " << dealt << "\n";

  hp -= dealt; // 75 − 38 stays in range: nothing to clamp
  std::cout << "after hit: " << hp << "\n";

  // More damage than is left: on_clamp fires the "downed" hook.
  bool downed{false};
  hp.on_clamp(
    [&](auto& self, auto overshoot)
    {
      if (self == just<0> * HP)
      {
        std::cout << "[downed — overshoot " << overshoot << "]\n"; // a quantity: "-163 HP"
        downed = true;
      }
    }) -= just<200> * HP;
  std::cout << "after big hit: " << hp << "  (downed=" << std::boolalpha << downed << ")\n";

  // A heal saturates at full health.
  hp.on_clamp([&](auto&, auto) { std::cout << "[heal saturated to max HP]\n"; }) += just<250> * HP;
  std::cout << "after heal: " << hp << "\n";

  // A level-scaled bonus may pass 100 HP: hit_points' clamp policy clips the conversion, no flag needed.
  using scaled_pool = quantity<HP, inside<{0, 1000}>>;
  scaled_pool bonus{180, HP};
  hit_points  capped = bonus;
  std::cout << "\nlevel bonus 180 HP -> hit_points (implicit clamp): " << capped << "\n";

  if (to_string(dealt) != "38 HP" || !downed || to_string(hp) != "100 HP" || to_string(capped) != "100 HP")
    return 1;
  return 0;
}
