// EXPECT: a quantity's type needs
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Without <inside/units.hpp> a unit has no meaning.
#include <inside/inside.hpp>

int main() {
    ins::id<"[0..9] [m]"> x;
    (void)x;
}
