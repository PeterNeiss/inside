// EXPECT: the argument's parts are numbers times blades
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A bivector in metres: e^x needs a dimensionless argument.
#include <beman/inside/units.hpp>
#include <beman/inside/units/geoalg.hpp>
#include <beman/inside/units/math.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    constexpr auto e1 = beman::inside::units::basis_vector<"e1", 1>;
    constexpr auto e2 = beman::inside::units::basis_vector<"e2", 1>;
    using I           = inside<{{-1, 1}, per<64>}, round_half_away>;
    using dom         = quantity<si::metre * e1 * e2, I>;
    using cod         = multivector<I, quantity<e1 * e2, I>>;
    (void)math::exp<cod>(dom{I{1}, si::metre * e1 * e2});
}
