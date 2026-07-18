// EXPECT: entirely outside lhs interval
// Assigning from a bound whose interval cannot overlap the target is rejected
// at compile time (bound_assignable_why names the failing clause).
#include "bound/bound.hpp"

int main()
{
  bnd::bound<{0, 10}>  small{5};
  bnd::bound<{20, 30}> big{25};
  small = big;   // ill-formed: [20,30] never fits in [0,10]
}
