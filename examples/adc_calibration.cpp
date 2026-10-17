// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An ADC and its calibration, with counts as a dimension of their own.
//   1. Counts get a base dimension, so a reading in cts never adds to a
//      voltage, and a gain in cts/V is not a 1/V.
//   2. A fixed calibration: (raw − offset) / gain is a voltage, exactly; one
//      value_cast rounds it onto millivolts.
//   3. A two-point calibration: from two reference voltages and the counts
//      they read, gain = Δcts / ΔV and offset = cts₁ − gain·V₁. A gain
//      outside the plausible range is an error, caught once.

#include <concepts>
#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/units.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
// 1. The count dimension, its base quantity and its unit.
inline constexpr base_dimension count_def{"count"};
inline constexpr quantity_spec  adc_counts_def{"adc_counts", count_def};
inline constexpr auto           adc_counts = spec_c<adc_counts_def>;
inline constexpr named_unit     cts_def{"cts", adc_counts};
inline constexpr auto           cts = unit_c<cts_def>;

using reading    = quantity<cts, inside<{0, 4095}>>; // a 12-bit ADC
using gain_t     = quantity<cts / si::volt, inside<{{1000, 1500}, per<1000>}, round_nearest>>;
using millivolts = quantity<si::milli(si::volt), inside<{-5000, 5000}, round_nearest>>;

template <class A, class B>
concept addable = requires(const A& a, const B& b) { a + b; };
} // namespace

int main() {
    const reading raw{2048};

    // 2. Offset 12 cts and gain 1240.909 cts/V, from a data sheet: 2048 cts
    //    read 2036 / 1240.909 V = 1.64073… V, which is 1641 mV.
    const quantity<cts, inside<{-64, 64}>> offset{12};
    const gain_t                           gain{rational{1240909, 1000}};
    const auto                             volts = (raw - offset) / gain;
    const auto                             mv    = value_cast<millivolts>(volts);
    std::cout << "fixed      " << raw << " at " << gain << " = " << mv << '\n';
    if (to_string(mv) != "1641 mV")
        return 1;

    // 3. 0.5 V reads 632 cts and 2.5 V reads 3114 cts: the gain is
    //    2482 cts / 2 V = 1241 cts/V, the offset 632 − 1241 · 0.5 = 11.5 cts.
    //    The two references lie in disjoint ranges, so ΔV is never 0 and the
    //    gain is a plain quantity, not an expected.
    const quantity<si::volt, inside<{{0, 1}, per<1000>}>> v1{rational{1, 2}};
    const quantity<si::volt, inside<{{2, 5}, per<1000>}>> v2{rational{5, 2}};
    const reading                                         c1{632};
    const reading                                         c2{3114};
    //    Equal readings would give a gain of 0, so the exact Δcts / ΔV is
    //    checked into gain_t's range once; after that, dividing by the gain
    //    cannot fail.
    const auto gain2 = try_value_cast<gain_t>((c2 - c1) / (v2 - v1));
    if (!gain2)
        return 1; // a broken calibration
    const auto offset2 = c1 - *gain2 * v1;
    const auto mv2     = value_cast<millivolts>((raw - offset2) / *gain2);
    std::cout << "two-point  gain " << *gain2 << ", offset " << offset2 << ", " << raw << " = " << mv2 << '\n';
    if (to_string(offset2) != "11.500000 cts" || to_string(mv2) != "1641 mV")
        return 1;

    // Counts and volts do not add, and a voltage is no current.
    static_assert(!addable<reading, decltype(v1)>);
    static_assert(!std::constructible_from<quantity<si::ampere, inside<{0, 5}>>, decltype(volts)>);
    return 0;
}
