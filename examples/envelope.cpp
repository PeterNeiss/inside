// Envelope shaping with bnd::math::exp and bnd::math::exp2.
//
// Demonstrates:
//   - Exponential decay envelope:  a(t) = a0 · exp(-t/τ).
//   - Log-spaced frequency sweep:  f(step) = f_min · 2^(step / steps_per_octave).
//     This is the canonical "log-scale knob" used in synthesizer pitch controls
//     and graphic-EQ band layouts.
//   - Both run with integer/constexpr math; the only float anywhere is in the
//     stream operator's decimal formatting (via the bound's `to_string`).

#include <iostream>

#include "bound/bound.hpp"
#include "bound/cmath.hpp"
#include "bound/print.hpp"

using namespace bnd;

int main()
{
  // Decay envelope: amp(t) = amp0 · exp(-t/τ), with τ = 1.0, amp0 = 1.0.
  // We sample t at 0, 0.25, 0.5, …, 2.0 — eight points along the decay curve.
  // exp(-2.0) ≈ 0.1353, comfortably inside [0, 1].
  using time_t  = bound<{{-4, 0}, notch<1, 1024>}, round_nearest>;
  using amp_t   = bound<{{0, 1}, notch<1, 16384>}, round_nearest>;

  std::cout << "Exponential decay envelope (τ = 1):\n";
  std::cout << "    t       exp(-t)\n";
  for (int n = 0; n <= 8; ++n) {
    // -t/τ — negative time means decay. We sweep t = 0, 0.25, …, 2.0.
    time_t neg_t{rational{-n, 4}};
    amp_t  a{math::exp(neg_t)};
    std::cout << "    " << rational{n, 4} << "    " << a << "\n";
  }

  // Log frequency sweep: starting at 20 Hz, double every "octave" of steps.
  // 4 steps per octave over 4 octaves = 16 steps reaching 320 Hz (= 20 · 2^4).
  // freq(step) = 20 · 2^(step/4).
  std::cout << "\nLog frequency sweep (20 Hz, 4 steps/octave, 4 octaves):\n";
  std::cout << "    step    freq (Hz)\n";

  // Exponent for exp2: step/4 ∈ [0, 4]. exp2 input range fits comfortably.
  using exponent_t = bound<{{0, 4}, notch<1, 1024>}, round_nearest>;
  // Multiplier 2^(step/4) ∈ [1, 16].
  using mult_t     = bound<{{0, 16}, notch<1, 16384>}, round_nearest>;

  constexpr rational base_freq{20};
  for (int step = 0; step <= 16; ++step) {
    exponent_t e{rational{step, 4}};
    mult_t     m{math::exp2(e)};
    rational   freq = rational::mul_unchecked(base_freq, rational{m});
    std::cout << "    " << step
              << "       " << freq << "\n";
  }

  return 0;
}
