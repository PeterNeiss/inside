// EXPECT: clamp and wrap are mutually exclusive
// Contradictory out-of-range policies on one bound are rejected in the class
// body's mutual-exclusion static_asserts.
#include "bound/bound.hpp"

int main()
{
  bnd::bound<{0, 100}, bnd::clamp | bnd::wrap> contradictory{};
  (void)contradictory;
}
