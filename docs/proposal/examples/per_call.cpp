// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour: a policy for one call, and actions that see what a policy did.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main() {
    // [listing]
    using whole = inside<{0, 100}>;
    using step  = inside<{1, 10}>;                           // never zero

    auto down       = div(whole{7}, step{2}, rounded_floor); // 3: this call rounds down
    auto up         = div(whole{7}, step{2}, rounded_ceil);  // 4
    auto [quo, rem] = divmod(whole{17}, step{5});            // 3 and 2

    whole x = 50;
    x.with_clamp() = 150;                                    // 100: this store saturates

    using seconds = inside<{0, 59}, wrap>;
    using minutes = inside<{0, 59}>;
    seconds s = 50;
    minutes m = 0;
    s.on_wrap([&](auto&, auto carry) { m += carry; }) += 75_ins; // 125 s: s == 5, m == 2
    // [/listing]
    if (down != just<3> || up != just<4> || quo != just<3> || rem != just<2> || x != just<100>)
        return 1;
    if (s != just<5> || m != just<2>)
        return 1;
    return 0;
}
