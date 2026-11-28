// EXPECT: of different dimensions
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A length and a duration do not add.
#include <inside/units.hpp>

int main() {
    using namespace ins;
    auto s = 1_q * si::metre + 1_q * si::second;
    (void)s;
}
