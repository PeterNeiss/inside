// EXPECT: a point from a number takes the point's own unit
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// 20 in kelvin on a ℃ point names no origin: 20 K, or 20 K from the ice point?
#include <inside/units.hpp>

int main() {
    using namespace ins;
    quantity_point<si::degree_Celsius, si::ice_point, inside<{-40, 60}>> t{20, si::kelvin};
    (void)t;
}
