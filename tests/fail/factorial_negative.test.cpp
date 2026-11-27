// EXPECT: the counts are insides on the integers from 0
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// factorial counts: its input grid starts at 0 or above.
#include <inside/inside.hpp>
#include <inside/math.hpp>

int main() {
    using namespace ins;
    (void)math::factorial(id<"[-1..5]">{});
}
