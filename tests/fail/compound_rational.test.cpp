// EXPECT: cannot be combined with a rational
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A rational has no grid, like an int or a double: a compound assign takes an
// inside (0.5_ins, just<1, 2>), as the binary operators do.
#include <inside/inside.hpp>

using namespace ins;

using ins::detail::operator""_r; // the library's own literal

int main() {
    id<"[0..10] 1/4"> x{1};
    x += 0.5_r;
}
