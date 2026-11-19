// EXPECT: takes two quantities that add
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// atan2 of a length and a duration: its arguments must add.
#include <inside/units/math.hpp>

int main() {
    using namespace ins;
    using m_t = quantity<si::metre, inside<{{-1, 1}, per<8>}, round_half_away>>;
    using s_t = quantity<si::second, inside<{{-1, 1}, per<8>}, round_half_away>>;
    auto a    = math::atan2(m_t{1, si::metre}, s_t{1, si::second});
    (void)a;
}
