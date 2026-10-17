// EXPECT: of different dimensions
// REQUIRES: delete-reason
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A base dimension of your own is a dimension like the SI ones: money does
// not add to a length.
#include <beman/inside/units.hpp>

inline constexpr beman::inside::units::base_dimension currency_def{"currency"};
inline constexpr beman::inside::units::quantity_spec  money_def{"money", currency_def};
inline constexpr beman::inside::units::named_unit     euro_def{"EUR", beman::inside::units::spec_c<money_def>};

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto s = just<1> * unit_c<euro_def> + just<1> * si::metre;
    (void)s;
}
