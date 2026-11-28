// EXPECT: or just<42>
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A number alone names no unit: Q{42, si::metre} or just<42> * si::metre.
#include <inside/units.hpp>

int main() {
    using namespace ins;
    id<"[0..100] [m]"> d{42};
    (void)d;
}
