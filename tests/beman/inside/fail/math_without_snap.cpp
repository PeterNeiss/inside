// EXPECT: must permit rounding
// Transcendentals round their result onto the grid, so the operand must carry
// `snap` (via round_nearest / a round_* mode / real) — require_snap fires.
#include <beman/inside/inside.hpp>
#include <beman/inside/cmath.hpp>

int main()
{
  beman::inside::inside<{0, 3}> plain{1};   // no snap permission
  auto s = beman::inside::math::sin(plain);
  (void)s;
}
