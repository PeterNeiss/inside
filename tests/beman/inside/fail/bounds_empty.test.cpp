// EXPECT: no grid point lies between the limits
// REQUIRES: gnu-message
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An open grid needs a point strictly between its limits.
#include <beman/inside/inside.hpp>

int main() {
    using namespace beman::inside;
    inside<{0, 1, bounds::open}> x{0};
    (void)x;
}
