// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour 4: Lower need not be a multiple of the notch.
#include <beman/inside/inside.hpp>

#include <type_traits>

using namespace beman::inside;

int main() {
    // [listing]
    using centre = inside<{{0.5, 255.5}, 1}>;    // 0.5, 1.5, ..., 255.5
    static_assert(sizeof(centre) == 1);          // 256 points: one byte

    centre x = 2.5_ins, y = 0.5_ins;
    auto   s = x + y;                            // inside<{1, 511}>: whole numbers again
    auto   p = x * y;                            // 0.25, 0.75, ...: Lower 1/4, notch 1/2
    static_assert(std::is_same_v<decltype(s), inside<{1, 511}>>);
    static_assert(lower_of<decltype(p)> == per<4> && notch_of<decltype(p)> == per<2>);
    // [/listing]
    if (s != just<3> || p != just<rational{5, 4}>)
        return 1;
    return 0;
}
