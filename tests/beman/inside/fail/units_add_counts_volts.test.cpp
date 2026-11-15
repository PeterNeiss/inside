// EXPECT: of different dimensions
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// ADC counts as a base dimension of their own: a reading never adds to a
// voltage.
#include <beman/inside/units.hpp>

inline constexpr auto count_dim       = beman::inside::units::base_dimension<"count">;
inline constexpr auto adc_counts_kind = beman::inside::units::kind<"adc_counts", count_dim>;
inline constexpr auto cts_unit        = beman::inside::units::unit<"cts", adc_counts_kind>;

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto s = just<1> * cts_unit + just<1> * si::volt;
    (void)s;
}
