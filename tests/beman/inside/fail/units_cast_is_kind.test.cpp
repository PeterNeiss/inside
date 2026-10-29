// EXPECT: across an is_kind boundary
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// quantity_cast cannot cross an is_kind boundary (rad → a dimensionless 1).
#include <beman/inside/units.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto q = quantity_cast<isq::dimensionless>(just<1> * si::radian);
    (void)q;
}
