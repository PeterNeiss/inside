// EXPECT: write checked_cast<Q>
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// checked_cast into a quantity takes a quantity.
#include <beman/inside/units.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    (void)checked_cast<quantity<si::metre, inside<{0, 100}>>>(42);
}
