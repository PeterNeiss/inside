// EXPECT: the base must be non-negative
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// pow checks its base on the grid: one reaching below 0 is rejected at
// compile time (pown<E>(x) takes a negative base with an integer exponent).
#include <beman/inside/math.hpp>

using namespace beman::inside;

int main() {
    using base = inside<{{-4, 4}, per<4>}>;
    using expo = inside<{{0, 2}, per<2>}>;
    using out  = inside<{{0, 16}, per<16>}, round_nearest>;
    auto p     = math::pow<out>(base{2}, expo{2});
    (void)p;
}
