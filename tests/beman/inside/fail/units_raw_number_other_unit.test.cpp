// EXPECT: in another unit than Q's has no grid
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// 2 km into metres rescales the number: a raw 2 has no grid to rescale.
#include <beman/inside/units.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    quantity<si::metre, inside<{0, 5000}>> d{2, si::kilo(si::metre)};
    (void)d;
}
