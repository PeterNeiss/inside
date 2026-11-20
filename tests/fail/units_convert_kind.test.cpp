// EXPECT: a quantity of another kind
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Becquerel and hertz are both 1/s, but activity is not frequency.
#include <inside/units.hpp>

int main() {
    using namespace ins;
    quantity<si::hertz, inside<{0, 10}>> q = just<1> * si::becquerel;
    (void)q;
}
