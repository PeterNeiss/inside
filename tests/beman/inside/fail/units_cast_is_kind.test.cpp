// EXPECT: across a distinct kind's boundary
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// kind_cast cannot cross an distinct boundary (rad → a dimensionless 1).
#include <beman/inside/units.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto q = kind_cast<isq::dimensionless>(just<1> * si::radian);
    (void)q;
}
