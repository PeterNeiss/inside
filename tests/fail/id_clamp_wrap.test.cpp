// EXPECT: expected clamp or wrap, not both
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// clamp and wrap exclude each other.
#include <inside/inside.hpp>

int main() {
    ins::id<"[0..9] clamp|wrap"> x;
    (void)x;
}
