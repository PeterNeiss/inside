// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A weighted mean of three temperature sensors with different ranges and notches: try_make rejects an outlier, the
// fixed-point weighted sum stays exact until the one rounding, and the readings are ℃ points, which have no sum,
// so the mean is the affine one, ref + Σ w·(p − ref) / Σ w.

#include <iostream>
#include <type_traits>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/inside/predicates.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;

constexpr auto deg_C = si::degree_Celsius;

int main() {
    // Three sensors in ℃, each with the precision of its hardware.
    using outdoor_reading = inside<{{-40, 60}, per<2>}, round_half_away>; // 0.5 ℃
    using indoor_reading = inside<{{0, 50}, per<10>}, round_half_away>;   // 0.1 ℃
    using ground_reading = inside<{{-10, 30}, per<4>}, round_half_away>;  // 0.25 ℃

    double raw[] = {22.5, 21.7, -50.0, 22.0, 23.25}; // -50 is an outlier, below the outdoor grid

    using weight = inside<{{0, 1}, per<8>}, round_half_away>; // tuned by confidence
    weight w_outdoor{0.5};
    weight w_indoor{0.375};
    weight w_ground{0.125};

    std::cout << "raw   accepted?   notes\n";

    // The accumulators, counted from the reference point 0 ℃. The 1/160 notch holds every reading·weight product
    // (outdoor 1/16, indoor 1/80, ground 1/32) exactly, so the running sum is lossless.
    const auto ref = point<deg_C>(just<0>);
    using weighted_celsius = quantity<deg_C, inside<{{-200, 200}, per<160>}, round_half_away>>;
    using weight_total = inside<{{0, 8}, per<8>}, round_half_away>;
    weighted_celsius weighted_sum{0, deg_C};
    weight_total weight_sum{0};

    // The accumulators hold any five readings, which their types cannot show: each += reports into one sticky
    // error, checked after the loop (the first error is kept).
    error accumulated{};
    auto accept = [&]<typename B>(std::type_identity<B>, double r, weight w, const char* tag) {
        const auto stored = B::try_make(r); // a run-time double: out of range, or NaN
        std::cout << r << "  " << (stored ? "yes" : "no ") << "  " << tag << "\n";
        if (!stored)
            return;
        const auto reading = point<deg_C>(*stored);
        weighted_sum.on_error(accumulated) += (reading - ref) * w; // a difference in ℃ times a weight: exact
        weight_sum.on_error(accumulated) += w;
    };

    accept(std::type_identity<outdoor_reading>{}, raw[0], w_outdoor, "outdoor");
    accept(std::type_identity<indoor_reading>{}, raw[1], w_indoor, "indoor ");
    accept(std::type_identity<outdoor_reading>{}, raw[2], w_outdoor, "outdoor");
    accept(std::type_identity<ground_reading>{}, raw[3], w_ground, "ground ");
    accept(std::type_identity<indoor_reading>{}, raw[4], w_indoor, "indoor ");
    if (accumulated != error{})
        return 1;

    // The fused value, whole ℃: its clamp and rounding policy saturates and rounds the quotient in one step.
    // The weight grid holds 0, so div is an expected: no accepted reading is reported, not a crash.
    using fused_celsius = quantity<deg_C, inside<{-40, 60}, clamp | round_half_away>>;
    const auto fused = ref + div<fused_celsius>(weighted_sum, weight_sum).value_or(fused_celsius{0, deg_C});

    std::cout << "\nweighted sum: " << weighted_sum << ",  total weight: " << weight_sum << "\n";
    std::cout << "fused (clamp+round into fused_celsius): " << fused << "\n";

    if (to_string(fused) != "22 ℃" || to_string(weighted_sum) != "30.87500 ℃")
        return 1;
    return 0;
}
