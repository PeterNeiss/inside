// EXPECT: this unit carries a sign
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// e2 e1 is -(e1 e2): a quantity's unit has no sign, the number does.
#include <beman/inside/units.hpp>

inline constexpr auto e1_basis_vector = beman::inside::units::basis_vector<"e1", 1>;
inline constexpr auto e2_basis_vector = beman::inside::units::basis_vector<"e2", 1>;

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    quantity<e2_basis_vector * e1_basis_vector, inside<{0, 10}>> q{};
    (void)q;
}
