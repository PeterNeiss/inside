// EXPECT: the policy names two rounding modes
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// One rounding mode per type: two would leave the store to a hidden
// precedence.
#include <inside/inside.hpp>

using namespace ins;

int main() {
    inside<{0, 10}, round_floor | round_ceil> x{3};
    (void)x;
}
