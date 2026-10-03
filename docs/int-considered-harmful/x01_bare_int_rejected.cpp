// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// DOES NOT COMPILE, BY DESIGN.
//
// A bare integer literal carries no grid, so it cannot enter inside
// arithmetic. You must say what the 1 means: `1_ins`, or `just<1>`.
// This is the diagnostic that replaces the silent unit mix-up in
// h09_units.cpp.

#include <beman/inside/inside.hpp>

using namespace beman::inside;

int main()
{
  inside<{0, 100}> x{42};
  auto bad = x + 1;      // error: no grid for the literal
  (void)bad;
  return 0;
}
