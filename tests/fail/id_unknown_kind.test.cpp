// EXPECT: expected a kind visible here
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A kind in an id's text is one visible where the type is named.
#include <inside/units.hpp>

int main() {
    ins::id<"[0..9] [m]{depthh}"> x;
    (void)x;
}
