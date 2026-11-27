// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An ADC and its calibration, with counts as a dimension of their own:
// a reading never adds to a voltage, and a gain in cts/V is not a 1/V.

#include <concepts>
#include <iostream>

#include <inside/inside.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;

namespace
{
  inline constexpr auto cts = base_unit<"cts", "count">;

  template <class A, class B>
  concept addable = requires(const A& a, const B& b) { a + b; };
} // namespace

int main()
{
  // 1. A fixed calibration from the data sheet:
  // (2048 − 12) / 1240.909 V = 1.64073… V, rounded once to 1641 mV.
  using adc_counts    = id<"[0..4095] [cts]">;
  using offset_counts = id<"[-64..64] [cts]">;
  using cts_per_volt  = id<"[1000..1500] 1/1000 [cts/V] round_half_away">;
  using millivolts    = id<"[-5000..5000] [mV] round_half_away">;

  const adc_counts    raw   {2048, cts};
  const offset_counts offset{12  , cts};
  const cts_per_volt  gain  {just<1240909, 1000> * (cts / si::volt)};
  const auto          volts {(raw - offset) / gain};
  const auto          mv    {value_cast<millivolts>(volts)};

  std::cout << "fixed      " << raw << " at " << gain << " = " << mv << '\n';
  if (to_string(mv) != "1641 mV")
    return 1;

  // 2. A two-point calibration: 0.5 V reads 632 cts, 2.5 V reads 3114 cts,
  // so the gain is 1241 cts/V and the offset 11.5 cts.
  // The references lie in disjoint ranges, so ΔV is never 0.
  using low_volts  = id<"[0..1] 1/1000 [V]">;
  using high_volts = id<"[2..5] 1/1000 [V]">;

  const low_volts  v1{just<1, 2> * si::volt};
  const high_volts v2{just<5, 2> * si::volt};
  const adc_counts c1{632 , cts};
  const adc_counts c2{3114, cts};

  const auto gain2{value_cast<cts_per_volt>((c2 - c1) / (v2 - v1))};
  if (!gain2)
    return 1; // past [1000, 1500] cts/V (equal readings give 0): a broken calibration

  const auto offset2{c1 - *gain2 * v1};
  const auto mv2    {value_cast<millivolts>((raw - offset2) / *gain2)}; // may miss millivolts' range
  if (!mv2)
    return 1;

  std::cout << "two-point  gain " << *gain2
            << ", offset " << offset2 << ", " << raw << " = " << *mv2 << '\n';
  if (to_string(offset2) != "11.500000 cts" || to_string(*mv2) != "1641 mV")
    return 1;

  // Counts and volts do not add, and a voltage is no current.
  static_assert(!addable<adc_counts, low_volts>);
  static_assert(!std::constructible_from<id<"[0..5] [A]">, decltype(volts)>);
  return 0;
}
