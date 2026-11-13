// EXPECT: write Q::try_make
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// try_make of a number names its unit: Q::try_make(v, si::metre).
#include <beman/inside/units.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    (void)quantity<si::metre, inside<{0, 100}>>::try_make(42);
}
