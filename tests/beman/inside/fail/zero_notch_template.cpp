// EXPECT: positive step
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// notch<N, D> spells a positive step; a continuous grid is spelled with the
// literal 0, not notch<0>.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main()
{
  inside<{{0, 1}, notch<0>}> x{0};
  (void)x;
}
