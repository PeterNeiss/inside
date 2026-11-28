// EXPECT: multiplies by a power of pi
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Degrees to radians has no exact grid: value_cast rounds it once.
#include <inside/units.hpp>

int main() {
    using namespace ins;
    id<"[0..10] [rad]"> q = 1_q * si::degree;
    (void)q;
}
