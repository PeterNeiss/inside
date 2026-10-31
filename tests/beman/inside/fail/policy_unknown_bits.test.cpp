// EXPECT: the policy holds bits no flag names
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A policy is built from the flags; a raw bit pattern that no flag names is
// rejected instead of silently meaning nothing.
#include <beman/inside/inside.hpp>

int main() {
    beman::inside::inside<{0, 10}, 7> x{3};
    (void)x;
}
