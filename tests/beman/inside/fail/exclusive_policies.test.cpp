// EXPECT: clamp and wrap are mutually exclusive
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Contradictory out-of-range policies on one inside are rejected in the class
// body's mutual-exclusion static_asserts.
#include <beman/inside/inside.hpp>

int main() {
    beman::inside::inside<{0, 100}, beman::inside::clamp | beman::inside::wrap> contradictory{};
    (void)contradictory;
}
