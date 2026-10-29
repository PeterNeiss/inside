// EXPECT: a raw number has no grid
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Scaling a quantity by a raw scalar: q * 2 names just<2> instead.
#include <beman/inside/units.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto q = (just<1> * si::metre) * 2;
    (void)q;
}
