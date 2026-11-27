// EXPECT: expected an origin visible here
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A point's origin is one visible where the type is named.
#include <inside/units.hpp>

int main() {
    ins::id<"[0..9] [K] @nowhere"> x;
    (void)x;
}
