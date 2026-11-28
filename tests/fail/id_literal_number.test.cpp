// EXPECT: _q: expected a number at 0
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A value literal starts with its number.
#include <inside/inside.hpp>

int main() {
    using namespace ins;
    auto x{"x [0..9]"_q};
    (void)x;
}
