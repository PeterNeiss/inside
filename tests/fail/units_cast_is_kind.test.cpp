// EXPECT: across a distinct kind's boundary
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// kind_cast cannot cross an distinct boundary (rad → a dimensionless 1).
#include <inside/units.hpp>

int main() {
    using namespace ins;
    auto q = kind_cast<isq::dimensionless>(1_q * si::radian);
    (void)q;
}
