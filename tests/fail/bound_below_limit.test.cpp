// EXPECT: the bound is below a limit's denominator
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Each limit is a value of the grid, so the bound covers its denominator.
#include <inside/inside.hpp>

int main() {
    using namespace ins;
    id<key{grid{interval{0, just<1, 3>}, max_denominator{2}}}> x{0};
    (void)x;
}
