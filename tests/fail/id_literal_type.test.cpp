// EXPECT: expected a number at 4
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A value literal's type reads as an id's text.
#include <inside/inside.hpp>

int main() {
    using namespace ins;
    auto x{"5 [0..9x"_ins};
    (void)x;
}
