// EXPECT: log: input must be strictly positive
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The math functions check their mathematical domain on the input grid: log of
// a grid that reaches 0 is rejected at compile time.
#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>

int main() {
    using from_zero = beman::inside::inside<{0, 8}, beman::inside::round_nearest>;
    auto l          = beman::inside::math::log(from_zero{1});
    (void)l;
}
