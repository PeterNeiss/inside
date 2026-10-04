// EXPECT: input magnitudes must be
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Every engine enforces the same input domain: sin of a grid wider than ±2^20 rad
// is rejected whichever engine the build selects (the FP engines used to skip
// the check the CORDIC engine applied).
#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>

int main()
{
  using wide = beman::inside::inside<{{-(1 << 24), 1 << 24}, beman::inside::per<16>},
                                     beman::inside::round_nearest>;
  auto s = beman::inside::math::sin(wide{1});
  (void)s;
}
