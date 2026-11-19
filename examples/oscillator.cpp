// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A quadrature oscillator: a 16-bit phase counter that wraps at a full turn,
// in slots of 2π/65536 rad, a unit of our own.
// A unit with a factor of π reduces exactly, so sin and cos round once onto 2^-14, no 2π approximated.

#include <iostream>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units/io.hpp>
#include <inside/units/math.hpp>

using namespace ins;

namespace
{
  inline constexpr auto slot = unit<"slot", just<1, 32768> * pi * si::radian>; // 65536 slots are a full turn
} // namespace

int main()
{
  using phase_slots = quantity<slot, inside<{0, 65535}, wrap>>; // wrap is the periodicity

  // A fixed frequency, 1/8 turn per sample: on_wrap counts the completed cycles.
  std::cout << "Fixed-frequency oscillator (1/8 turn per sample, 8 samples = 1 cycle):\n";
  std::cout << "  i   sin              cos\n";
  {
    phase_slots           phase    {0, slot};
    constexpr phase_slots increment{65536 / 8, slot};
    int                   cycles   {0};
    for (int i = 0; i < 8; ++i)
    {
      std::cout << "  " << i << "   " << math::sin<per<(1 << 14)>>(phase) << "   "
                << math::cos<per<(1 << 14)>>(phase) << "\n";
      phase.on_wrap([&](auto&, auto) { ++cycles; }) += increment;
    }
    std::cout << "  completed cycles: " << cycles << "\n";
    if (cycles != 1 || phase != just<0> * slot)
      return 1;
    // 1/8 turn is 45°: sin and cos both round to 11585/16384.
    if (math::sin<per<(1 << 14)>>(increment) != just<11585, (1 << 14)>
        || math::cos<per<(1 << 14)>>(phase_slots{65536 / 4, slot}) != 0)
      return 1;
  }

  // A swept frequency, the increment growing each step, as in a chirp generator or a vibrato LFO.
  std::cout << "\nSwept-frequency oscillator (increment grows each step):\n";
  std::cout << "  i   inc        phase        sin\n";
  {
    // increment / 4 truncates; clamp keeps the growth within 65535
    // (×1.25 twelve times from 256 reaches 3725, which the grid cannot show).
    using increment_slots = quantity<slot, inside<{1, 65535}, round_toward_zero | clamp>>;
    phase_slots     phase    {0  , slot};
    increment_slots increment{256, slot}; // 1/256 turn
    for (int i = 0; i < 12; ++i)
    {
      std::cout << "  " << i << "   " << increment << "    " << phase << "    "
                << math::sin<per<(1 << 14)>>(phase) << "\n";
      phase     += increment;           // wraps at a full turn
      increment += increment / just<4>; // ×1.25
    }
  }
  return 0;
}
