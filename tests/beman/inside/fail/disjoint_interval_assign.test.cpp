// EXPECT: entirely outside lhs interval
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Assigning from an inside whose interval cannot overlap the target is rejected
// at compile time (inside_assignable_why names the failing clause).
#include <beman/inside/inside.hpp>

int main() {
    beman::inside::inside<{0, 10}>  small{5};
    beman::inside::inside<{20, 30}> big{25};
    small = big; // ill-formed: [20,30] never fits in [0,10]
}
