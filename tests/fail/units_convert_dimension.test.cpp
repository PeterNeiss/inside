// EXPECT: a quantity of another dimension
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A duration does not convert to a length.
#include <inside/units.hpp>

int main() {
    using namespace ins;
    id<"[0..10] [m]"> q = just<1> * si::second;
    (void)q;
}
