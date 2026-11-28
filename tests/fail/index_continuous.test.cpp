// EXPECT: a continuous grid has none to count
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// index() counts notches.
#include <inside/inside.hpp>

int main() {
    using namespace ins;
    const id<"[0..1] <10>"> x = 0;
    (void)x.step();
}
