// EXPECT: "1/0"_q: expected a denominator above 0 at 2
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A fraction literal's denominator is above 0: "1/0"_q names no point.
#include <inside/inside.hpp>
using namespace ins::literals;

int main() {
    auto x = "1/0"_q;
    (void)x;
}
