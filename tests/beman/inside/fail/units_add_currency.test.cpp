// EXPECT: of different dimensions
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A base dimension of your own is a dimension like the SI ones: money does
// not add to a length.
#include <beman/inside/units.hpp>

inline constexpr auto currency_def = beman::inside::units::base_dimension<"currency">;
inline constexpr auto money_def    = beman::inside::units::kind<"money", currency_def>;
inline constexpr auto euro_def     = beman::inside::units::unit<"EUR", money_def>;

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto s = just<1> * euro_def + just<1> * si::metre;
    (void)s;
}
