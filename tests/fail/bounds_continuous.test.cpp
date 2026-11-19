// EXPECT: an open limit needs a notch
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An open grid limit moves in by one notch: a continuous grid has none.
#include <inside/inside.hpp>

int main() {
    using namespace ins;
    inside<{{0, 1}, 0, bounds::half_open}> x{0};
    (void)x;
}
