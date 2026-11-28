// EXPECT: incompatible notches
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A literal's value is stored as its type stores it: 0.3 is off the quarters, and the type does not round.
#include <inside/inside.hpp>

int main() {
    using namespace ins;
    auto x{"0.3 [0..1] 1/4"_q};
    (void)x;
}
