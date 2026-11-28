// EXPECT: modulo requires notched grids
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// `%` works on any two lattices (integer or fractional notches); a continuous
// grid has no lattice for the remainder to lie on.
#include <inside/inside.hpp>

using namespace ins;

int main() {
    using ratio = id<"[0..8] <100>">;
    id<"[1..4]"> divisor{2};
    auto           rem = ratio{detail::rational{5, 2}} % divisor;
    (void)rem;
}
