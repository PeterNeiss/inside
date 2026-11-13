// EXPECT: name the result's notch
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// sin of an angle in degrees is reduced exactly by quarter turns, but its
// notch says nothing about the result's: the call names one, sin<per<K>>(q).
#include <beman/inside/units/math.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto s = math::sin(quantity<si::degree, inside<{0, 90}, round_nearest>>{30});
    (void)s;
}
