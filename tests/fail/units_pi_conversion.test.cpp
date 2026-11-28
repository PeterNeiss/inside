// EXPECT: multiplies by a power of pi
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// degree → radian has no exact grid: .in() refuses it, value_cast rounds.
#include <inside/units.hpp>

int main() {
    using namespace ins;
    auto r = (90_q * si::degree).in(si::radian);
    (void)r;
}
