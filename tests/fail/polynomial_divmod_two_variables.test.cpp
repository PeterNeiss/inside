// EXPECT: polynomials in one variable
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Division with a remainder is unique only in one variable.
#include <inside/units.hpp>
#include <inside/units/polynomial.hpp>

inline constexpr auto x = ins::basis_vector<"x", ins::polynomial>;
inline constexpr auto y = ins::basis_vector<"y", ins::polynomial>;

int main() {
    auto r = ins::divmod(ins::just<1> * x * y + ins::just<1>, ins::just<1> * x + ins::just<1>);
    (void)r;
}
