// EXPECT: positive step
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// per<D> spells the positive step 1/D; a continuous grid is spelled with the
// literal 0, not per<0>.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main()
{
  inside<{{0, 1}, per<0>}> x{0};
  (void)x;
}
