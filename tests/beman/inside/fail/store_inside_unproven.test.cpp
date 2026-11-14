// EXPECT: may fall outside this grid
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An inside whose grid is wider than the target's does not store implicitly.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    const inside<{0, 200}> wide{75};
    inside<{0, 100}>       narrow{wide};
    (void)narrow;
}
