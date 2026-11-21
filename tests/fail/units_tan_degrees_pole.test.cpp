// EXPECT: holds a pole
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// tan of a degree angle whose grid holds 90°: no value there.
#include <inside/units.hpp>
#include <inside/units/math.hpp>

int main() {
    using namespace ins;
    using deg = quantity<si::degree, inside<{{0, 180}, per<2>}>>;
    using out = inside<{{-1000, 1000}, per<1000>}, round_half_away>;
    (void)math::tan<out>(deg{30, si::degree});
}
