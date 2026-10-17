// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Audio-shaped corners of beman::inside::math, each result the correctly
// rounded point of its output grid (no <cmath>, the same at compile time and on
// every platform):
//   1. An exponential curve over a stepping loop, exp(t).
//   2. A log-spaced frequency sweep, 20 Hz · 2^e, e by half octaves.
//   3. A tanh soft clipper beside a hard clip, and cosh² − sinh² = 1.
//   4. Octaves with log2, and a cube-root loudness curve.

#include <iostream>

#include <beman/inside/cmath.hpp>
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

int main() {
    // 1. Growth and decay: t = −4, −3.75, …, 4 by a cursor over seconds_t in
    //    steps of 1/4 (end(t) is the step past 4), amp on Q.14.
    using seconds_t = inside<{{-4, 4}, per<1024>}, round_nearest>;
    using amp_t     = inside<{{0, 100}, per<16384>}, round_nearest>;
    std::cout << "t     exp(t)\n";
    for (cursor<seconds_t, per<4>> t; t != end(t); ++t) {
        std::cout << t << "\t" << math::exp_into<amp_t>(t) << "\n";
    }

    // 2. Sweep: half-octave steps from 20 Hz, 2^e rounded once onto mult_t;
    //    every whole octave doubles exactly.
    using exponent_t = inside<{{0, 4}, per<1024>}, round_nearest>;
    using mult_t     = inside<{{0, 16}, per<16384>}, round_nearest>;
    std::cout << "\noctave  Hz\n";
    for (cursor<exponent_t, per<2>> e; e != end(e); ++e)
        std::cout << *e << "\t" << just<20> * math::exp2_into<mult_t>(*e) << "\n";
    if (math::exp2_into<mult_t>(exponent_t{3}) != 8 || math::exp2_into<mult_t>(exponent_t{4}) != 16)
        return 1;

    // 3. Soft clip and the hyperbolic identity.
    using drive_t = inside<{{-4, 4}, per<2048>}, round_nearest>;
    std::cout << "\nx     tanh(x)   hard clip\n";
    for (const drive_t x : {drive_t{-3}, drive_t{-0.5}, drive_t{0}, drive_t{1}, drive_t{3}})
        std::cout << x << "\t" << math::tanh(x) << "\t" << inside<{{-1, 1}, per<1024>}, clamp | round_nearest>{x}
                  << "\n";
    for (const drive_t x : {drive_t{-2}, drive_t{0}, drive_t{2}}) {
        const auto s   = math::sinh(x);
        const auto c   = math::cosh(x);
        const auto one = c * c - s * s; // within a few notches of 1
        std::cout << "cosh^2 - sinh^2 at " << x << " = " << one << "\n";
        if (math::abs(one - just<1>) > just<1, 64>)
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
