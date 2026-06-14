// 4-channel audio mixer with per-channel dB gain and master-bus peak detection.
//
// Demonstrates:
//   - Signed Q1.14 sample storage (1/16384 notch)
//   - Q-format gain bound; per-frame mix runs entirely in integer notch math
//   - `with(on_clamp, on_overflow)` multi-action probe for peak metering
//   - `clamp_cast` to push the mixed value back into the sample grid
//   - `bnd::math::sin` on a radians-valued bound — no `.Raw` bit-twiddling,
//     no `<cmath>`, and no `rational::*_unchecked` in the example body.
//     The 2π scaling is a single bound × bound multiply via a
//     singleton-bound wrap of `math::two_pi`.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/cmath.hpp"
#include "bound/formats.hpp"
#include "bound/print.hpp"

using namespace bnd;

// Q1.14 signed samples in [-1, 1].
using sample_t = bound<{{-1, 1}, notch<1, 16384>}, round_nearest | clamp>;
static_assert(sizeof(sample_t) == 2);

// dB gain bounded to [-24, 12] dB with 0.5 dB step — fits in a uint8.
using db_t = bound<{{-24, 12}, notch<1, 2>}, round_nearest>;

// Linear gain on a notch grid wide enough to cover 10^(dB/20) for dB∈[-24,12]
// (i.e. ~[0.063, 3.98]) with 1/65536 resolution — well below per-step audible.
using gain_t = bound<{{0, 4}, notch<1, 65536>}, round_nearest>;

// dB/20 intermediate: dB ∈ [-24, 12] → [-1.2, 0.6]. `real` (the math operand
// feeding pow_base<10>) requires a dyadic grid, so use a power-of-two notch and
// integer endpoints (the 1/40 step is snapped onto 1/65536, far below audible).
using db_div20_t = bound<{{-2, 1}, notch<1, 65536>}, round_nearest | real>;

// Decibels → linear amplitude via 10^(dB/20). The library's pow_base<10>
// derives log2(10) at compile time from its own log2 implementation, so the
// whole chain is integer/constexpr — no std::pow, no FPU boundary anywhere.
static gain_t db_to_linear(db_t db)
{
  db_div20_t exponent{db / just<20>};
  return gain_t{math::pow_base<10>(exponent)};
}

int main()
{
  // Four channels — pre-gain levels: vocals hot, drums normal, bass quiet,
  // pad ducked.
  db_t gain[4] = { db_t{2.5}, db_t{0.0}, db_t{-6.0}, db_t{-12.0} };

  // Hoist the per-channel gain computation out of the per-sample loop: the
  // linear gain (pow_base<10>) is constant across the buffer.
  gain_t lin[4] = { db_to_linear(gain[0]), db_to_linear(gain[1]),
                    db_to_linear(gain[2]), db_to_linear(gain[3]) };

  // 8 sample frames of synthetic audio (sine waves at different phases).
  // All synthesis flows through bound arithmetic — bound + bound, bound ×
  // bound, math::sin(bound) — same idiom as the mix pipeline below. The
  // only place we reach for `rational` is the singleton-bound wrap of
  // the irrational `math::two_pi`, which lets `t * two_pi_bnd` resolve
  // through the existing bound × bound operator.
  constexpr std::size_t N = 16;

  using time_t    = bound<{{ 0,  1}, notch<1,     N>}, round_nearest>;
  using offset_t  = bound<{{-2,  2}, notch<1, 16384>}, round_nearest>;
  using angle_t   = bound<{{-4, 10}, notch<1, 16384>}, round_nearest | real>;
  using gainfac_t = bound<{{ 0,  1}, notch<1,  1024>}, round_nearest>;

  constexpr offset_t offsets[4] =
  {
      0,
      math::pi / just<4>,  // +π/4
    - math::pi / just<3>,  // -π/3
    - math::pi / just<2>   // +π/2
  };
  constexpr gainfac_t gains[4] =
  {
    0.9_b,
    0.7_b,
    0.6_b,
    0.4_b
  };

  sample_t channels[4][N];
  for (auto i : bound_range<{0, N-1}>{})
  {
    time_t  t{i / just<N>};   // bound / bound — give the divisor N a grid
    // `t * two_pi_bnd` returns a plain `bound`: the static safety check on
    // rational multiplication folds out the optional wrapper because the
    // result grid's numerator/denominator products provably fit in umax.
    // Snap the result into angle_t to land on the 1/16384 grid; the
    // subsequent bound + bound then stays on a friendly notch.
    angle_t base{t * math::two_pi};                            // bound × bound, snap
    for (int ch = 0; ch < 4; ++ch)
    {
      angle_t a{base + offsets[ch]};                                 // bound + bound, snap
      channels[ch][i] = sample_t{gains[ch] * math::sin(a)};          // bound × bound
    }
  }

  counter<1'000'000> clip_events{0};
  counter<1'000'000> overflow_events{0};

  std::cout << "frame  c0      c1      c2      c3      mix    \n";
  for (std::size_t i = 0; i < N; ++i)
  {
    // Each (gain × sample) widens to a bound on the product grid; the four-
    // way `+` chain widens further. Everything stays in int64 notch math —
    // no rational cross-multiply, no float in the hot loop.
    auto mix = (lin[0] * channels[0][i])
             + (lin[1] * channels[1][i])
             + (lin[2] * channels[2][i])
             + (lin[3] * channels[3][i]);

    // `with(...)` packs two callbacks into a single write. on_overflow
    // fires for an integer-cast overflow on the way through assignment;
    // on_clamp fires when the value lands outside [-1, 1] and is clipped.
    sample_t bus{0};
    bus.with(
      on_overflow([&](auto&, errc)        { ++overflow_events; }),
      on_clamp   ([&](auto&, auto)         { ++clip_events;    })
    ) = mix;

    std::cout << channels[0][i] << "   "
              << channels[1][i] << "   "
              << channels[2][i] << "   "
              << channels[3][i] << "   "
              << bus << "\n";
  }

  std::cout << "\nclip events:     " << clip_events << "\n";
  std::cout << "overflow events: " << overflow_events << "\n";

  // Standalone: clamp_cast pushes a wider-grid bound into sample_t.
  // Common idiom inside std::transform over a sum bus.
  using mixbus_t = bound<{{-4, 4}, notch<1, 16384>}, round_nearest>;
  mixbus_t hot;
  hot.with_clamp() = 2.5;
  std::cout << "\nclamp_cast<sample_t>(mixbus 2.5) = "
            << clamp_cast<sample_t>(hot) << "\n";

  // `add_all_into<Target>` folds N inputs and clips back into Target —
  // the audio-mix pipeline in one expression.
  using bus_t = bound<{-3, 3}, clamp>;
  using ch_t  = bound<{-1, 1}>;
  std::cout << "add_all_into<bus_t>(1,-1,1,1) = "
            << add_all_into<bus_t>(ch_t{1}, ch_t{-1}, ch_t{1}, ch_t{1}) << "\n";

  return 0;
}
