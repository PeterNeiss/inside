// EXPECT: a pi exponent past 127
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Two units with a factor of pi, each to the 127th power: the power of pi (254) does not fit
// their magnitude (which a conversion or magnitude_of forms).
#include <inside/units.hpp>

int main() {
    using namespace ins;
    constexpr auto u = pow<127>(si::degree) * pow<127>(si::arcminute);
    constexpr auto m = magnitude_of(decltype(u)::value);
    (void)m;
}
