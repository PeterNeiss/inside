// EXPECT: take an angle in radians
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// tan of an angle in degrees: convert it to radians first.
#include <beman/inside/units/math.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto t = math::tan<per<1024>>(quantity<si::degree, inside<{0, 60}, round_nearest>>{30});
    (void)t;
}
