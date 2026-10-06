// EXPECT: the notch must be non-negative
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A grid decodes as Lower + raw·Notch, so a negative notch would count
// downward; grid::validate rejects it.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main()
{
  inside<{{0, 10}, -2}> x{0};
  (void)x;
}
