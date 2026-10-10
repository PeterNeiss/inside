// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour 2: an exact decimal notch, and rounding named in the type.
#include <beman/inside/inside.hpp>

#include <type_traits>

using namespace beman::inside;

int main() {
    // [listing]
    using cents = inside<{{0, 1'000'000}, per<100>}, round_nearest>;

    cents price = 0.1_ins, tax = 0.2_ins;   // exactly 1/10 and 2/10
    auto  total = price + tax;              // exactly 3/10 — no 0.30000000000000004
    static_assert(notch_of<decltype(total)> == per<100>);

    cents share = total / 3_ins;            // 1/10 exactly
    cents third = cents{1} / 3_ins;         // 1/3 rounds to nearest: 0.33
    // [/listing]
    if (total != just<rational{3, 10}> || share != just<rational{1, 10}> || third != just<rational{33, 100}>)
        return 1;
    return 0;
}
