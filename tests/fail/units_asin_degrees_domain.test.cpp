// EXPECT: take a number in
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// asin into degrees takes a number in [-1, 1]: a grid past it does not compile.
#include <inside/units.hpp>
#include <inside/units/math.hpp>

int main() {
    using namespace ins;
    using ratio = id<"[-2..2] 1/64 [1]">;
    using deg   = id<"[-90..90] 1/1000 [deg] round_half_away">;
    (void)math::asin<deg>(ratio{1, one});
}
