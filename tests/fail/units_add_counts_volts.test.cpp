// EXPECT: of different dimensions
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// ADC counts as a base dimension of their own: a reading never adds to a
// voltage.
#include <inside/units.hpp>

inline constexpr auto count_dim       = ins::base_dimension<"count">;
inline constexpr auto adc_counts_kind = ins::kind<"adc_counts", count_dim>;
inline constexpr auto cts_unit        = ins::unit<"cts", adc_counts_kind>;

int main() {
    using namespace ins;
    auto s = 1_q * cts_unit + 1_q * si::volt;
    (void)s;
}
