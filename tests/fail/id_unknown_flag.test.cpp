// EXPECT: expected a flag: clamp, wrap
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A flag is one of the policy words.
#include <inside/inside.hpp>

int main() {
    ins::id<"[0..9] clampy"> x;
    (void)x;
}
