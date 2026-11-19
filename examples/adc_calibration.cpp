// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An ADC and its calibration, with counts as a dimension of their own: a reading never adds to a voltage,
// and a gain in cts/V is not a 1/V.

#include <concepts>
#include <iostream>

#include <inside/inside.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;

namespace {
inline constexpr auto cts = base_unit<"cts", "count">; // counts: a base dimension of their own

template <class A, class B>
concept addable = requires(const A& a, const B& b) { a + b; };
} // namespace

int main() {
    using adc_counts = quantity<cts, inside<{0, 4095}>>; // a 12-bit ADC
    const adc_counts raw{2048, cts};

    // 1. A fixed calibration from the data sheet: (2048 − 12) / 1240.909 V = 1.64073… V, rounded once to 1641 mV.
    using offset_counts = quantity<cts, inside<{-64, 64}>>;
    using cts_per_volt  = quantity<cts / si::volt, inside<{{1000, 1500}, per<1000>}, round_half_away>>;
    using millivolts    = quantity<si::milli(si::volt), inside<{-5000, 5000}, round_half_away>>;

    const offset_counts offset{12, cts};
    const cts_per_volt gain{just<1240909, 1000> * (cts / si::volt)};
    const auto volts = (raw - offset) / gain;
    const auto mv = value_cast<millivolts>(volts);
    std::cout << "fixed      " << raw << " at " << gain << " = " << mv << '\n';
    if (to_string(mv) != "1641 mV")
        return 1;

    // 2. A two-point calibration: 0.5 V reads 632 cts, 2.5 V reads 3114 cts, so the gain is 1241 cts/V and
    //    the offset 11.5 cts. The references lie in disjoint ranges, so ΔV is never 0.
    using low_volts  = quantity<si::volt, inside<{{0, 1}, per<1000>}>>;
    using high_volts = quantity<si::volt, inside<{{2, 5}, per<1000>}>>;

    const low_volts v1{just<1, 2> * si::volt};
    const high_volts v2{just<5, 2> * si::volt};
    const adc_counts c1{632, cts};
    const adc_counts c2{3114, cts};

    const auto gain2 = value_cast<cts_per_volt>((c2 - c1) / (v2 - v1)); // equal readings would give gain 0
    if (!gain2)
        return 1; // a broken calibration
    const auto offset2 = c1 - *gain2 * v1;
    const auto mv2 = value_cast<millivolts>((raw - offset2) / *gain2); // may miss millivolts' range
    if (!mv2)
        return 1;
    std::cout << "two-point  gain " << *gain2 << ", offset " << offset2 << ", " << raw << " = " << *mv2 << '\n';
    if (to_string(offset2) != "11.500000 cts" || to_string(*mv2) != "1641 mV")
        return 1;

    // Counts and volts do not add, and a voltage is no current.
    static_assert(!addable<adc_counts, low_volts>);
    static_assert(!std::constructible_from<quantity<si::ampere, inside<{0, 5}>>, decltype(volts)>);
    return 0;
}
