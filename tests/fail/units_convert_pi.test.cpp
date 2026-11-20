// EXPECT: multiplies by a power of pi
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Degrees to radians has no exact grid: value_cast rounds it once.
#include <inside/units.hpp>

int main() {
    using namespace ins;
    quantity<si::radian, inside<{0, 10}>> q = just<1> * si::degree;
    (void)q;
}
