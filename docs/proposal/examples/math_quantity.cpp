// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour: on a quantity the unit follows the function, and π is reduced exactly.
#include <beman/inside/units_math.hpp>

#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::units;

int main() {
    // [listing]
    using down = inside<{{-1, 1}, per<16384>}, round_floor>;
    auto h = units::cos_into<down>(quantity<si::degree, inside<{0, 360}>>{120}); // exactly -1/2, even rounding down

    using area_t = quantity<square(si::metre), inside<{{0, 100}, per<1024>}, round_nearest>>;
    auto side = units::sqrt(area_t{2});              // 1.4140625 m: the unit's root
    static_assert(std::is_same_v<decltype(side)::rep, inside<{{0, 10}, per<1024>}, round_nearest>>);

    using rad_t = inside<{{0, 7}, per<1'000'000>}, round_nearest>;
    auto right = value_cast<si::radian, rad_t>(quantity<si::degree, inside<{0, 360}>>{90}); // 1.570796 rad: π/2, rounded once
    // [/listing]
    if (h != -0.5_ins || side.numerical_value() != 1.4140625_ins || right.numerical_value() != 1.570796_ins)
        return 1;
    return 0;
}
