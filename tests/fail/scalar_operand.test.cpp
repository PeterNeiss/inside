// EXPECT: cannot be combined with a raw scalar
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Grid-less scalars are rejected with a fix-it (write `a + 1_q`), not a wall
// of overload-resolution noise — the deleted overload's reason names it.
#include <inside/inside.hpp>

int main() {
    ins::id<"[0..100]"> percent{50};
    auto                  sum = percent + 1; // ill-formed: scalar has no grid
    (void)sum;
}
