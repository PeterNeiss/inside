// DOES NOT COMPILE, BY DESIGN.
//
// A bare integer literal carries no grid, so it cannot enter bound
// arithmetic. You must say what the 1 means: `1_b`, or `just<1>`.
// This is the diagnostic that replaces the silent unit mix-up in
// h09_units.cpp.

#include "bound/bound.hpp"

using namespace bnd;

int main()
{
  bound<{0, 100}> x{42};
  auto bad = x + 1;      // error: no grid for the literal
  (void)bad;
  return 0;
}
