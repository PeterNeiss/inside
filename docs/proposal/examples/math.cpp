// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour 7: math rounds correctly onto the output grid.
#include <beman/inside/units_math.hpp>

#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::units;

int main() {
    // [listing]
    using angle = inside<{{-8, 8}, per<16384>}, round_nearest>;
    auto s = math::sin(angle{1});                    // 0.84149169921875, the nearest 1/16384
    static_assert(grid_of<decltype(s)> == grid{{-1, 1}, per<16384>});
    auto c = math::cos_into<math::unit_amplitude<256>>(angle{1}); // 0.5390625: a coarser output, named

    using level = inside<{{-4, 4}, per<256>}, round_nearest>;
    auto r = math::sqrt(level{2.25});                // expected: the grid holds negative values
    auto n = math::sqrt(level{-1});                  // unexpected(errc::domain_error)

    using down = inside<{{-1, 1}, per<16384>}, round_floor>;
    auto h = units::cos_into<down>(quantity<si::degree, inside<{0, 360}>>{120}); // exactly -1/2, even rounding down
    // [/listing]
    if (s != 0.84149169921875_ins || c != 0.5390625_ins)
        return 1;
    if (!r || *r != 1.5_ins || n || n.error() != errc::domain_error)
        return 1;
    if (h != -0.5_ins)
        return 1;
    return 0;
}
