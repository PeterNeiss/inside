// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour 3: the policy decides what a narrowing store does.
#include <inside/inside.hpp>

using namespace ins;

int main() {
    // [listing]
    using degrees = id<"[0..359] wrap">;      // modular
    using level   = id<"[0..10] clamp">;      // saturating
    using count   = id<"[0..10]">;            // checked (the default)

    degrees heading = 350;
    heading += 20_q;                        // 370 wraps to 10
    level   volume  = 9;
    volume += 5_q;                          // 14 clamps to 10

    auto n = count::try_make(14);             // unexpected(error::overflow)
    // count m = 14;                          // ill-formed: 14 is outside [0, 10]
    // [/listing]
    if (heading != 10 || volume != 10 || n.has_value() || n.error() != error::overflow)
        return 1;
    return 0;
}
