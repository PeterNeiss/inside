// EXPECT: the library bounds no range of f over these input grids
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// pow over a base grid that reaches 0 has no bounded range: a checked named
// output can be neither proven nor checked, so it names clamp or wrap.
#include <inside/math.hpp>

using namespace ins;

int main() {
    const id<"[0..4] 1/4"> b{2};
    const id<"[0..2] 1/2"> e{1};
    (void)math::pow<id<"[0..16] 1/16 round_half_away">>(b, e);
}
