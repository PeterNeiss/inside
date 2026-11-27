// EXPECT: the input interval holds a pole
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// tan checks its domain on the input grid: an interval holding a pole (an odd
// multiple of pi/2) is rejected at compile time; on one branch the result is a
// plain inside.
#include <inside/math.hpp>

using namespace ins;

int main() {
    using angle = id<"[0..2] 1/1024 round_half_away">;
    auto t      = math::tan(angle{1});
    (void)t;
}
