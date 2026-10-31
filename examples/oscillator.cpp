// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Quadrature oscillator: a wrapping phase accumulator + beman::inside::math::sin/cos.
//
// Demonstrates:
//   - A 16-bit phase counter as `quantity<slot, inside<{0, 65535}, wrap>>`:
//     `+=` wraps at a full turn, and `on_wrap` counts the completed cycles.
//   - The counter is an angle in a unit of our own, the slot (2π/65536 rad).
//     A unit with a factor of π reduces exactly: `units::sin_into` /
//     `cos_into` take the slots as they are and round once onto Q.14 — no
//     rational approximation of 2π anywhere.
//   - A frequency-swept variant where the per-sample increment changes
//     each step — exactly the pattern a chirp generator or vibrato LFO uses.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/units/io.hpp>
#include <beman/inside/units/math.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
// One full turn is 65536 slots: a slot is 2π/65536 = π/32768 rad.
inline constexpr named_unit slot_def{"slot", mag<rational{1, 32768}> * mag_pi * si::radian};
inline constexpr auto       slot = unit_c<slot_def>;
} // namespace

int main() {
    // One full turn of phase in 65536 slots; wrap is the periodicity.
    using phase_t = quantity<slot, inside<{0, 65535}, wrap>>;
    // Q.14 output of sin and cos.
    using sample_t = inside<{{-1, 1}, per<16384>}, round_nearest>;

    std::cout << "Fixed-frequency oscillator (1/8 turn per sample, 8 samples = 1 cycle):\n";
    std::cout << "  i   sin              cos\n";
    {
        phase_t           phase{0};
        constexpr phase_t increment{65536 / 8}; // 1/8 turn per sample
        int               cycles = 0;
        for (int i = 0; i < 8; ++i) {
            std::cout << "  " << i << "   " << units::sin_into<sample_t>(phase) << "   "
                      << units::cos_into<sample_t>(phase) << "\n";
            phase.on_wrap([&](auto&, auto) { ++cycles; }) += increment;
        }
        std::cout << "  completed cycles: " << cycles << "\n";
        if (cycles != 1 || phase != just<0> * slot)
            return 1;
        // 1/8 turn is 45°: sin and cos both round to 11585/16384.
        if (units::sin_into<sample_t>(increment) != just<11585, 16384> ||
            units::cos_into<sample_t>(phase_t{65536 / 4}) != 0)
            return 1;
    }

    std::cout << "\nSwept-frequency oscillator (increment grows each step):\n";
    std::cout << "  i   inc        phase        sin\n";
    {
        using increment_t = quantity<slot, inside<{1, 65535}, snap>>; // snap: increment / 4 truncates
        phase_t     phase{0};
        increment_t increment{256}; // start at 1/256 turn
        for (int i = 0; i < 12; ++i) {
            std::cout << "  " << i << "   " << increment << "    " << phase << "    "
                      << units::sin_into<sample_t>(phase) << "\n";
            phase += increment;               // wraps at a full turn
            increment += increment / just<4>; // ×1.25 each step
        }
    }

    return 0;
}
