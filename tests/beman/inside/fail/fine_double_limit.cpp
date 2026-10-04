// EXPECT: _r literal
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A floating-point limit that needs a notch finer than 1/1024 is rejected,
// even when it is an exact binary fraction; spell it 0x1p-11_r.
#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main()
{
  inside<{0, 0x1p-11}> x{0};
  (void)x;
}
