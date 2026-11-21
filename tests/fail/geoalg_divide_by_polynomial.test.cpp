// EXPECT: the divisor is a polynomial
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A polynomial has no inverse: divmod divides with a remainder.
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>

inline constexpr auto x = ins::basis_vector<"x", ins::polynomial>;

int main() {
    auto r = ins::just<1> / (ins::just<1> + ins::just<1> * x);
    (void)r;
}
