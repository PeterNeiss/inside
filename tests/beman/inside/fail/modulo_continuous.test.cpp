// EXPECT: modulo requires notched grids
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// `%` works on any two lattices (integer or fractional notches); a continuous
// grid has no lattice for the remainder to lie on.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    using ratio = inside<{{0, 8}, max_denominator{100}}>;
    inside<{1, 4}> divisor{2};
    auto           rem = ratio{rational{5, 2}} % divisor;
    (void)rem;
}
