// EXPECT: may fall off this quantity's grid
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// 1500 m is 1.5 km: off a grid of whole kilometres, so the store names a rounding.
#include <inside/units.hpp>

int main() {
    using namespace ins;
    quantity<si::kilo(si::metre), inside<{0, 10}>> q = just<1500> * si::metre;
    (void)q;
}
