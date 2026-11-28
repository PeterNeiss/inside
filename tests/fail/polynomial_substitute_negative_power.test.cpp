// EXPECT: negative power of the variable
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// substitute takes x to the powers 0 and up: 1/x at 0 has no value.
#include <inside/units.hpp>
#include <inside/units/polynomial.hpp>
using namespace ins::literals;

inline constexpr auto x = ins::basis_vector<"x", ins::polynomial>;

int main() {
    auto v = ins::substitute(1_q + 1_q / (1_q * x), x, 2_q);
    (void)v;
}
