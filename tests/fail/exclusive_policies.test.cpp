// EXPECT: clamp and wrap are mutually exclusive
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Contradictory out-of-range policies on one inside are rejected in the class
// body's mutual-exclusion static_asserts.
#include <inside/inside.hpp>

int main() {
    ins::id<ins::key{ins::grid{0, 100}, ins::clamp | ins::wrap}> contradictory{};
    (void)contradictory;
}
