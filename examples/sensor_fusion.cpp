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

// Output: a coarse fused grid, integer °C. `clamp | round_nearest` lets
// `fused_t{raw_fused}` saturate AND round the rational raw quotient in
// one step — no explicit `clamp_round<fused_t>(...)` cast needed.
using fused_t = bound<{-40, 60}, clamp | round_nearest>;

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

  // Accumulate the weighted sum exactly in rational. Each accepted sample
  // is snapped onto its sensor's bound grid (lossless after the predicate),
  // lifted to rational, multiplied by the rational weight, and summed.
  rational weighted_sum{0};
  rational weight_sum{0};

  auto accept = [&]<typename B>(double r, weight_t w, auto tag, auto predicate) {
    bool ok = predicate(r);
    std::cout << r << "  " << (ok ? "yes" : "no ") << "  " << tag << "\n";
    if (!ok) return;
    B reading{r};
    weighted_sum += rational{reading} * rational{w};   // op*= via optional unwrap
    weight_sum   += rational{w};
  };

  accept.template operator()<outdoor_t>(raw[0], w_outdoor, "outdoor",
         [](double v){ return !will_conversion_overflow<outdoor_t>(v); });
  accept.template operator()<indoor_t >(raw[1], w_indoor,  "indoor ",
         [](double v){ return !will_conversion_overflow<indoor_t>(v); });
  accept.template operator()<outdoor_t>(raw[2], w_outdoor, "outdoor",
         [](double v){ return !will_conversion_overflow<outdoor_t>(v); });
  accept.template operator()<ground_t >(raw[3], w_ground,  "ground ",
         [](double v){ return !will_conversion_overflow<ground_t>(v); });
  accept.template operator()<indoor_t >(raw[4], w_indoor,  "indoor ",
         [](double v){ return !will_conversion_overflow<indoor_t>(v); });

  rational raw_fused = (weight_sum.Numerator > 0)
                       ? rational::div_unchecked(weighted_sum, weight_sum)
                       : rational{0};
  auto fused = fused_t{raw_fused};

  std::cout << "\nweighted sum: " << weighted_sum
            << ",  total weight: " << weight_sum
            << ",  raw fused: " << raw_fused << "\n";
  std::cout << "fused (clamp_round into fused_t): " << fused << "\n";

  return 0;
}
