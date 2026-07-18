// EXPECT: can only be added to another bound
// Grid-less scalars are rejected with a fix-it (write `a + 1_b`), not a wall
// of overload-resolution noise — the guidance overload's static_assert fires.
#include "bound/bound.hpp"

int main()
{
  bnd::bound<{0, 100}> percent{50};
  auto sum = percent + 1;   // ill-formed: scalar has no grid
  (void)sum;
}
