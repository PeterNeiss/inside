// EXPECT: of different dimensions
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A base dimension of your own is a dimension like the SI ones: money does
// not add to a length.
#include <beman/inside/units.hpp>

inline constexpr auto currency_dim = beman::inside::units::base_dimension<"currency">;
inline constexpr auto money_kind   = beman::inside::units::kind<"money", currency_dim>;
inline constexpr auto euro         = beman::inside::units::unit<"EUR", money_kind>;

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto s = just<1> * euro + just<1> * si::metre;
    (void)s;
}
