// EXPECT: lies entirely outside this grid
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A literal's value is stored as its type stores it: 200 is past [0..99], and the type does not clamp.
#include <inside/inside.hpp>

int main() {
    using namespace ins;
    auto x{"200 [0..99]"_ins};
    (void)x;
}
