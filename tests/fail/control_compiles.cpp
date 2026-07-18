// Control case: valid code that MUST compile through the same harness — a
// broken harness (misconfigured compiler, always-failing builds) fails here.
#include "bound/bound.hpp"

using namespace bnd;

int main()
{
  bnd::bound<{0, 100}> percent{50};
  auto sum = percent + 1_b;
  (void)sum;
}
