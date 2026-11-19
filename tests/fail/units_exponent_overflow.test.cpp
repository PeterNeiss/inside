// EXPECT: a dimension exponent past 127
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A unit to the 200th power: its dimension exponent does not fit.
#include <inside/units.hpp>

int main() {
    using namespace ins;
    constexpr auto u = pow<200>(si::metre);
    (void)u;
}
