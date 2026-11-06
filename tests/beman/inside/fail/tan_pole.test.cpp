// EXPECT: the input interval holds a pole
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// tan checks its domain on the input grid: an interval holding a pole (an odd
// multiple of pi/2) is rejected at compile time; on one branch the result is a
// plain inside.
#include <beman/inside/math.hpp>

using namespace beman::inside;

int main() {
    using angle = inside<{{0, 2}, per<1024>}, round_nearest>;
    auto t      = math::tan(angle{1});
    (void)t;
}
