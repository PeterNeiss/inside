// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour 3: the policy decides what a narrowing store does.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    // [listing]
    using degrees = inside<{0, 359}, wrap>;   // modular
    using level   = inside<{0, 10}, clamp>;   // saturating
    using count   = inside<{0, 10}>;          // checked (the default)

    degrees heading = 350;
    heading += 20_ins;                        // 370 wraps to 10
    level   volume  = 9;
    volume += 5_ins;                          // 14 clamps to 10

    auto n = count::try_make(14);             // unexpected(errc::overflow)
    // count m = 14;                          // a checked store: throws inside_error
    // [/listing]
    if (heading != just<10> || volume != just<10> || n.has_value() || n.error() != errc::overflow)
        return 1;
    return 0;
}
