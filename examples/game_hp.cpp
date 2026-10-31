// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Game character HP with fixed-point damage multipliers. Health is a base
// dimension of its own with the unit HP, so damage and healing are HP and a
// multiplier stays a plain number: HP never adds to a count of anything else.
//
// Demonstrates:
//   - `mul_all` to chain damage multipliers (crit * vulnerable * armor_pen)
//   - `mul_into` to round the scaled damage once onto whole HP
//   - Implicit clamping when assigning a wider inside into a clamp-policy
//     target (level-scaling that may exceed the HP range)
//   - `on_clamp` callback on HP to fire "full health" and "downed" hooks
//   - Fixed-point multipliers in [0, 4] with 1/16 resolution
//
// See examples/rifle.cpp for magazine / reload mechanics built on the
// `wrap` policy.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units/io.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
inline constexpr base_dimension health_dim{"health"};
inline constexpr quantity_spec  health_def{"health", health_dim};
inline constexpr named_unit     hp_def{"HP", spec_c<health_def>};
inline constexpr auto           HP = unit_c<hp_def>;
} // namespace

// Health in [0, 100] HP, clamp-saturating.
using hp_t = quantity<HP, inside<{0, 100}, clamp>>;

// Damage multipliers in [0, 4] with 1/16 step (Q2.4-ish).
using mult_t = inside<{{0, 4}, per<16>}, round_nearest>;

// Base damage in [0, 50] HP.
using damage_t = quantity<HP, inside<{0, 50}>>;

// A widened pool that level-scaling can produce — values may exceed hp_t.
using big_pool_t = quantity<HP, inside<{0, 1000}>>;

int main() {
    hp_t hp{75};
    std::cout << "starting health: " << hp << "\n";

    bool downed = false;

    // Single damage event with three multipliers.
    damage_t base{10};
    mult_t   crit{2.0};  // 2x on crit
    mult_t   vuln{1.25}; // 1.25x vulnerable target
    mult_t   pen{1.5};   // 1.5x armor penetration

    // mul_all folds three multipliers into one — grid widens each step.
    auto chain = mul_all(crit, vuln, pen);
    // chain has interval [0, 64] notch 1/4096. mul_into scales base by it and
    // rounds once to the nearest whole HP of damage — no rational, no cast.
    using dealt_t    = quantity<HP, inside<{0, 3200}, round_nearest>>;
    const auto dealt = mul_into<dealt_t>(base, chain);
    std::cout << "damage chain (2.0 * 1.25 * 1.5) on base 10 HP = " << dealt << "\n";

    hp -= dealt; // 75 − 38 stays in range: nothing to clamp
    std::cout << "after hit: " << hp << "\n";

    // Pile on more damage to trigger the downed callback.
    hp.on_clamp([&](auto& self, auto overshoot) {
        if (self == just<0> * HP) {
            std::cout << "[downed — overshoot " << overshoot << " HP]\n";
            downed = true;
        }
    }) -= just<200> * HP;
    std::cout << "after big hit: " << hp << "  (downed=" << std::boolalpha << downed << ")\n";

    // Heal via clamp-saturation back to full.
    hp.on_clamp([&](auto&, auto) { std::cout << "[heal saturated to max HP]\n"; }) += just<250> * HP;
    std::cout << "after heal: " << hp << "\n";

    // Level-scaled max-HP buff — could exceed hp_t. Because `hp_t` carries a
    // `clamp` policy, the implicit quantity→quantity conversion already clips at the
    // boundary: no `clamp_cast` needed when the target type already says
    // what to do with out-of-range values.
    big_pool_t bonus{180};
    hp_t       capped = bonus;
    std::cout << "\nlevel bonus 180 HP -> hp_t (implicit clamp): " << capped << "\n";

    if (to_string(dealt) != "38 HP" || !downed || to_string(hp) != "100 HP" || to_string(capped) != "100 HP")
        return 1;
    return 0;
}
