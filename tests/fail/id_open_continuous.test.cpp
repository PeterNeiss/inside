// EXPECT: expected closed limits: a continuous grid has no next point
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A continuous grid has no next point to move an open limit in to.
#include <inside/inside.hpp>

int main() {
    ins::id<"[0..1) <1000>"> x;
    (void)x;
}
