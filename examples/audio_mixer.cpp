// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// 4-channel audio mixer with per-channel dB gain and master-bus peak detection.
//
// Demonstrates:
//   - Signed Q1.14 sample storage (1/16384 notch)
//   - Q-format gain inside; per-frame mix runs entirely in integer notch math
//   - `with(on_clamp, on_overflow)` multi-action probe for peak metering
//   - `clamp_cast` to push the mixed value back into the sample grid
//   - `units::pow_base_into` / `mul_into`: one rounding straight into the
//     target type, never a wide product narrowed afterwards
//   - Units: channel gains in dB (a level unit of our own, as in
//     decibels.cpp), frame times in s, the tone in Hz, and its phase in turns
//     (tr = 2π rad), so the phase offsets 1/8, −1/6 and −1/4 tr are exact and
//     `units::sin_into` reduces them exactly — no approximation of π, no `<cmath>`
//   - `values<T>` for the frame loops

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/formats.hpp>
#include <beman/inside/units_math.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
inline constexpr quantity_spec level_def{"level", isq::dimensionless, is_kind};
inline constexpr named_unit    decibel_def{"dB", one, spec_c<level_def>};
inline constexpr auto          dB = unit_c<decibel_def>;
inline constexpr named_unit    turn_def{"tr", mag<2> * mag_pi * si::radian};
inline constexpr auto          tr = unit_c<turn_def>;
} // namespace

// Q1.14 signed samples in [-1, 1].
using sample_t = inside<{{-1, 1}, per<16384>}, round_nearest | clamp>;
static_assert(sizeof(sample_t) == 2);

// dB gain bounded to [-24, 12] dB with 0.5 dB step — fits in a uint8.
using db_t = quantity<dB, inside<{{-24, 12}, per<2>}, round_nearest>>;

// Linear gain on a notch grid wide enough to cover 10^(dB/20) for dB∈[-24,12]
// (i.e. ~[0.063, 3.98]) with 1/65536 resolution — well below per-step audible.
using gain_t = inside<{{0, 4}, per<65536>}, round_nearest>;

// Decibels → linear amplitude via 10^(L / 20 dB). The ratio is exact (a 1/40
// grid), and pow_base_into rounds 10^x once onto gain_t — integer/constexpr,
// no std::pow, no FPU boundary anywhere.
static gain_t db_to_linear(db_t db) { return units::pow_base_into<gain_t, 10>(db / (just<20> * dB)); }

int main() {
    // Four channels — pre-gain levels: vocals hot, drums normal, bass quiet,
    // pad ducked.
    db_t gain[4] = {db_t{2.5}, db_t{0.0}, db_t{-6.0}, db_t{-12.0}};

    // Hoist the per-channel gain computation out of the per-sample loop: the
    // linear gain (pow_base<10>) is constant across the buffer.
    gain_t lin[4] = {db_to_linear(gain[0]), db_to_linear(gain[1]), db_to_linear(gain[2]), db_to_linear(gain[3])};

    // 16 sample frames of synthetic audio (sine waves at different phases).
    // All synthesis is exact arithmetic on quantities until sin_into's one
    // rounding — same idiom as the mix pipeline below.
    constexpr std::size_t N = 16;
    using frame_t           = inside<{0, N - 1}>;

    using seconds_t = quantity<si::second, inside<{{0, 1}, per<N>}>>;
    using cycles_t  = inside<{{0, 1}, per<N>}>;
    using offset_t  = quantity<tr, inside<{{-1, 1}, per<24>}>>;
    using amp_t     = math::unit_amplitude<16384>;
    using gainfac_t = inside<{{0, 1}, per<1024>}, round_nearest>;

    constexpr auto     tone       = just<1> * si::hertz; // one cycle over the 1 s buffer
    constexpr offset_t offsets[4] = {
        offset_t{0},
        just<1, 8> * tr,  // +π/4
        just<-1, 6> * tr, // -π/3
        just<-1, 4> * tr  // -π/2
    };
    constexpr gainfac_t gains[4] = {0.9_ins, 0.7_ins, 0.6_ins, 0.4_ins};

    sample_t channels[4][N];
    for (frame_t i : values<frame_t>) {
        const seconds_t t = (i / just<N>)*si::second; // exact: N is a nonzero point
        const cycles_t  c = tone * t;                 // Hz · s is a plain number of cycles
        for (int ch = 0; ch < 4; ++ch) {
            const auto a    = c * tr + offsets[ch]; // in turns, exact
            channels[ch][i] = mul_into<sample_t>(gains[ch], units::sin_into<amp_t>(a));
        }
    }

    clamp_counter<1'000'000> clip_events{0};
    clamp_counter<1'000'000> overflow_events{0};

    std::cout << "frame  c0      c1      c2      c3      mix    \n";
    for (frame_t i : values<frame_t>) {
        // Each (gain × sample) widens to an inside on the product grid; the four-
        // way `+` chain widens further. Everything stays in int64 notch math —
        // no rational cross-multiply, no float in the hot loop.
        auto mix = (lin[0] * channels[0][i]) + (lin[1] * channels[1][i]) + (lin[2] * channels[2][i]) +
                   (lin[3] * channels[3][i]);

        // `with(...)` packs two callbacks into a single write. on_overflow
        // fires for an integer-cast overflow on the way through assignment;
        // on_clamp fires when the value lands outside [-1, 1] and is clipped.
        sample_t bus{0};
        bus.with(on_overflow([&](auto&, errc) { ++overflow_events; }), on_clamp([&](auto&, auto) { ++clip_events; })) =
            mix;

        std::cout << channels[0][i] << "   " << channels[1][i] << "   " << channels[2][i] << "   " << channels[3][i]
                  << "   " << bus << "\n";
    }

    std::cout << "\nclip events:     " << clip_events << "\n";
    std::cout << "overflow events: " << overflow_events << "\n";

    // Standalone: clamp_cast pushes a wider-grid inside into sample_t.
    // Common idiom inside std::transform over a sum bus.
    using mixbus_t = inside<{{-4, 4}, per<16384>}, round_nearest>;
    mixbus_t hot;
    hot.with_clamp() = 2.5;
    std::cout << "\nclamp_cast<sample_t>(mixbus 2.5) = " << clamp_cast<sample_t>(hot) << "\n";

    // `add_all` folds N inputs exactly (the grid widens); `clamp_cast` clips the
    // sum back into the bus — the audio-mix pipeline in one expression.
    using bus_t = inside<{-3, 3}, clamp>;
    using ch_t  = inside<{-1, 1}>;
    std::cout << "clamp_cast<bus_t>(add_all(1,-1,1,1)) = "
              << clamp_cast<bus_t>(add_all(ch_t{1}, ch_t{-1}, ch_t{1}, ch_t{1})) << "\n";

    if (clip_events != 10 || overflow_events != 0 || db_to_linear(gain[2]) != gain_t{rational{32846, 65536}})
        return 1;
    return 0;
}
