// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour: a function rounds once, onto the grid it returns.
#include <beman/inside/math.hpp>

#include <type_traits>

using namespace beman::inside;

int main() {
    // [listing]
    using angle = inside<{{-8, 8}, per<16384>}, round_nearest>;
    auto s = math::sin(angle{1});                    // 0.84149169921875, the nearest 1/16384
    static_assert(grid_of<decltype(s)> == grid{{-1, 1}, per<16384>});
    auto c = math::cos<per<256>>(angle{1});                  // 0.5390625: a coarser notch, named

    using level = inside<{{0, 4}, per<256>}, round_nearest>;
    auto r = math::sqrt(level{2.25});                // 1.5, on [0, 2]
    // math::sqrt(inside<{{-4, 4}, per<256>}, round_nearest>{2.25}) does not compile:
    // the grid reaches below 0, so take abs(x) or store x into a non-negative grid first
    // [/listing]
    if (s != 0.84149169921875_ins || c != 0.5390625_ins)
        return 1;
    if (r != 1.5_ins)
        return 1;
    return 0;
}
