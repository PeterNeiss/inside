// EXPECT: a number alone has no unit - read q
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Reading the number names the unit: q.numerical_value_in(si::metre).
#include <beman/inside/units.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    const auto d = just<42> * si::metre;
    (void)d.numerical_value();
}
