// EXPECT: _r literal
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// 0.1 is 3602879701896397/2^55 in binary, so deriving the notch from it would
// give a 2^-55 grid; grid{lo, hi} rejects it and points to 0.1_r.
#include <inside/inside.hpp>

using namespace ins;

int main() {
    inside<{0.1, 1}> x{1};
    (void)x;
}
