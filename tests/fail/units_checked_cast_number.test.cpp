// EXPECT: write exact_cast<Q>
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// exact_cast into a quantity takes a quantity.
#include <inside/units.hpp>

int main() {
    using namespace ins;
    (void)exact_cast<quantity<si::metre, inside<{0, 100}>>>(42);
}
