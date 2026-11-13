// EXPECT: add to a multivector - include
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Quantities of different blades add to a multivector, which needs
// units/geoalg.hpp; the message names it.
#include <beman/inside/units.hpp>

inline constexpr beman::inside::units::basis_vector e1_def{"e1", 1};

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto z = just<3> + just<4> * unit_c<e1_def>;
    (void)z;
}
