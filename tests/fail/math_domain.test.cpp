// EXPECT: log: input must be strictly positive
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The math functions check their mathematical domain on the input grid: log of
// a grid that reaches 0 is rejected at compile time.
#include <inside/inside.hpp>
#include <inside/math.hpp>

int main() {
    using from_zero = ins::inside<{0, 8}, ins::round_half_away>;
    auto l          = ins::math::log(from_zero{1});
    (void)l;
}
