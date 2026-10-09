// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Audio-shaped corners of beman::inside::math, each result the correctly
// rounded point of its output grid (no <cmath>, the same at compile time and on
// every platform):
//   1. An exponential curve over a stepping loop, exp(t).
//   2. A log-spaced frequency sweep, 20 Hz · 2^(step/4).
//   3. A tanh soft clipper beside a hard clip, and cosh² − sinh² = 1.
//   4. Octaves with log2, and a cube-root loudness curve.

#include <iostream>

#include <beman/inside/cmath.hpp>
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

int main() {
    // 1. Growth and decay: t = −4, −3.75, …, 4 by a cursor over time_t in
    //    steps of 1/4 (it may step one past 4), amp on Q.14.
    using time_t = inside<{{-4, 4}, per<1024>}, round_nearest>;
    using amp_t  = inside<{{0, 100}, per<16384>}, round_nearest>;
    std::cout << "t     exp(t)\n";
    for (cursor<time_t, per<4>> t; t != end(t); ++t) {
        std::cout << t << "\t" << math::exp_into<amp_t>(t) << "\n";
    }

    // 2. Sweep: 4 steps per octave from 20 Hz; every 4th step doubles exactly.
    using exponent_t = inside<{{0, 4}, per<1024>}, round_nearest>;
    using mult_t     = inside<{{0, 16}, per<16384>}, round_nearest>;
    std::cout << "\nstep  Hz\n";
    for (int step = 0; step <= 16; step += 2) {
        const mult_t m{math::exp2(exponent_t{inside<{0, 16}>{step} / just<4>})};
        std::cout << step << "\t" << just<20> * m << "\n";
        if (step % 4 == 0 && rational{just<20> * m} != rational{20 << (step / 4)})
            return 1;
    }

    // 3. Soft clip and the hyperbolic identity.
    using drive_t = inside<{{-4, 4}, per<1024>}, round_nearest>;
    std::cout << "\nx     tanh(x)   hard clip\n";
    for (const drive_t x : {drive_t{-3}, drive_t{-0.5}, drive_t{0}, drive_t{1}, drive_t{3}})
        std::cout << x << "\t" << math::tanh(x) << "\t" << inside<{{-1, 1}, per<1024>}, clamp | round_nearest>{x}
                  << "\n";
    for (const drive_t x : {drive_t{-2}, drive_t{0}, drive_t{2}}) {
        const auto s   = math::sinh(x);
        const auto c   = math::cosh(x);
        const auto one = c * c - s * s; // within a few notches of 1
        std::cout << "cosh^2 - sinh^2 at " << x << " = " << one << "\n";
        if (abs(rational{one} - rational{1}) > rational{1, 64})
            return 1;
    }

    // 4. Octaves and a perceptual curve: exact where the answer is.
    using ratio_t = inside<{{0x1p-4, 16}, per<1024>}, round_nearest>;
    using vol_t   = inside<{{0, 64}, per<256>}, round_nearest>;
    std::cout << "\nlog2(8) = " << math::log2(ratio_t{8}) << ", log2(0.5) = " << math::log2(ratio_t{0.5})
              << ", cbrt(27) = " << math::cbrt(vol_t{27}) << "\n";
    if (math::log2(ratio_t{8}) != 3 || math::cbrt(vol_t{27}) != 3)
        return 1;
    return 0;
}
