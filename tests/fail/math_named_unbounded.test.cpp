// EXPECT: the library bounds no range of f over these input grids
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// pow over a base grid that reaches 0 has no bounded range: a checked named
// output can be neither proven nor checked, so it names clamp or wrap.
#include <inside/math.hpp>

using namespace ins;

int main() {
    const inside<{{0, 4}, per<4>}> b{2};
    const inside<{{0, 2}, per<2>}> e{1};
    (void)math::pow<inside<{{0, 16}, per<16>}, round_half_away>>(b, e);
}
