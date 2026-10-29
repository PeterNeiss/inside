// EXPECT: a raw number has no grid
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A raw scalar has no grid: 42 * m names just<42> instead.
#include <beman/inside/units.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto q = 42 * si::metre;
    (void)q;
}
