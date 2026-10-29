// EXPECT: the unit has another dimension
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A length in seconds: in() takes a unit of the quantity's dimension.
#include <beman/inside/units.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto t = (just<1> * si::metre).in(si::second);
    (void)t;
}
