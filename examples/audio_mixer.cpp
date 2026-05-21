// 4-channel audio mixer with per-channel dB gain and master-bus peak detection.
//
// Demonstrates:
//   - Signed Q1.14 sample storage (1/16384 notch)
//   - `add_all` to sum N channels into a master bus
//   - `with(on_clamp, on_overflow)` multi-action probe for peak metering
//   - Rational dB→linear gain stored as `rational` and applied via `*`
//   - `saturated_cast` to push the mixed value back into the sample grid

#include <cmath>
#include <iostream>
#include <numbers>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

// Q1.14 signed samples in [-1, 1].
using sample_t = bound<{{-1, 1}, notch<1, 16384>}, round_nearest | clamp>;
static_assert(sizeof(sample_t) == 2);

// dB gain bounded to [-24, 12] dB with 0.5 dB step — fits in a uint8.
using db_t = bound<{{-24, 12}, notch<1, 2>}, round_nearest>;

// Decibels → linear amplitude. dB = 20 * log10(amp), so amp = 10^(dB/20).
// Computed in `double` at the API boundary; the result lives as a rational
// so subsequent grid arithmetic stays exact.
static rational db_to_linear(db_t db)
{
  return rational{std::pow(10.0, double(db) / 20.0)};
}

int main()
{
  // Four channels — pre-gain levels: vocals hot, drums normal, bass quiet,
  // pad ducked.
  db_t gain[4] = { db_t{2.5}, db_t{0.0}, db_t{-6.0}, db_t{-12.0} };

  // 8 sample frames of synthetic audio (sin waves at different phases).
  constexpr std::size_t N = 8;
  sample_t channels[4][N];
  for (std::size_t i = 0; i < N; ++i)
  {
    double t = static_cast<double>(i) / N;
    channels[0][i] = 0.9 * std::sin(2 * std::numbers::pi * t);
    channels[1][i] = 0.7 * std::sin(2 * std::numbers::pi * t + std::numbers::pi/4);
    channels[2][i] = 0.6 * std::sin(2 * std::numbers::pi * t - std::numbers::pi/3);
    channels[3][i] = 0.4 * std::sin(2 * std::numbers::pi * t + std::numbers::pi/2);
  }

  int clip_events    = 0;
  int overflow_events = 0;

  std::cout << "frame  c0      c1      c2      c3      mix    \n";
  for (std::size_t i = 0; i < N; ++i)
  {
    // Per-channel post-gain values, expressed in rational space so the
    // mixed sum stays exact until the final write to sample_t. The
    // checked `*` returns `slim::optional` because cross-multiplied
    // denominators (both ~2^53 from double conversion) can overflow imax;
    // `.value_or(0_r)` collapses that hazard to a silent zero.
    rational g0 = (db_to_linear(gain[0]) * rational{channels[0][i]}).value_or(0_r);
    rational g1 = (db_to_linear(gain[1]) * rational{channels[1][i]}).value_or(0_r);
    rational g2 = (db_to_linear(gain[2]) * rational{channels[2][i]}).value_or(0_r);
    rational g3 = (db_to_linear(gain[3]) * rational{channels[3][i]}).value_or(0_r);

    double mix = double(g0) + double(g1) + double(g2) + double(g3);

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

  // Standalone: saturated_cast pushes a wider-grid bound into sample_t.
  // Common idiom inside std::transform over a sum bus.
  using mixbus_t = bound<{{-4, 4}, notch<1, 16384>}, round_nearest>;
  mixbus_t hot;
  hot.with_clamp() = 2.5;
  std::cout << "\nsaturated_cast<sample_t>(mixbus 2.5) = "
            << saturated_cast<sample_t>(hot) << "\n";

  // `add_all_into<Target>` folds N inputs and clips back into Target —
  // the audio-mix pipeline in one expression.
  using bus_t = bound<{-3, 3}, clamp>;
  using ch_t  = bound<{-1, 1}>;
  std::cout << "add_all_into<bus_t>(1,-1,1,1) = "
            << add_all_into<bus_t>(ch_t{1}, ch_t{-1}, ch_t{1}, ch_t{1}) << "\n";

  return 0;
}
