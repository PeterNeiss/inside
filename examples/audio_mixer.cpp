// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A 4-channel audio mixer on Q1.14 samples: gains in dB, a mix in integer notch math,
// peak metering with on_clamp, and tones whose phases in turns are exact,
// so sin rounds once with no <cmath>.

#include <iostream>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/inside/formats.hpp>
#include <inside/units/io.hpp>
#include <inside/units/math.hpp>

using namespace ins;
using namespace ins::formats;

namespace
{
  inline constexpr auto level_kind = kind<"level", isq::dimensionless, {.distinct = true}>;
  inline constexpr auto dB         = unit<"dB", one, {.kind = level_kind}>;
  inline constexpr auto turn       = unit<"tr", just<2> * pi * si::radian>; // 1 tr = 2π rad
  inline constexpr auto FS         = base_unit<"FS", "amplitude">;         // full scale: a sample's peak
  inline constexpr auto clip       = base_unit<"clip", "clipping">;        // one clipped frame
} // namespace

using sample = id<"[-1..1] 1/16384 [FS] round_half_away|clamp">; // Q1.14 in FS
static_assert(sizeof(sample) == 2);

using decibels    = id<"[-24..12] 1/2 [dB] round_half_away">;                   // 0.5 dB steps, one byte
using linear_gain = id<"[0..4] 1/65536 round_half_away">;                       // a ratio: 10^(dB/20) ≈ [0.063, 3.98]

// 10^(L / 20 dB): the ratio is exact (a 1/40 grid), and pow_base rounds once onto linear_gain.
static linear_gain db_to_linear(decibels db) { return math::pow_base<10, linear_gain>(db / (just<20> * dB)); }

int main()
{
  // Pre-gain levels: vocals hot, drums normal, bass quiet, pad ducked;
  // the linear gains are computed once.
  decibels    level[4] = {decibels{2.5, dB}, decibels{0.0, dB}, decibels{-6.0, dB}, decibels{-12.0, dB}};
  linear_gain gain[4]  = {db_to_linear(level[0]), db_to_linear(level[1]), db_to_linear(level[2]),
                          db_to_linear(level[3])};

  // 16 frames of four sines: the phase is exact up to sin (onto 2^-14);
  // mul<sample> rounds the scaled sample.
  constexpr std::size_t N{16};
  using frame     = id<key{grid{0, N - 1}}>;
  using seconds   = id<quantity_key<si::second, id<key{grid{{0, 1}, per<N>}}>>>;
  using cycles    = id<key{grid{{0, 1}, per<N>}}>;
  using turns     = id<"[-1..1] 1/24 [tr]">;
  using amplitude = id<"[0..1] 1/1024 [FS] round_half_away">;

  constexpr auto  tone{just<1> * si::hertz}; // one cycle over the 1 s buffer
  constexpr turns offsets[4] = {
    turns{0, turn},
    just<1, 8> * turn,  // +π/4
    just<-1, 6> * turn, // -π/3
    just<-1, 4> * turn, // -π/2
  };
  constexpr amplitude amplitudes[4] = {0.9_ins * FS, 0.7_ins * FS, 0.6_ins * FS, 0.4_ins * FS};

  sample channels[4][N];
  for (frame i : values<frame>)
  {
    const seconds t = (i / just<N>) * si::second; // exact: N is a nonzero point
    const cycles  c = tone * t;                   // Hz · s is a plain number of cycles
    for (int ch = 0; ch < 4; ++ch)
    {
      const auto a{c * turn + offsets[ch]}; // in turns, exact
      channels[ch][i] = mul<sample>(amplitudes[ch], math::sin<per<(1 << 14)>>(a));
    }
  }

  // The mix: each gain × sample widens to its product grid and the sum widens further,
  // all in int64 notch math.
  // Storing it onto the bus clips, and on_clamp counts the clips.
  id<quantity_key<clip, clamp_counter<1'000'000>>> clip_events{0, clip};
  const auto fs{[](const sample& x) { return x.numerical_value_in(FS); }}; // the table's numbers, in FS
  std::cout << "frame  c0, c1, c2, c3 and the mix, in FS\n";
  for (frame i : values<frame>)
  {
    auto mix{(gain[0] * channels[0][i]) + (gain[1] * channels[1][i]) + (gain[2] * channels[2][i])
             + (gain[3] * channels[3][i])};
    sample bus{0, FS};
    bus.on_clamp([&](auto&, auto) { ++clip_events; }) = mix;

    std::cout << i << (i < 10 ? "      " : "     ") << fs(channels[0][i]) << "   " << fs(channels[1][i]) << "   "
              << fs(channels[2][i]) << "   " << fs(channels[3][i]) << "   " << fs(bus) << "\n";
  }
  std::cout << "\nclip events:     " << clip_events << "\n";

  // The clamp flag pushes a wider-grid inside into a sample,
  // as in a std::transform over a sum bus.
  using mix_bus = id<"[-4..4] 1/16384 [FS] round_half_away">;
  const mix_bus hot{2.5, FS};
  std::cout << "\nsample{mix_bus 2.5 FS, clamp} = " << sample{hot, clamp} << "\n";

  // add_all folds the inputs exactly (the grid widens);
  // the clamp flag clips the sum onto the bus.
  using clipped_bus = id<"[-3..3] [FS] clamp">;
  using channel     = id<"[-1..1] [FS]">;
  std::cout << "clipped_bus{add_all(1, -1, 1, 1 FS), clamp} = "
            << clipped_bus{add_all(channel{1, FS}, channel{-1, FS}, channel{1, FS}, channel{1, FS}), clamp} << "\n";

  if (clip_events != just<10> * clip || db_to_linear(level[2]) != just<32846, (1 << 16)>)
    return 1;
  return 0;
}
