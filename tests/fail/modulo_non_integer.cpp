// EXPECT: modulo requires integer-valued grids and snap
// `%` is integer-only by design — non-integer remainders are not representable
// on a fractional grid without a rounding story.
#include "bound/bound.hpp"

using namespace bnd;

int main()
{
  bound<{{0, 8}, notch<1, 2>}> halves{2.5_b};
  bound<{{1, 4}, notch<1, 2>}> divisor{1.5_b};
  auto rem = halves % divisor;   // ill-formed: fractional grids
  (void)rem;
}
