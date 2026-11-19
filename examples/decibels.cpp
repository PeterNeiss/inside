// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Decibels ↔ linear with math::pow_base<10> and math::log10.
// A level is a quantity in dB, a unit of our own on a dimensionless kind:
// levels add, but a level is no plain number; L / 20 dB is one, and 10^x takes it.

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units/io.hpp>
#include <inside/units/math.hpp>

#include <print>
using std::println;

using namespace ins;

namespace
{
  // A level: dimensionless, but a kind of its own, so it never turns into a plain number by itself.
  inline constexpr auto level_kind = kind<"level", isq::dimensionless, {.distinct = true}>;
  inline constexpr auto dB         = unit<"dB", one, {.kind = level_kind}>;
} // namespace

using decibels    = quantity<dB, inside<{{-24, 12}, per<2>}, round_half_away>>; // a fader: 0.5 dB steps
using linear_gain = inside<{{0x1p-8, 4}, per<(1 << 16)>}, round_half_away>;     // 10^(dB/20) ≈ [0.063, 3.98]

// dB → linear: 10^(L / 20 dB).
// The ratio is exact; pow_base rounds once (as in audio_mixer.cpp).
static constexpr linear_gain db_to_linear(decibels db)
{
  return math::pow_base<10, linear_gain>(db / (just<20> * dB));
}

// linear → dB: log10 rounds once onto 1/40, the same as 20·log10 onto 0.5 dB; the × 20 dB is exact.
// Its deduced range is rounded outward, a little wider than decibels':
// clamp states that it lands there.
static constexpr decibels linear_to_db(linear_gain amp)
{
  return decibels{math::log10<per<40>>(amp) * (just<20> * dB), clamp};
}

int main()
{
  // std::println works on insides: io.hpp has their std::formatter, which writes what << writes.
  println("dB → linear → dB round-trip:");
  println("    dB       linear         round-trip dB");

  for (cursor<decibels, 3> n; n != end(n); ++n) // 3 dB steps over the whole range
  {
    const decibels    db          {*n};
    const linear_gain lin         {db_to_linear(db)};
    const decibels    db_recovered{linear_to_db(lin)};
    println("    {}       {}         {}", db, lin, db_recovered);
    if (db_recovered != db)
      return 1;
  }

  // The landmarks: 6 dB doubles, 12 dB quadruples.
  println("\nLandmark conversions (within decibels' [-24, 12] range):");
  for (const decibels db : {decibels{-24, dB}, decibels{-12, dB}, decibels{-6, dB},
                            decibels{0  , dB}, decibels{6  , dB}, decibels{12, dB}})
  {
    linear_gain lin{db_to_linear(db)};
    println("    {}  =  {}", db, lin);
  }

  // Levels add; −6 dB is close to half the amplitude.
  if (decibels{-6, dB} + just<3> * dB != decibels{-3, dB}
      || db_to_linear(decibels{-6, dB}) != just<32846, (1 << 16)>)
    return 1;
  return 0;
}
