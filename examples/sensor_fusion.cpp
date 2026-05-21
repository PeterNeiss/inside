// Weighted average across three temperature sensors with disparate ranges.
//
// Demonstrates:
//   - Per-sensor fixed-point grids (different lower/upper/notch each)
//   - `will_conversion_overflow` predicate as a soft outlier filter
//   - Rational-weighted average to keep the fused value exact
//   - `clamp_round` to snap the fused value onto the output grid

#include <iostream>

#include "bound/bound.hpp"
#include "bound/predicates.hpp"
#include "bound/print.hpp"

using namespace bnd;

// Three sensors covering overlapping but distinct ranges.
// Each has a different precision matched to its hardware.
using outdoor_t = bound<{{-40, 60},  notch<1, 2>},   round_nearest>;  // 0.5  °C
using indoor_t  = bound<{{0,   50},  notch<1, 10>},  round_nearest>;  // 0.1  °C
using ground_t  = bound<{{-10, 30},  notch<1, 4>},   round_nearest>;  // 0.25 °C

// Output: a coarse fused grid, integer °C with clamp.
using fused_t = bound<{-40, 60}, clamp>;

int main()
{
  // Three readings — one outdoor reading is a clear outlier (-50, below
  // grid). will_conversion_overflow rejects it before construction.
  double raw[] = { 22.5,  21.7,  -50.0,  22.0,  23.25 };

  // Weights per sensor, in 1/8 step — could be tuned by confidence.
  using weight_t = bound<{{0, 1}, notch<1, 8>}, round_nearest>;
  weight_t w_outdoor{0.5};
  weight_t w_indoor {0.375};
  weight_t w_ground{0.125};

  std::cout << "raw   accepted?   notes\n";

  // Accumulate the weighted sum as a plain double (rational sums of three
  // different fractional grids would force the result type to a rational
  // raw — fine, but heavier than needed for a 3-sensor average).
  double weighted_sum = 0;
  double weight_sum   = 0;

  auto accept = [&](double r, double w, auto tag, auto predicate) {
    bool ok = predicate(r);
    std::cout << r << "  " << (ok ? "yes" : "no ") << "  " << tag << "\n";
    if (!ok) return;
    weighted_sum += r * w;
    weight_sum   += w;
  };

  accept(raw[0], double(w_outdoor), "outdoor",
         [](double v){ return !will_conversion_overflow<outdoor_t>(v); });
  accept(raw[1], double(w_indoor),  "indoor ",
         [](double v){ return !will_conversion_overflow<indoor_t>(v); });
  accept(raw[2], double(w_outdoor), "outdoor",
         [](double v){ return !will_conversion_overflow<outdoor_t>(v); });
  accept(raw[3], double(w_ground),  "ground ",
         [](double v){ return !will_conversion_overflow<ground_t>(v); });
  accept(raw[4], double(w_indoor),  "indoor ",
         [](double v){ return !will_conversion_overflow<indoor_t>(v); });

  double fused_raw = (weight_sum > 0) ? (weighted_sum / weight_sum) : 0;
  auto fused = clamp_round<fused_t>(fused_raw);

  std::cout << "\nweighted sum: " << weighted_sum
            << ",  total weight: " << weight_sum
            << ",  raw fused: " << fused_raw << "\n";
  std::cout << "fused (clamp_round into fused_t): " << fused << "\n";

  return 0;
}
