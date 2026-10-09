// EXPECT: wrap onto a grid that does not pass through 0
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A math result onto an unanchored grid is computed on a wider anchored grid
// and then rounded; wrap would fold that wider grid differently, so it is
// rejected (clamp and the default work).
#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>

int main() {
    using namespace beman::inside;
    using out_t = inside<{{0.5_r, 10.5_r}, 1}, round_nearest | wrap>;
    auto e      = math::exp_into<out_t>(inside<{0, 3}>{1});
    (void)e;
}
