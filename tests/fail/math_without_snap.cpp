// EXPECT: must permit rounding
// Transcendentals round their result onto the grid, so the operand must carry
// `snap` (via round_nearest / a round_* mode / real) — require_snap fires.
#include "bound/bound.hpp"
#include "bound/cmath.hpp"

int main()
{
  bnd::bound<{0, 3}> plain{1};   // no snap permission
  auto s = bnd::math::sin(plain);
  (void)s;
}
