// EXPECT: unit must be exactly the result's
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A named quantity output has the result's unit: the root of an area is in m,
// not in km (name only the number, sqrt<L>(q), to take the result's unit).
#include <inside/units/math.hpp>

int main() {
    using namespace ins;
    using area_t = id<"[0..100] [m^2]">;
    using km_t   = id<"[0..1] 1/1000 [km] round_half_away">;
    auto l       = math::sqrt<km_t>(area_t{4, square(si::metre)});
    (void)l;
}
