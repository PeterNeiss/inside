// EXPECT: write Q::try_make
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// try_make of a number names its unit: Q::try_make(v, si::metre).
#include <inside/units.hpp>

int main() {
    using namespace ins;
    (void)id<"[0..100] [m]">::try_make(42);
}
