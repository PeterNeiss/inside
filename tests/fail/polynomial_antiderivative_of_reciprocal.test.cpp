// EXPECT: is a logarithm
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The antiderivative of 1/x is log x, no polynomial.
#include <inside/units.hpp>
#include <inside/units/polynomial.hpp>
using namespace ins::literals;

inline constexpr auto x = ins::basis_vector<"x", ins::polynomial>;

int main() {
    auto f = ins::antiderivative(1_q / (1_q * x), x);
    (void)f;
}
