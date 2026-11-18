// EXPECT: the argument's parts are numbers times blades
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A bivector in metres: e^x needs a dimensionless argument.
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>
#include <inside/units/math.hpp>

int main() {
    using namespace ins;
    constexpr auto e1 = ins::basis_vector<"e1", 1>;
    constexpr auto e2 = ins::basis_vector<"e2", 1>;
    using I           = inside<{{-1, 1}, per<64>}, round_half_away>;
    using dom         = quantity<si::metre * e1 * e2, I>;
    using cod         = multivector<I, quantity<e1 * e2, I>>;
    (void)math::exp<cod>(dom{I{1}, si::metre * e1 * e2});
}
