// EXPECT: a number alone has no unit - read q
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Reading the number names the unit: q.numerical_value_in(si::metre).
#include <inside/units.hpp>

int main() {
    using namespace ins;
    const auto d = 42_q * si::metre;
    (void)d.numerical_value();
}
