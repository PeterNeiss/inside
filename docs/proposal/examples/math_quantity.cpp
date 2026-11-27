// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Tour: on a quantity the unit follows the function, and π is reduced exactly.
#include <inside/units/math.hpp>

#include <type_traits>

using namespace ins;

int main() {
    // [listing]
    using down = id<"[-1..1] 1/16384 round_floor">;
    auto h = math::cos<down>(just<120> * si::degree); // exactly -1/2, even rounding down

    using area_t = id<"[0..100] 1/1024 [m^2] round_half_away">;
    auto side = math::sqrt(area_t{2, square(si::metre)}); // 1.4140625 m: the unit's root
    static_assert(std::is_same_v<decltype(side)::rep, id<"[0..10] 1/1024 round_half_away">>);

    using rad_t = id<"[0..7] 1/1'000'000 round_half_away">;
    auto right = value_cast<si::radian, rad_t>(just<90> * si::degree); // 1.570796 rad: π/2, rounded once
    // [/listing]
    if (h != -0.5_ins || side.numerical_value_in(si::metre) != 1.4140625_ins ||
        right.numerical_value_in(si::radian) != 1.570796_ins)
        return 1;
    return 0;
}
