// EXPECT: the unit has another dimension
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A length in seconds: in() takes a unit of the quantity's dimension.
#include <inside/units.hpp>

int main() {
    using namespace ins;
    auto t = (1_q * si::metre).in(si::second);
    (void)t;
}
