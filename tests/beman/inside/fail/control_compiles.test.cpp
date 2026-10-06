// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Control case: valid code that MUST compile through the same harness — a
// broken harness (misconfigured compiler, always-failing builds) fails here.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main()
{
  beman::inside::inside<{0, 100}> percent{50};
  auto sum = percent + 1_ins;
  (void)sum;
}
