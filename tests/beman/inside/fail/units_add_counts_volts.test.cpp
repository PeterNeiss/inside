// EXPECT: of different dimensions
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// ADC counts as a base dimension of their own: a reading never adds to a
// voltage.
#include <beman/inside/units.hpp>

inline constexpr auto count_def      = beman::inside::units::base_dimension<"count">;
inline constexpr auto adc_counts_def = beman::inside::units::kind<"adc_counts", count_def>;
inline constexpr auto cts_def        = beman::inside::units::unit<"cts", adc_counts_def>;

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto s = just<1> * cts_def + just<1> * si::volt;
    (void)s;
}
