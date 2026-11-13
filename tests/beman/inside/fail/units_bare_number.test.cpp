// EXPECT: or just<42>
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A number alone names no unit: Q{42, si::metre} or just<42> * si::metre.
#include <beman/inside/units.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    quantity<si::metre, inside<{0, 100}>> d{42};
    (void)d;
}
