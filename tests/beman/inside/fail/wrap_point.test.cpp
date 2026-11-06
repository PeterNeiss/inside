// EXPECT: wrap onto a point grid
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// wrap folds by the period Upper − Lower + Notch, which is 0 on a point grid:
// a store that may leave the point is rejected at compile time instead of
// reporting overflow at run time.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    inside<{3}, wrap> x{5};
    (void)x;
}
