// EXPECT: CoDomain must hold the scalar part and every blade of the argument
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// exp of a complex number onto its bivector part alone: the scalar part e^a·cos b
// has nowhere to go.
#include <beman/inside/units.hpp>
#include <beman/inside/units/geoalg.hpp>
#include <beman/inside/units/math.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    constexpr auto e1 = beman::inside::units::basis_vector<"e1", 1>;
    constexpr auto e2 = beman::inside::units::basis_vector<"e2", 1>;
    using I           = inside<{{-4, 4}, per<64>}, round_half_away>;
    using dom         = multivector<I, quantity<e1 * e2, I>>;
    using cod         = quantity<e1 * e2, I>;
    (void)math::exp<cod>(dom{I{1} + I{1} * (e1 * e2)});
}
