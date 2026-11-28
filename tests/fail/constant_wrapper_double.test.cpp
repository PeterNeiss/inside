// EXPECT: cannot be combined with a raw scalar
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A std::cw of a double is its binary value, not the decimal written (std::cw<0.1>): rejected as 0.1 is; 0.1_q is exact.
#include <inside/inside.hpp>

int main() {
    ins::id<"[0..9]"> x{3};
    auto              y = x * std::cw<0.5>;
    (void)y;
}
