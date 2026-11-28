// EXPECT: of different dimensions
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A base dimension of your own is a dimension like the SI ones: money does
// not add to a length.
#include <inside/units.hpp>

inline constexpr auto currency_dim = ins::base_dimension<"currency">;
inline constexpr auto money_kind   = ins::kind<"money", currency_dim>;
inline constexpr auto euro         = ins::unit<"EUR", money_kind>;

int main() {
    using namespace ins;
    auto s = 1_q * euro + 1_q * si::metre;
    (void)s;
}
