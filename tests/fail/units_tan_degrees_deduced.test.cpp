// EXPECT: name the result
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// tan of a degree angle takes a named result: its range follows from the angle's grid.
#include <inside/units.hpp>
#include <inside/units/math.hpp>

int main() {
    using namespace ins;
    using deg = id<"[-80..80] 1/2 [deg]">;
    (void)math::tan<per<1000>>(deg{30, si::degree});
}
