// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A 4-channel audio mixer on Q1.14 samples: gains in dB, a mix in integer notch math, peak metering with
// on_clamp, and tones whose phases in turns are exact, so sin rounds once with no <cmath>.

#include <iostream>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/inside/formats.hpp>
#include <inside/units/math.hpp>

using namespace ins;
using namespace ins::formats;

namespace {
inline constexpr auto level_kind = kind<"level", isq::dimensionless, {.distinct = true}>;
inline constexpr auto dB = unit<"dB", one, {.kind = level_kind}>;
inline constexpr auto turn = unit<"tr", just<2> * pi * si::radian>; // 1 tr = 2π rad
} // namespace

using sample = inside<{{-1, 1}, per<(1 << 14)>}, round_half_away | clamp>; // Q1.14
static_assert(sizeof(sample) == 2);

using decibels = quantity<dB, inside<{{-24, 12}, per<2>}, round_half_away>>; // 0.5 dB steps, one byte
using linear_gain = inside<{{0, 4}, per<(1 << 16)>}, round_half_away>;       // covers 10^(dB/20) ≈ [0.063, 3.98]

// 10^(L / 20 dB): the ratio is exact (a 1/40 grid), and pow_base rounds once onto linear_gain.
static linear_gain db_to_linear(decibels db) { return math::pow_base<10, linear_gain>(db / (just<20> * dB)); }

int main() {
    // Pre-gain levels: vocals hot, drums normal, bass quiet, pad ducked; the linear gains are computed once.
    decibels level[4] = {decibels{2.5, dB}, decibels{0.0, dB}, decibels{-6.0, dB}, decibels{-12.0, dB}};
    linear_gain gain[4] = {db_to_linear(level[0]), db_to_linear(level[1]), db_to_linear(level[2]),
                           db_to_linear(level[3])};

    // 16 frames of four sines: the phase is exact up to sin (onto 2^-14); mul<sample> rounds the scaled sample.
    constexpr std::size_t N = 16;
    using frame = inside<{0, N - 1}>;
    using seconds = quantity<si::second, inside<{{0, 1}, per<N>}>>;
    using cycles = inside<{{0, 1}, per<N>}>;
    using turns = quantity<turn, inside<{{-1, 1}, per<24>}>>;
    using amplitude = inside<{{0, 1}, per<(1 << 10)>}, round_half_away>;

    constexpr auto tone = just<1> * si::hertz; // one cycle over the 1 s buffer
    constexpr turns offsets[4] = {
        turns{0, turn},
        just<1, 8> * turn,  // +π/4
        just<-1, 6> * turn, // -π/3
        just<-1, 4> * turn, // -π/2
    };
    constexpr amplitude amplitudes[4] = {0.9_ins, 0.7_ins, 0.6_ins, 0.4_ins};

    sample channels[4][N];
    for (frame i : values<frame>) {
        const seconds t = (i / just<N>) * si::second; // exact: N is a nonzero point
        const cycles c = tone * t;                    // Hz · s is a plain number of cycles
        for (int ch = 0; ch < 4; ++ch) {
            const auto a = c * turn + offsets[ch]; // in turns, exact
            channels[ch][i] = mul<sample>(amplitudes[ch], math::sin<per<(1 << 14)>>(a));
        }
    }

    // The mix: each gain × sample widens to its product grid and the sum widens further, all in int64 notch
    // math. Storing it onto the bus clips, and on_clamp counts the clips.
    clamp_counter<1'000'000> clip_events{0};
    std::cout << "frame  c0      c1      c2      c3      mix    \n";
    for (frame i : values<frame>) {
        auto mix = (gain[0] * channels[0][i]) + (gain[1] * channels[1][i]) + (gain[2] * channels[2][i])
                 + (gain[3] * channels[3][i]);
        sample bus{0};
        bus.on_clamp([&](auto&, auto) { ++clip_events; }) = mix;

        std::cout << channels[0][i] << "   " << channels[1][i] << "   " << channels[2][i] << "   "
                  << channels[3][i] << "   " << bus << "\n";
    }
    std::cout << "\nclip events:     " << clip_events << "\n";

    // The clamp flag pushes a wider-grid inside into a sample, as in a std::transform over a sum bus.
    using mix_bus = inside<{{-4, 4}, per<(1 << 14)>}, round_half_away>;
    mix_bus hot;
    hot.with<clamp>() = 2.5;
    std::cout << "\nsample{mix_bus 2.5, clamp} = " << sample{hot, clamp} << "\n";

    // add_all folds the inputs exactly (the grid widens); the clamp flag clips the sum onto the bus.
    using clipped_bus = inside<{-3, 3}, clamp>;
    using channel = inside<{-1, 1}>;
    std::cout << "clipped_bus{add_all(1,-1,1,1), clamp} = "
              << clipped_bus{add_all(channel{1}, channel{-1}, channel{1}, channel{1}), clamp} << "\n";

    if (clip_events != 10 || db_to_linear(level[2]) != just<32846, (1 << 16)>)
        return 1;
    return 0;
}
