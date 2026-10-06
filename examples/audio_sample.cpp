// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Signed fixed-point audio samples in [-1, 1] with mixing.
// A 1/16384 notch gives ~Q1.14 precision and fits uint16 storage.
// Mixing two waveforms can exceed [-1, 1] at peaks; `with_clamp()` saturates
// to the boundary instead of wrapping or throwing.
//
// No `<cmath>` in the example body — the sine waveforms come from
// `beman::inside::math::sin` on a radians-valued inside. The 2π scaling is one
// inside × inside multiply via inline `just<math::two_pi>`.

#include <iostream>
#include <vector>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/cmath.hpp>

using namespace beman::inside;

int main() {
    // Signed fractional grid: 32769 steps in [-1, 1] -> uint16 storage
    using sample = inside<{{-1, 1}, per<16384>}, round_nearest>;
    static_assert(sizeof(sample) == 2);

    // Two sine waves; combined peaks exceed unity to exercise clamping.
    // wave_b is phase-shifted by -π/6 (= 30° behind wave_a).
    constexpr std::size_t N = 8;

    using time_t    = inside<{{0, 1}, per<N>}, round_nearest>;
    using offset_t  = inside<{{-2, 2}, per<16384>}, round_nearest>;
    using angle_t   = inside<{{-4, 10}, per<16384>}, round_nearest | f64>;
    using gainfac_t = inside<{{0, 1}, per<1024>}, round_nearest>;

    constexpr offset_t  off_a{0};
    constexpr offset_t  off_b{-math::pi / just<6>};
    constexpr gainfac_t gain_a{0.8_ins};
    constexpr gainfac_t gain_b{0.6_ins};

    std::vector<sample> wave_a(N), wave_b(N);
    for (auto i : inside_range<{0, N - 1}>{}) {
        time_t  t{i / just<N>};                      // inside / inside — give the divisor N a grid
        angle_t base{t * math::two_pi};              // inside × inside, snap
        angle_t a_a{base + off_a};                   // inside + inside
        angle_t a_b{base + off_b};                   // inside + inside
        wave_a[i] = sample{gain_a * math::sin(a_a)}; // inside × inside
        wave_b[i] = sample{gain_b * math::sin(a_b)};
    }

    std::cout << "i  a       b       a+b (clamped)\n";
    for (std::size_t i = 0; i < N; ++i) {
        sample mixed{0};
        mixed.with_clamp() = wave_a[i] + wave_b[i];
        std::cout << i << "  " << wave_a[i] << "    " << wave_b[i] << "    " << mixed << "\n";
    }

    // Peak detection over the buffer (max magnitude). `math::abs` keeps us in
    // the inside world; the result is signed-stripped via |x| <= max.
    using abs_t = inside<{{0, 1}, per<16384>}, round_nearest>;
    abs_t peak{0};
    for (auto s : wave_a) {
        abs_t mag{math::abs(s)};
        if (mag > peak)
            peak = mag;
    }
    std::cout << "\npeak |a| = " << peak << " (expected ~0.8)\n";

    return 0;
}
