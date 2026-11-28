// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour 4: Lower need not be a multiple of the notch.
#include <inside/inside.hpp>

#include <type_traits>

using namespace ins;

int main() {
    // [listing]
    using centre = id<"[0.5..255.5] 1">;         // 0.5, 1.5, ..., 255.5
    static_assert(sizeof(centre) == 1);          // 256 points: one byte

    centre x = 2.5_q, y = 0.5_q;
    auto   s = x + y;                            // id<"[1..511]">: whole numbers again
    auto   p = x * y;                            // 0.25, 0.75, ...: Lower 1/4, notch 1/2
    static_assert(std::is_same_v<decltype(s), id<"[1..511]">>);
    static_assert(lower_of<decltype(p)> == per<4> && notch_of<decltype(p)> == per<2>);
    // [/listing]
    if (s != 3 || p != 1.25_q)
        return 1;
    return 0;
}
