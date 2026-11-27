// EXPECT: name the result's notch
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// sin of an angle in degrees is reduced exactly by quarter turns, but its
// notch says nothing about the result's: the call names one, sin<per<K>>(q).
#include <inside/units/math.hpp>

int main() {
    using namespace ins;
    auto s = math::sin(id<"[0..90] [deg] round_half_away">{30, si::degree});
    (void)s;
}
