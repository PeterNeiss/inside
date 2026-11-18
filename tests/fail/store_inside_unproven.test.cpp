// EXPECT: may fall outside this grid
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An inside whose grid is wider than the target's does not store implicitly.
#include <inside/inside.hpp>

using namespace ins;

int main() {
    const inside<{0, 200}> wide{75};
    inside<{0, 100}>       narrow{wide};
    (void)narrow;
}
