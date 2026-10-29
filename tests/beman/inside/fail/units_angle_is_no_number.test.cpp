// EXPECT: is no plain number
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A dimensionless quantity converts to an inside, but an angle (is_kind) does not.
#include <beman/inside/units.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    const inside<{0, 7}> x = just<1> * si::radian;
    (void)x;
}
