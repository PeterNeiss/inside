// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Ohm's law with units: resistors in kΩ and Ω add exactly,
// a quotient rounds only where the program says so,
// and a divisor that may be 0 Ω gives an error value instead of a garbage current.

#include <iostream>

#include <inside/inside.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>
#include <inside/units/math.hpp>

using namespace ins;

namespace
{
  constexpr auto kohm = si::kilo(si::ohm);
  constexpr auto mA   = si::milli(si::ampere);
} // namespace

int main()
{
  // 1. 4.7 kΩ + 330 Ω + 1.2 kΩ: the sum runs in Ω,
  // 6230 Ω on the exact hull of the three grids.
  using kiloohms = id<"[1..100] 1/10 [kohm]">;                     // 0.1 kΩ steps, at least 1 kΩ
  using ohms     = id<"[1..1000] [ohm]">;                          // whole ohms

  const kiloohms r1{4.7_q * kohm};
  const ohms     r2{330, si::ohm};
  const kiloohms r3{1.2_q * kohm};
  const auto     r {r1 + r2 + r3};
  std::cout << "R      = " << r << '\n';
  if (to_string(r) != "6230 Ω")
    return 1;

  // 2. I = V / R: every R is at least 1 kΩ, so 9/6230 A is a plain value;
  // it rounds once onto µA.
  using volts     = id<"[1..12] [V]">;                                           // a supply
  using milliamps = id<"[0..20] 1/1000 [mA] round_half_away">;                   // mA to the µA

  const volts supply{9, si::volt};
  const auto  i     {value_cast<milliamps>(supply / r)};
  std::cout << "I      = " << i << '\n'; // 1.445 mA
  if (to_string(i) != "1.445 mA")
    return 1;

  // 3. A potentiometer's grid holds 0 Ω, so V / R is a std::expected
  // and the zero setting an error value.
  using pot_ohms = id<"[0..10'000] [ohm]">;
  const pot_ohms pot{0, si::ohm};
  const auto     ip {supply / pot};
  if (ip)
    return 1;
  std::cout << "I(pot) = error: " << ip.error() << '\n';
  if (ip.error() != error::division_by_zero)
    return 1;

  // 4. P = V·I: V·mA is a power, so it converts to watts exactly (×1/1000).
  using milliwatts = id<"[0..300] 1/1000 [mW]">;
  const milliwatts p = supply * i;
  std::cout << "P      = " << p << '\n'; // 13.005 mW
  if (to_string(p) != "13.005 mW")
    return 1;

  // 5. An RC low-pass: τ = R·C with C = 100 nF is a time,
  // 0.623 ms exactly (to the product's notch, 10^-6 ms).
  using nanofarads = id<"[1..1000] [nF]">;
  const nanofarads c  {100, si::nano(si::farad)};
  const auto       tau{(r * c).in(si::milli(si::second))};
  std::cout << "tau    = " << tau << '\n';
  if (to_string(tau) != "0.623000 ms")
    return 1;

  // The cutoff 1/(2πτ) is 1/s, cast to a frequency (not an activity, Bq).
  // pi keeps π in the unit, so it rounds once, with the true π, onto 0.01 Hz.
  using hundredths = id<"[0..100'000] 1/100 round_half_away">;
  const auto f {1_q / (2_q * (pi * tau))};
  const auto fc{value_cast<si::hertz, hundredths>(kind_cast<isq::frequency>(f))};
  std::cout << "f_c    = " << fc << '\n'; // 255.47 Hz
  if (to_string(fc) != "255.47 Hz")
    return 1;
  return 0;
}
