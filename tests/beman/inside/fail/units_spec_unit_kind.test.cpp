// EXPECT: measures another kind
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A unit of one kind (Hz: frequency) for another spec (activity).
#include <beman/inside/units.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    auto r = isq::activity[si::hertz];
    (void)r;
}
