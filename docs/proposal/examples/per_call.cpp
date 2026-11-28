// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour: a policy for one call, and actions that see what a policy did.
#include <inside/inside.hpp>

using namespace ins;

int main() {
    // [listing]
    using whole = id<"[0..100]">;
    using step  = id<"[1..10]">;                           // never zero

    auto down       = div(whole{7}, step{2}, round_floor); // 3: this call rounds down
    auto up         = div(whole{7}, step{2}, round_ceil);  // 4
    auto [quo, rem] = divmod(whole{17}, step{5});            // 3 and 2

    whole x = 50;
    x.with<clamp>() = 150;                                      // 100: this store saturates

    using seconds = id<"[0..59] wrap">;
    using minutes = id<"[0..59] clamp">; // m += carry names what happens past 59
    seconds s = 50;
    minutes m = 0;
    s.on_wrap([&](auto&, auto carry) { m += carry; }) += 75_ins; // 125 s: s == 5, m == 2
    // [/listing]
    if (down != 3 || up != 4 || quo != 3 || rem != 2 || x != 100)
        return 1;
    if (s != 5 || m != 2)
        return 1;
    return 0;
}
