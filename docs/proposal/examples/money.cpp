// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour 2: an exact decimal notch, and rounding named in the type.
#include <inside/inside.hpp>

#include <type_traits>

using namespace ins;

int main() {
    // [listing]
    using cents = id<"[0..1'000'000] 1/100 round_half_away">;

    cents price = 0.1_q, tax = 0.2_q;       // exactly 1/10 and 2/10
    auto  total = price + tax;              // exactly 3/10 — no 0.30000000000000004
    static_assert(notch_of<decltype(total)> == per<100>);

    cents share = total / 3_q;            // 1/10 exactly
    cents third = cents{1} / 3_q;         // 1/3 rounds to nearest: 0.33
    // [/listing]
    if (total != 0.3_q || share != 0.1_q || third != 0.33_q)
        return 1;
    return 0;
}
