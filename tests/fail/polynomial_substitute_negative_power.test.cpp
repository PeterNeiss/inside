// EXPECT: negative power of the variable
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// substitute takes x to the powers 0 and up: 1/x at 0 has no value.
#include <inside/units.hpp>
#include <inside/units/polynomial.hpp>

inline constexpr auto x = ins::basis_vector<"x", ins::polynomial>;

int main() {
    auto v = ins::substitute(ins::just<1> + ins::just<1> / (ins::just<1> * x), x, ins::just<2>);
    (void)v;
}
