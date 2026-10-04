// EXPECT: atanh: input must be in
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// atanh(±1) is infinite: an input grid reaching ±1 is rejected at compile time.
#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>

using namespace beman::inside;

int main()
{
  using unit = inside<{{-1, 1}, per<1024>}, round_nearest>;
  auto r = math::atanh(unit{0});
  (void)r;
}
