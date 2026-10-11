// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Quadrature oscillator: a wrapping phase accumulator + beman::inside::math::sin/cos.
//
// Demonstrates:
//   - A 16-bit phase counter as `inside<{0, 65535}, wrap>`: `+=` wraps at a
//     full turn, and `on_wrap` counts the completed cycles.
//   - Counter → radians as one exact multiply by the constant `rad_per_slot`,
//     with `math::sin_into` / `cos_into` rounding the result once onto Q.14.
//   - A frequency-swept variant where the per-sample increment changes
//     each step — exactly the pattern a chirp generator or vibrato LFO uses.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/cmath.hpp>

using namespace beman::inside;

int main() {
    // One full turn of phase in 65536 slots; wrap is the periodicity.
    using phase_t = inside<{0, 65535}, wrap>;
    // Q.14 output of sin and cos.
    using out_t = inside<{{-1, 1}, per<16384>}, round_nearest>;

    // 2π / 65536 — radians per slot, as a compile-time point. phase ×
    // rad_per_slot is the exact angle; sin_into rounds only the result.
    constexpr auto rad_per_slot = math::two_pi / just<65536>;

    std::cout << "Fixed-frequency oscillator (1/8 turn per sample, 8 samples = 1 cycle):\n";
    std::cout << "  i   sin              cos\n";
    {
        phase_t           phase{0};
        constexpr phase_t increment{65536 / 8}; // 1/8 turn per sample
        int               cycles = 0;
        for (int i = 0; i < 8; ++i) {
            const auto a = phase * rad_per_slot;
            std::cout << "  " << i << "   " << math::sin_into<out_t>(a) << "   " << math::cos_into<out_t>(a) << "\n";
            phase.on_wrap([&](auto&, auto) { ++cycles; }) += increment;
        }
        std::cout << "  completed cycles: " << cycles << "\n";
        if (cycles != 1 || phase != 0)
            return 1;
    }

    std::cout << "\nSwept-frequency oscillator (increment grows each step):\n";
    std::cout << "  i   inc      phase    sin\n";
    {
        using inc_t = inside<{1, 65535}, snap>; // snap: inc / 4 truncates
        phase_t phase{0};
        inc_t   inc{256}; // start at 1/256 turn
        for (int i = 0; i < 12; ++i) {
            std::cout << "  " << i << "   " << inc << "    " << phase << "    "
                      << math::sin_into<out_t>(phase * rad_per_slot) << "\n";
            phase += inc;       // wraps at a full turn
            inc += inc / 4_ins; // ×1.25 each step
        }
    }

    return 0;
}
