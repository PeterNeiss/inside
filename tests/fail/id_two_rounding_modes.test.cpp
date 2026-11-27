// EXPECT: expected one rounding mode
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// One rounding mode per type.
#include <inside/inside.hpp>

int main() {
    ins::id<"[0..9] round_floor|round_ceil"> x;
    (void)x;
}
