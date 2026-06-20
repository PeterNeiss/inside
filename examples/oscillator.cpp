// Quadrature oscillator: integer phase accumulator + bnd::math::sin/cos.
//
// Demonstrates:
//   - A plain `uint16_t` phase counter — integer overflow on `+=` IS the
//     periodic wrap. The same property `turns_t<N>.Raw` used internally
//     before the trig API moved to a radians-first design; now exposed
//     here as the natural integer pattern instead of a special bound.
//   - Per-step conversion of the counter to a radians-valued bound via a
//     constexpr `rad_per_slot` constant — one rational multiply, then
//     `bnd::math::sin(angle_bound)` / `bnd::math::cos(angle_bound)`.
//   - A frequency-swept variant where the per-sample increment changes
//     each step — exactly the pattern a chirp generator or vibrato LFO uses.

#include <cstdint>
#include <iostream>

#include "bound/bound.hpp"
#include "bound/io.hpp"
#include "bound/cmath.hpp"

using namespace bnd;

int main()
{
  // Radians-valued angle bound covering one full cycle. ±8 rad is wider
  // than 2π so the conversion result always fits without saturation.
  using angle_t = bound<{{-8, 8}, notch<1, 16384>}, round_nearest | real>;

  // 2π / 65536 — the radians-per-slot for a uint16 phase counter, as a
  // compile-time point-bound. Multiplying the phase (lifted into a bound) by
  // it converts counter → radians without leaving bound-space.
  constexpr auto rad_per_slot = math::two_pi / just<65536>;
  using phase_b = bound<{0, 65535}>;

  std::cout << "Fixed-frequency oscillator (1/8 turn per sample, 8 samples = 1 cycle):\n";
  std::cout << "  i   sin              cos\n";
  {
    uint16_t phase = 0;
    constexpr uint16_t increment = 65536u / 8;               // 1/8 turn per sample
    for (int i = 0; i < 8; ++i) {
      angle_t a{phase_b{phase} * rad_per_slot};
      std::cout << "  " << i
                << "   " << math::sin(a)
                << "   " << math::cos(a) << "\n";
      phase += increment;                                    // wraps modulo 2^16
    }
  }

  std::cout << "\nSwept-frequency oscillator (increment grows each step):\n";
  std::cout << "  i   inc      phase    sin\n";
  {
    uint16_t phase = 0;
    uint16_t inc   = 256;                                    // start at ~1/256 turn
    for (int i = 0; i < 12; ++i) {
      angle_t a{phase_b{phase} * rad_per_slot};
      std::cout << "  " << i
                << "   " << +inc
                << "    " << +phase
                << "    " << math::sin(a) << "\n";
      phase += inc;                                          // free modular wrap
      inc   += inc / 4;                                      // ×1.25 each step
    }
  }

  return 0;
}
