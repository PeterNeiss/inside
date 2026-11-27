// EXPECT: expected a number at 4
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// An id's text names where it stops making sense and what it expected there.
#include <inside/inside.hpp>

int main() {
    ins::id<"[0..10x"> x;
    (void)x;
}
