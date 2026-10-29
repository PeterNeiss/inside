// EXPECT: of different dimensions
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// ADC counts as a base dimension of their own: a reading never adds to a
// voltage.
#include <beman/inside/units.hpp>

inline constexpr beman::inside::units::base_dimension count_def{"count"};
inline constexpr beman::inside::units::quantity_spec  adc_counts_def{"adc_counts", count_def};
inline constexpr beman::inside::units::named_unit     cts_def{"cts", beman::inside::units::spec_c<adc_counts_def>};

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto s = just<1> * unit_c<cts_def> + just<1> * si::volt;
    (void)s;
}
