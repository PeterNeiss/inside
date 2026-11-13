// EXPECT: unit must be exactly the result's
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A named quantity output has the result's unit: the root of an area is in m,
// not in km (name only the number, sqrt<L>(q), to take the result's unit).
#include <beman/inside/units/math.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    using area_t = quantity<square(si::metre), inside<{0, 100}>>;
    using km_t   = quantity<si::kilo(si::metre), inside<{{0, 1}, per<1000>}, round_nearest>>;
    auto l       = math::sqrt<km_t>(area_t{4});
    (void)l;
}
