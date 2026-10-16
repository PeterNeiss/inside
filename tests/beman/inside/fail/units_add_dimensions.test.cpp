// EXPECT: of different dimensions
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A length and a duration do not add.
#include <beman/inside/units.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto s = just<1> * si::metre + just<1> * si::second;
    (void)s;
}
