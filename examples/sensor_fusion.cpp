// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Weighted average across three temperature sensors with disparate ranges.
//
// Demonstrates:
//   - Per-sensor fixed-point grids (different lower/upper/notch each)
//   - `conversion_overflows` predicate as a soft outlier filter
//   - A fixed-point weighted sum that stays exact until the one rounding
//   - `div_into` with `clamp | round_nearest` on the output type: the average
//     saturates and rounds once (no explicit `clamp_cast<B, round_nearest>`)

#include <iostream>
#include <type_traits>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/predicates.hpp>

using namespace beman::inside;

// Three sensors covering overlapping but distinct ranges.
// Each has a different precision matched to its hardware.
using outdoor_t = inside<{{-40, 60}, per<2>}, round_nearest>; // 0.5  °C
using indoor_t  = inside<{{0, 50}, per<10>}, round_nearest>;  // 0.1  °C
using ground_t  = inside<{{-10, 30}, per<4>}, round_nearest>; // 0.25 °C

// Output: a coarse fused grid, integer °C. `clamp | round_nearest` lets
// `div_into<fused_t>` saturate AND round the quotient in one step — no
// explicit `clamp_cast<fused_t, round_nearest>(...)` cast needed.
using fused_t = inside<{-40, 60}, clamp | round_nearest>;

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
    using acc_t  = inside<{{-200, 200}, per<160>}, round_nearest>;
    using wsum_t = inside<{{0, 8}, per<8>}, round_nearest>;
    acc_t  weighted_sum{0};
    wsum_t weight_sum{0};

    auto accept = [&]<typename B>(std::type_identity<B>, double r, weight_t w, const char* tag) {
        const bool ok = !conversion_overflows<B>(r);
        std::cout << r << "  " << (ok ? "yes" : "no ") << "  " << tag << "\n";
        if (!ok)
            return;
        weighted_sum += B{r} * w; // inside-space, exact
        weight_sum += w;
    };

    accept(std::type_identity<outdoor_t>{}, raw[0], w_outdoor, "outdoor");
    accept(std::type_identity<indoor_t>{}, raw[1], w_indoor, "indoor ");
    accept(std::type_identity<outdoor_t>{}, raw[2], w_outdoor, "outdoor");
    accept(std::type_identity<ground_t>{}, raw[3], w_ground, "ground ");
    accept(std::type_identity<indoor_t>{}, raw[4], w_indoor, "indoor ");

    // The weight grid includes 0, so div_into returns expected<fused_t, errc>:
    // no accepted reading is a division by zero, reported, not a crash.
    const fused_t fused = div_into<fused_t>(weighted_sum, weight_sum).value_or(fused_t{0});

    std::cout << "\nweighted sum: " << weighted_sum << ",  total weight: " << weight_sum << "\n";
    std::cout << "fused (clamp+round into fused_t): " << fused << "\n";

    return 0;
}
