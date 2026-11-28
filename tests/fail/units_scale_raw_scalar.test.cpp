// EXPECT: a raw number has no grid
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Scaling a quantity by a raw scalar: q * 2 names 2_q instead.
#include <inside/units.hpp>

int main() {
    using namespace ins;
    auto q = (1_q * si::metre) * 2;
    (void)q;
}
