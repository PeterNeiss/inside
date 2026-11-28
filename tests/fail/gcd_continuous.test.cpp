// EXPECT: the operands need a lattice
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// gcd and lcm take two lattices (integer or fractional notches, or points); a
// continuous grid has no lattice for a common divisor to lie on.
#include <inside/inside.hpp>
#include <inside/math.hpp>

int main() {
    using namespace ins;
    (void)math::gcd(id<"[0..1] <10>">{}, id<"[0..5]">{});
}
