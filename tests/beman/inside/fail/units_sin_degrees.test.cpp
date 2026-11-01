// EXPECT: takes an angle in radians
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// sin of an angle in degrees: convert it to radians first.
#include <beman/inside/units/math.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto s = sin(quantity<si::degree, inside<{0, 90}, round_nearest>>{30});
    (void)s;
}
