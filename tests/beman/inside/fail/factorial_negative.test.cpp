// EXPECT: the counts are insides on the integers from 0
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// factorial counts: its input grid starts at 0 or above.
#include <beman/inside/inside.hpp>
#include <beman/inside/math.hpp>

int main() {
    using namespace beman::inside;
    (void)math::factorial(inside<{-1, 5}>{});
}
