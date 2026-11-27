// EXPECT: wrap onto a grid that does not pass through 0
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A math result onto an unanchored grid is computed on a wider anchored grid
// and then rounded; wrap would fold that wider grid differently, so it is
// rejected (clamp and the default work).
#include <inside/inside.hpp>
#include <inside/math.hpp>

int main() {
    using namespace ins;
    using ins::detail::operator""_r; // the library's own literal
    using out_t = id<"[0.5..10.5] 1 round_half_away|wrap">;
    auto e      = math::exp<out_t>(id<"[0..3]">{1});
    (void)e;
}
