// EXPECT: si::metre, clamp
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A one-shot store of a number names its unit too: Q{v, si::metre, clamp}.
#include <inside/units.hpp>

int main() {
    using namespace ins;
    id<"[0..100] [m]"> d{150, clamp};
    (void)d;
}
