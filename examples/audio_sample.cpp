// Signed fixed-point audio samples in [-1, 1] with mixing.
// A 1/16384 notch gives ~Q1.14 precision and fits uint16 storage.
// Mixing two waveforms can exceed [-1, 1] at peaks; `with_clamp()` saturates
// to the boundary instead of wrapping or throwing.

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <vector>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

int main()
{
  // Signed fractional grid: 32769 steps in [-1, 1] -> uint16 storage
  using sample = bound<{{-1, 1}, *(1_r/16384)}, round_nearest>;
  static_assert(sizeof(sample) == 2);

  // Two sine waves; combined peaks exceed unity to exercise clamping.
  constexpr std::size_t N = 8;
  std::vector<sample> wave_a(N), wave_b(N);
  for (std::size_t i = 0; i < N; ++i)
  {
    double t = static_cast<double>(i) / N;
    wave_a[i] = 0.8 * std::sin(2 * std::numbers::pi * t);
    wave_b[i] = 0.6 * std::sin(2 * std::numbers::pi * t - std::numbers::pi/6);
  }

  std::cout << "i  a       b       a+b (clamped)\n";
  for (std::size_t i = 0; i < N; ++i)
  {
    sample mixed{0};
    mixed.with_clamp() = double(wave_a[i]) + double(wave_b[i]);
    std::cout << i << "  "
              << wave_a[i] << "    "
              << wave_b[i] << "    "
              << mixed << "\n";
  }

  // Peak detection over the buffer (max magnitude)
  double peak = 0;
  for (auto s : wave_a) peak = std::max(peak, std::abs(double(s)));
  std::cout << "\npeak |a| = " << peak << " (expected ~0.8)\n";

  return 0;
}
