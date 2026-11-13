// EXPECT: may fall outside this grid
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A compound assignment the grids cannot prove names its channel: x + 1 may
// pass 100, so `x += 1_ins` on a checked inside<{0, 100}> does not compile.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    inside<{0, 100}> x{50};
    x += 1_ins;
}
