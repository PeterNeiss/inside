// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Audio corners of ins::math: each result is the correctly rounded point of its output grid,
// with no <cmath>, the same at compile time and on every platform.

#include <iostream>

#include <inside/math.hpp>
#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units/io.hpp>
#include <inside/units/math.hpp>

using namespace ins;

int main() {
    // 1. An envelope exp(t / τ) for t = −4, −3.75, …, 4 s (a cursor in steps of 1/4), onto 2^-14.
    using seconds = quantity<si::second, inside<{{-4, 4}, per<(1 << 10)>}, round_half_away>>;
    constexpr auto tau = just<1> * si::second;
    std::cout << "t     exp(t / 1 s)\n";
    for (cursor<seconds, per<4>> n; n != end(n); ++n) {
        const seconds t{*n};
        std::cout << t << "\t" << math::exp<per<(1 << 14)>>(t / tau) << "\n";
    }

    // 2. A sweep in half octaves from 20 Hz, 2^e rounded once onto 2^-14: whole octaves double exactly.
    using octave = inside<{{0, 4}, per<(1 << 10)>}, round_half_away>;
    constexpr auto f0 = just<20> * si::hertz;
    std::cout << "\noctave  frequency\n";
    for (cursor<octave, per<2>> e; e != end(e); ++e)
        std::cout << *e << "\t" << f0 * math::exp2<per<(1 << 14)>>(*e) << "\n";
    if (f0 * math::exp2<per<(1 << 14)>>(octave{3}) != just<160> * si::hertz)
        return 1;
    if (math::exp2<per<(1 << 14)>>(octave{3}) != 8 || math::exp2<per<(1 << 14)>>(octave{4}) != 16)
        return 1;

    // 3. A tanh soft clip beside a hard clip, and cosh² − sinh² = 1.
    using drive = inside<{{-4, 4}, per<(1 << 11)>}, round_half_away>;
    using hard_clipped = inside<{{-1, 1}, per<(1 << 10)>}, clamp | round_half_away>;
    std::cout << "\nx     tanh(x)   hard clip\n";
    for (const drive x : {drive{-3}, drive{-0.5}, drive{0}, drive{1}, drive{3}})
        std::cout << x << "\t" << math::tanh(x) << "\t" << hard_clipped{x} << "\n";
    for (const drive x : {drive{-2}, drive{0}, drive{2}}) {
        const auto s = math::sinh(x);
        const auto c = math::cosh(x);
        const auto identity = c * c - s * s; // within a few notches of 1
        std::cout << "cosh^2 - sinh^2 at " << x << " = " << identity << "\n";
        if (math::abs(identity - just<1>) > just<4, (1 << 11)>)
            return 1;
    }

    // 4. Octaves with log2 and a cube-root loudness curve: exact where the answer is.
    using frequency_ratio = inside<{{0x1p-4, 16}, per<(1 << 10)>}, round_half_away>;
    using loudness = inside<{{0, 64}, per<(1 << 8)>}, round_half_away>;
    std::cout << "\nlog2(8) = " << math::log2(frequency_ratio{8}) << ", log2(0.5) = "
              << math::log2(frequency_ratio{0.5}) << ", cbrt(27) = " << math::cbrt(loudness{27}) << "\n";
    if (math::log2(frequency_ratio{8}) != 3 || math::cbrt(loudness{27}) != 3)
        return 1;
    return 0;
}
