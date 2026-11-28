// EXPECT: the divisor is a polynomial
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A polynomial has no inverse: divmod divides with a remainder.
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>
using namespace ins::literals;

inline constexpr auto x = ins::basis_vector<"x", ins::polynomial>;

int main() {
    auto r = 1_q / (1_q + 1_q * x);
    (void)r;
}
