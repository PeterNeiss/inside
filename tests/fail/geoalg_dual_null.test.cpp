// EXPECT: use complement
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The dual divides by the pseudoscalar, which a null vector makes 0: the
// projective algebra takes the complement instead.
#include <inside/units.hpp>
#include <inside/units/geoalg.hpp>

int main() {
    using namespace ins;
    constexpr auto p0 = ins::basis_vector<"p0", 0>;
    constexpr auto p1 = ins::basis_vector<"p1", 1>;
    auto           d  = dual(just<1> * p1, p0 * p1);
    (void)d;
}
