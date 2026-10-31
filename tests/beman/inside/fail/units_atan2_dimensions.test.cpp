// EXPECT: takes two quantities that add
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// atan2 of a length and a duration: its arguments must add.
#include <beman/inside/units/math.hpp>

int main() {
    using namespace beman::inside;
    using namespace beman::inside::units;
    using m_t = quantity<si::metre, inside<{{-1, 1}, per<8>}, round_nearest>>;
    using s_t = quantity<si::second, inside<{{-1, 1}, per<8>}, round_nearest>>;
    auto a    = atan2(m_t{1}, s_t{1});
    (void)a;
}
