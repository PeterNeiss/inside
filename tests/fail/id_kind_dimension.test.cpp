// EXPECT: expected a kind of the unit's dimension
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A kind has its unit's dimension.
#include <inside/units.hpp>

int main() {
    ins::id<"[0..9] [m]{mass}"> x;
    (void)x;
}
