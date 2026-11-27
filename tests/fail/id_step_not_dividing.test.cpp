// EXPECT: expected a step that divides upper - lower at 8
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A step must divide the interval as written.
#include <inside/inside.hpp>

int main() {
    ins::id<"[0..10] 3"> x;
    (void)x;
}
