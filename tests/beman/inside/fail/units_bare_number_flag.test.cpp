// EXPECT: si::metre, clamp
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A one-shot store of a number names its unit too: Q{v, si::metre, clamp}.
#include <beman/inside/units.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    quantity<si::metre, inside<{0, 100}>> d{150, clamp};
    (void)d;
}
