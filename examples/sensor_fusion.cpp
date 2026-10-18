// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Weighted average across three temperature sensors with disparate ranges.
//
// Demonstrates:
//   - Per-sensor fixed-point grids (different lower/upper/notch each)
//   - `conversion_overflows` predicate as a soft outlier filter
//   - A fixed-point weighted sum that stays exact until the one rounding
//   - `div_into` with `clamp | round_nearest` on the output type: the average
//     saturates and rounds once (no explicit `clamp_cast<B, round_nearest>`)
//   - Readings as ℃ points: a point has no sum and no product, so the
//     weighted mean is the affine one, ref + Σ w·(p − ref) / Σ w, with each
//     p − ref a temperature difference in ℃

#include <iostream>
#include <type_traits>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/predicates.hpp>
#include <beman/inside/units.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

constexpr auto deg_C = si::degree_Celsius;

// Three sensors covering overlapping but distinct ranges, in ℃.
// Each has a different precision matched to its hardware.
using outdoor_t = inside<{{-40, 60}, per<2>}, round_nearest>; // 0.5  °C
using indoor_t  = inside<{{0, 50}, per<10>}, round_nearest>;  // 0.1  °C
using ground_t  = inside<{{-10, 30}, per<4>}, round_nearest>; // 0.25 °C

// Output: a coarse fused grid, whole ℃ above the reference. `clamp |
// round_nearest` lets `div_into<fused_t>` saturate AND round the quotient in
// one step — no explicit `clamp_cast<fused_t, round_nearest>(...)` cast needed.
using fused_t = quantity<deg_C, inside<{-40, 60}, clamp | round_nearest>>;

int main() {
    // Five readings — one outdoor reading is a clear outlier (-50, below
    // grid). conversion_overflows rejects it before construction.
    double raw[] = {22.5, 21.7, -50.0, 22.0, 23.25};

    // Weights per sensor, in 1/8 step — could be tuned by confidence.
    using weight_t = inside<{{0, 1}, per<8>}, round_nearest>;
    weight_t w_outdoor{0.5};
    weight_t w_indoor{0.375};
    weight_t w_ground{0.125};

    std::cout << "raw   accepted?   notes\n";

    // Accumulate the weighted sum and total weight in fixed-point bounds — no
    // rational. The 1/160 accumulator notch holds every sensor·weight product
    // (outdoor 1/16, indoor 1/80, ground 1/32) exactly, so the running sum stays
    // lossless; the weight accumulator keeps the weights' 1/8 notch.
    // The reference point is 0 ℃; each reading counts from it.
    const auto ref = point<deg_C>(just<0>);
    using acc_t    = quantity<deg_C, inside<{{-200, 200}, per<160>}, round_nearest>>;
    using wsum_t   = inside<{{0, 8}, per<8>}, round_nearest>;
    acc_t  weighted_sum{0};
    wsum_t weight_sum{0};

    auto accept = [&]<typename B>(std::type_identity<B>, double r, weight_t w, const char* tag) {
        const bool ok = !conversion_overflows<B>(r);
        std::cout << r << "  " << (ok ? "yes" : "no ") << "  " << tag << "\n";
        if (!ok)
            return;
        const auto reading = point<deg_C>(B{r});
        weighted_sum += (reading - ref) * w; // a difference in ℃ times a weight: exact
        weight_sum += w;
    };

    accept(std::type_identity<outdoor_t>{}, raw[0], w_outdoor, "outdoor");
    accept(std::type_identity<indoor_t>{}, raw[1], w_indoor, "indoor ");
    accept(std::type_identity<outdoor_t>{}, raw[2], w_outdoor, "outdoor");
    accept(std::type_identity<ground_t>{}, raw[3], w_ground, "ground ");
    accept(std::type_identity<indoor_t>{}, raw[4], w_indoor, "indoor ");

    // The weight grid includes 0, so div_into returns expected<fused_t, errc>:
    // no accepted reading is a division by zero, reported, not a crash.
    const auto fused = ref + div_into<fused_t>(weighted_sum, weight_sum).value_or(fused_t{0});

    std::cout << "\nweighted sum: " << weighted_sum << ",  total weight: " << weight_sum << "\n";
    std::cout << "fused (clamp+round into fused_t): " << fused << "\n";

    if (to_string(fused) != "22 ℃" || to_string(weighted_sum) != "30.87500 ℃")
        return 1;
    return 0;
}
