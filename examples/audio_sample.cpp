// Signed fixed-point audio samples in [-1, 1] with mixing.
// A 1/16384 notch gives ~Q1.14 precision and fits uint16 storage.
// Mixing two waveforms can exceed [-1, 1] at peaks; `with_clamp()` saturates
// to the boundary instead of wrapping or throwing.
//
// No `<cmath>` in the example body — the sine waveforms come from
// `bnd::math::sin` on a radians-valued bound. The 2π scaling is one
// bound × bound multiply via inline `just<math::two_pi>`.

#include <iostream>
#include <vector>

#include "bound/bound.hpp"
#include "bound/cmath.hpp"
#include "bound/print.hpp"

using namespace bnd;

int main()
{
  // Signed fractional grid: 32769 steps in [-1, 1] -> uint16 storage
  using sample = bound<{{-1, 1}, notch<1, 16384>}, round_nearest>;
  static_assert(sizeof(sample) == 2);

  // Two sine waves; combined peaks exceed unity to exercise clamping.
  // wave_b is phase-shifted by -π/6 (= 30° behind wave_a).
  constexpr std::size_t N = 8;

  using time_t    = bound<{{ 0,  1}, notch<1,     N>}, round_nearest>;
  using offset_t  = bound<{{-2,  2}, notch<1, 16384>}, round_nearest>;
  using angle_t   = bound<{{-4, 10}, notch<1, 16384>}, round_nearest>;
  using gainfac_t = bound<{{ 0,  1}, notch<1,  1024>}, round_nearest>;

  constexpr offset_t  off_a{0};
  constexpr offset_t  off_b{-math::pi / just<6>};
  constexpr gainfac_t gain_a{rational{8, 10}};
  constexpr gainfac_t gain_b{rational{6, 10}};

  std::vector<sample> wave_a(N), wave_b(N);
  for (auto i : bound_range<{0, N-1}>{})
  {
    time_t  t{i / N};
    angle_t base{t * just<math::two_pi>};                            // bound × bound, snap
    angle_t a_a{base + off_a};                                        // bound + bound
    angle_t a_b{base + off_b};                                        // bound + bound
    wave_a[i] = sample{gain_a * math::sin(a_a)};                     // bound × bound
    wave_b[i] = sample{gain_b * math::sin(a_b)};
  }

  std::cout << "i  a       b       a+b (clamped)\n";
  for (std::size_t i = 0; i < N; ++i)
  {
    sample mixed{0};
    mixed.with_clamp() = rational::add_unchecked(rational{wave_a[i]},
                                                  rational{wave_b[i]});
    std::cout << i << "  "
              << wave_a[i] << "    "
              << wave_b[i] << "    "
              << mixed << "\n";
  }

  // Peak detection over the buffer (max magnitude). `math::abs` keeps us in
  // the bound world; the result is signed-stripped via |x| <= max.
  using abs_t = bound<{{0, 1}, notch<1, 16384>}, round_nearest>;
  abs_t peak{0};
  for (auto s : wave_a)
  {
    abs_t mag{math::abs(s)};
    if (rational{mag} > rational{peak}) peak = mag;
  }
  std::cout << "\npeak |a| = " << peak << " (expected ~0.8)\n";

  return 0;
}
