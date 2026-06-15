// Game character HP with fixed-point damage multipliers.
//
// Demonstrates:
//   - `mul_all` to chain damage multipliers (crit * vulnerable * armor_pen)
//   - Implicit clamping when assigning a wider bound into a clamp-policy
//     target (level-scaling that may exceed the HP range)
//   - `on_clamp` callback on HP to fire "full health" and "downed" hooks
//   - Fixed-point multipliers in [0, 4] with 1/16 resolution
//
// See examples/rifle.cpp for magazine / reload mechanics built on the
// `wrap` policy.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

// Health in [0, 100], clamp-saturating.
using hp_t  = bound<{0, 100}, clamp>;

// Damage multipliers in [0, 4] with 1/16 step (Q2.4-ish).
using mult_t = bound<{{0, 4}, notch<1, 16>}, round_nearest>;

// Base damage in [0, 50] integer.
using damage_t = bound<{0, 50}>;

// A widened pool that level-scaling can produce — values may exceed hp_t.
using big_pool_t = bound<{0, 1000}>;

int main()
{
  hp_t hp{75};
  std::cout << "starting HP: " << hp << "\n";

  bool downed = false;

  // Single damage event with three multipliers.
  damage_t base{10};
  mult_t crit{2.0};        // 2x on crit
  mult_t vuln{1.25};       // 1.25x vulnerable target
  mult_t pen{1.5};         // 1.5x armor penetration

  // mul_all folds three multipliers into one — grid widens each step.
  auto chain = mul_all(crit, vuln, pen);
  // chain has interval [0, 64] notch 1/4096 — convert to a hit damage by
  // scaling base, then snap to an integer HP deduction. `base * chain` returns
  // a plain `bound` (the static-overflow check on multiplication elides the
  // optional wrapper). Assigning it into a `round_nearest` integer grid rounds
  // it to the nearest whole point of damage — no rational, no cast.
  using dealt_t = bound<{0, 3200}, round_nearest>;
  dealt_t dealt{base * chain};   // integer damage, snapped to the round_nearest grid
  std::cout << "damage chain (2.0 * 1.25 * 1.5) on base 10 = " << dealt << "\n";

  hp.on_clamp([&](auto& self, auto overshoot) {
    if (self == 0) {
      std::cout << "[downed — overshoot " << overshoot << "]\n";
      downed = true;
    } else {
      std::cout << "[full HP, overshoot " << overshoot << "]\n";
    }
  }) -= dealt;
  std::cout << "HP after hit: " << hp << "\n";

  // Pile on more damage to trigger the downed callback.
  hp.on_clamp([&](auto& self, auto overshoot) {
    if (self == 0) {
      std::cout << "[downed — overshoot " << overshoot << "]\n";
      downed = true;
    }
  }) -= 200_b;
  std::cout << "HP after big hit: " << hp << "  (downed=" << std::boolalpha << downed << ")\n";

  // Heal via clamp-saturation back to full.
  hp.on_clamp([&](auto&, auto) { std::cout << "[heal saturated to max HP]\n"; })
    += 250_b;
  std::cout << "HP after heal: " << hp << "\n";

  // Level-scaled max-HP buff — could exceed hp_t. Because `hp_t` carries a
  // `clamp` policy, the implicit bound→bound conversion already clips at the
  // boundary: no `clamp_cast` needed when the target type already says
  // what to do with out-of-range values.
  big_pool_t bonus{180};
  hp_t capped = bonus;
  std::cout << "\nlevel bonus 180 -> hp_t (implicit clamp): " << capped << "\n";

  return 0;
}
