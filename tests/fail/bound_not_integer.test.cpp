// EXPECT: the bound must be a positive integer
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A denominator bound is a positive integer.
#include <inside/inside.hpp>

int main() {
    using namespace ins;
    id<key{grid{interval{0, 1}, max_denominator{0.5_q}}}> x{0};
    (void)x;
}
