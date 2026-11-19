// EXPECT: a constant is irrational
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A constant has no exact value on a grid: storing it needs a rounding mode.
#include <inside/math/constants.hpp>

int main() {
    using namespace ins;
    inside<{{0, 4}, per<1024>}> x = math::pi;
    (void)x;
}
