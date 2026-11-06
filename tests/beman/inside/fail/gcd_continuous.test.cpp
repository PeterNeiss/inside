// EXPECT: the operands need a lattice
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// gcd and lcm take two lattices (integer or fractional notches, or points); a
// continuous grid has no lattice for a common divisor to lie on.
#include <beman/inside/inside.hpp>
#include <beman/inside/math.hpp>

int main() {
    using namespace beman::inside;
    (void)math::gcd(inside<{{0, 1}, max_denominator{10}}>{}, inside<{0, 5}>{});
}
