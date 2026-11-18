// EXPECT: tan: wrap is not supported
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// tan has no bound near its poles, so a wrap output cannot be given the
// folded true value: rejected (clamp and the default work).
#include <inside/inside.hpp>
#include <inside/math.hpp>

int main() {
    using namespace ins;
    using out_t = inside<{{-1, 1}, detail::rational{1, 1024}}, round_half_away | wrap>;
    auto t      = math::tan<out_t>(inside<{{0, 3}, detail::rational{1, 16}}>{1});
    (void)t;
}
