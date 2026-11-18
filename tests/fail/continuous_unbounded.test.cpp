// EXPECT: continuous grid .notch 0. needs a denominator bound
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A continuous grid names how far its fractions go: max_denominator.
#include <inside/inside.hpp>

using namespace ins;

int main() {
    inside<{{0, 10}, 0}> x{1};
    (void)x;
}
