// EXPECT: basis vectors are geometric ones
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// log and sqrt take geometric basis vectors: an inner vector has only its dot product.
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>
#include <inside/units/math.hpp>

inline constexpr auto b1 = ins::basis_vector<"b1", ins::inner>;

int main() {
    using namespace ins;
    using I   = inside<{{1, 4}, per<64>}, round_half_away>;
    using dom = multivector<I, quantity<b1, I>>;
    (void)math::sqrt(dom{I{2} + I{1} * b1});
}
