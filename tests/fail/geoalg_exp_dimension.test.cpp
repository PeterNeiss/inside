// EXPECT: the argument's parts are numbers times blades
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A bivector in metres: e^x needs a dimensionless argument.
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>
#include <inside/units/math.hpp>

inline constexpr auto e1 = ins::basis_vector<"e1", ins::geometric<1>>;
inline constexpr auto e2 = ins::basis_vector<"e2", ins::geometric<1>>;

int main() {
    using namespace ins;
    using I   = inside<{{-1, 1}, per<64>}, round_half_away>;
    using dom = quantity<si::metre * e1 * e2, I>;
    using cod = multivector<I, quantity<e1 * e2, I>>;
    (void)math::exp<cod>(dom{I{1}, si::metre * e1 * e2});
}
