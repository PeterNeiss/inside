// EXPECT: CoDomain must hold the scalar part and every blade of the argument
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// exp of a complex number onto its bivector part alone: the scalar part e^a·cos b
// has nowhere to go.
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>
#include <inside/units/math.hpp>

inline constexpr auto e1 = ins::basis_vector<"e1", ins::geometric<1>>;
inline constexpr auto e2 = ins::basis_vector<"e2", ins::geometric<1>>;

int main() {
    using namespace ins;
    using I   = inside<{{-4, 4}, per<64>}, round_half_away>;
    using dom = multivector<I, quantity<e1 * e2, I>>;
    using cod = quantity<e1 * e2, I>;
    (void)math::exp<cod>(dom{I{1} + I{1} * (e1 * e2)});
}
