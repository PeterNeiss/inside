// EXPECT: add to a multivector - include
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Quantities of different blades add to a multivector, which needs
// units/geoalg.hpp; the message names it.
#include <inside/units.hpp>

inline constexpr auto e1 = ins::basis_vector<"e1", ins::geometric<1>>;

int main() {
    using namespace ins;
    auto z = just<3> + just<4> * e1;
    (void)z;
}
