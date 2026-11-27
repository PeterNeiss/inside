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

namespace
{
  inline constexpr auto level_kind = kind<"level", isq::dimensionless, {.distinct = true}>;
  // an octave: log₂ of a frequency ratio
  inline constexpr auto oct = unit<"oct", one, {.kind = level_kind}>;
  // full scale: a sample's peak
  inline constexpr auto Pk  = base_unit<"Pk", "peak">;
} // namespace

int main()
{
  using namespace ins::si;

  // 1. An envelope exp(t / τ) for t = −4, −3.75, …, 4 s (a cursor in steps of 1/4),
  // onto 2^-14.
  using seconds = id<"[-4..4] 1/1024 [s] round_half_away">;
  constexpr auto tau{just<1> * si::second};
  std::cout << "t     exp(t / 1 s)\n";
  for (cursor<seconds, per<4>> n; n != end(n); ++n)
  {
    const seconds t{*n};
    std::cout << t << "\t" << math::exp<per<(1 << 14)>>(t / tau) << "\n";
  }

  // 2. A sweep in half octaves from 20 Hz: an octave is a level, so the ratio 2^(e / 1 oct)
  // rounds once onto 2^-14, and whole octaves double exactly.
  using octaves = id<"[0..4] 1/1024 [oct] round_half_away">;
  constexpr auto f0{just<20> * si::hertz};
  auto exp = [](octaves o) { return math::exp2<per<1 << 14>>(o / oct); };

  std::cout << "\nsweep   frequency\n";
  for (cursor<octaves, per<2>> e; e != end(e); ++e)
    std::cout << *e << "\t" << f0 * exp(*e) << "\n";

  if (f0 * exp(octaves{3, oct}) != just<160> * si::hertz)
    return 1;

  if (exp(octaves{3, oct}) != 8 || exp(octaves{4, oct}) != 16)
    return 1;

  // 3. A tanh soft clip beside a hard clip, on samples in full scale (tanh takes the number x / 1 Pk),
  // and cosh² − sinh² = 1.
  using drive        = id<"[-4..4] 1/2048 [Pk] round_half_away">;
  using hard_clipped = id<"[-1..1] 1/1024 [Pk] clamp|round_half_away">;
  constexpr auto full_scale{just<1> * Pk};
  std::cout << "\nx       tanh(x)   hard clip\n";
  for (const drive x : {drive{-3, Pk}, drive{-0.5, Pk}, drive{0, Pk}, drive{1, Pk}, drive{3, Pk}})
    std::cout << x << "\t" << math::tanh(x / full_scale) * full_scale << "\t" << hard_clipped{x} << "\n";
  using number = id<"[-4..4] 1/2048 round_half_away">;
  for (const number x : {number{-2}, number{0}, number{2}})
  {
    const auto s       {math::sinh(x)};
    const auto c       {math::cosh(x)};
    const auto identity{c * c - s * s}; // within a few notches of 1
    std::cout << "cosh^2 - sinh^2 at " << x << " = " << identity << "\n";
    if (math::abs(identity - just<1>) > just<4, (1 << 11)>)
      return 1;
  }

  // 4. A frequency ratio in octaves, log2 of it times 1 oct, and a cube-root loudness curve of an intensity ratio:
  // exact where the answer is.
  using frequency_ratio = id<"[0x1p-4..16] 1/1024 round_half_away">;
  using intensity_ratio = id<"[0..64] 1/256 round_half_away">;
  std::cout << "\nratio 8 = "   << math::log2(frequency_ratio{8}) * oct
            << ", ratio 0.5 = " << math::log2(frequency_ratio{0.5}) * oct
            << ", cbrt(27) = "  << math::cbrt(intensity_ratio{27}) << "\n";
  if (math::log2(frequency_ratio{8}) * oct != just<3> * oct || math::cbrt(intensity_ratio{27}) != 3)
    return 1;
  return 0;
}
