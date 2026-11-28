// EXPECT: of different kinds
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Hz and Bq are both 1/s, but separate kinds: they do not add.
#include <inside/units.hpp>

int main() {
    using namespace ins;
    auto s = 1_q * si::hertz + 1_q * si::becquerel;
    (void)s;
}
