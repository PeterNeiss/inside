// EXPECT: is a unit of another quantity
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A number in seconds is no length.
#include <beman/inside/units.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    quantity<si::metre, inside<{0, 100}>> d{42, si::second};
    (void)d;
}
