// EXPECT: only a continuous grid carries a max_denominator
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A point has one value: no denominator bound.
#include <beman/inside/inside.hpp>

int main() {
    using namespace beman::inside;
    inside<grid{interval{1, 1}, max_denominator{10}}> x{1};
    (void)x;
}
