// EXPECT: is a unit of another quantity
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A number in seconds is no length.
#include <inside/units.hpp>

int main() {
    using namespace ins;
    id<"[0..100] [m]"> d{42, si::second};
    (void)d;
}
