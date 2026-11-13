// EXPECT: CoDomain must hold the scalar part and every blade of the argument
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// exp of a complex number onto its bivector part alone: the scalar part e^a·cos b
// has nowhere to go.
#include <beman/inside/units.hpp>
#include <beman/inside/units/geoalg.hpp>
#include <beman/inside/units/math.hpp>

inline constexpr beman::inside::units::basis_vector e1_def{"e1", 1};
inline constexpr beman::inside::units::basis_vector e2_def{"e2", 1};

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    constexpr auto e1 = unit_c<e1_def>;
    constexpr auto e2 = unit_c<e2_def>;
    using I           = inside<{{-4, 4}, per<64>}, round_nearest>;
    using dom         = multivector<I, quantity<e1 * e2, I>>;
    using cod         = quantity<e1 * e2, I>;
    (void)math::exp<cod>(dom{I{1} + I{1} * (e1 * e2)});
}
