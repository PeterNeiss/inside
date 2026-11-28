// EXPECT: is no plain number
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A dimensionless quantity converts to an inside, but an angle (distinct) does not.
#include <inside/units.hpp>

int main() {
    using namespace ins;
    const id<"[0..7]"> x = 1_q * si::radian;
    (void)x;
}
