// EXPECT: the operands are insides on the integers
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// gcd and lcm take integer grids.
#include <beman/inside/inside.hpp>
#include <beman/inside/math.hpp>

int main() {
    using namespace beman::inside;
    (void)math::gcd(inside<{{0, 1}, 0.5}>{}, inside<{0, 5}>{});
}
