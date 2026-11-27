// EXPECT: the base must be non-negative
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// pow checks its base on the grid: one reaching below 0 is rejected at
// compile time (pown<E>(x) takes a negative base with an integer exponent).
#include <inside/math.hpp>

using namespace ins;

int main() {
    using base = id<"[-4..4] 1/4">;
    using expo = id<"[0..2] 1/2">;
    using out  = id<"[0..16] 1/16 round_half_away">;
    auto p     = math::pow<out>(base{2}, expo{2});
    (void)p;
}
