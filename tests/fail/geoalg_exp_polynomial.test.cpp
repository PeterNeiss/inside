// EXPECT: basis vectors are geometric ones
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The exponential of a polynomial variable is no polynomial.
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>
#include <inside/units/math.hpp>
using namespace ins::literals;

inline constexpr auto x = ins::basis_vector<"x", ins::polynomial>;

int main() {
    using angle = ins::id<"[-1..1] 1/1024">;
    auto e      = ins::math::exp<ins::per<1024>>(1_q + angle{0.5} * x);
    (void)e;
}
