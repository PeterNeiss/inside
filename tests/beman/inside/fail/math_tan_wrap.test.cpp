// EXPECT: tan: wrap is not supported
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// tan has no bound near its poles, so a wrap output cannot be given the
// folded true value: rejected (clamp and the default work).
#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>

int main() {
    using namespace beman::inside;
    using out_t = inside<{{-1, 1}, rational{1, 1024}}, round_nearest | wrap>;
    auto t      = math::tan_into<out_t>(inside<{{0, 3}, rational{1, 16}}>{1});
    (void)t;
}
