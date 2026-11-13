// EXPECT: this unit carries a sign
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// e2 e1 is -(e1 e2): a quantity's unit has no sign, the number does.
#include <beman/inside/units.hpp>

inline constexpr beman::inside::units::basis_vector e1_def{"e1", 1};
inline constexpr beman::inside::units::basis_vector e2_def{"e2", 1};

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    quantity<unit_c<e2_def> * unit_c<e1_def>, inside<{0, 10}>> q{1};
    (void)q;
}
