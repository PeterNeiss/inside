// Range, bearing, and elevation to a target — the inverse-trig + hypot corner
// of bnd::math. All operands carry the `real` policy (the transcendentals
// require it); angles come back in radians, consistent with sin/cos/atan2.
//
// Demonstrates:
//   - `hypot(x, y)` — Euclidean ground range without intermediate overflow
//   - `atan2(y, x)` — bearing in radians ∈ [-π, π]
//   - `asin` / `acos` — recovering an angle from a normalized ratio in [-1, 1]
//     (the inverse of the sin/cos used in polar.cpp / oscillator.cpp)
//
// Runs identically on both math engines (default `double` and the fixed/CORDIC
// engine): no `<cmath>`, no FPU dependence in the result.

#include <iostream>

#include "bound/bound.hpp"
#include "bound/cmath.hpp"
#include "bound/print.hpp"

using namespace bnd;

int main()
{
  // Local-tangent-plane offsets in metres, 1/256 m resolution. `real` + a dyadic
  // notch is the standard math-operand shape; the fine notch also sharpens the
  // deduced bearing grid (atan2's output inherits the input notch). Magnitudes
  // stay well under the 2^20 working-scale envelope hypot/atan2 require.
  using pos_t = bound<{{-1024, 1024}, notch<1, 256>}, round_nearest | real>;

  struct target { pos_t east; pos_t north; const char* label; };
  const target targets[] = {
    {  30,  40, "NE  (3-4-5)   " },
    {   0,  50, "due north     " },
    { -50,   0, "due west      " },
    { -30, -40, "SW            " },
    {  60,  25, "ENE           " },
  };

  std::cout << "Range & bearing to target (east, north):\n";
  std::cout << "  target            range (m)      bearing (rad)\n";
  for (auto& t : targets)
  {
    auto range   = math::hypot(t.east, t.north);   // >= 0, no overflow
    auto bearing = math::atan2(t.north, t.east);    // radians in [-pi, pi]
    std::cout << "  " << t.label
              << "    " << range
              << "        " << bearing << "\n";
  }

  // asin / acos recover an angle from a normalized ratio in [-1, 1] — e.g. an
  // elevation whose sine (height / slant-range) is known. Inputs are `real`
  // bounds clamped to the [-1, 1] domain the functions require.
  using ratio_t = bound<{{-1, 1}, notch<1, 4096>}, round_nearest | real>;
  std::cout << "\nInverse trig (radians):\n";
  std::cout << "  r        asin(r)        acos(r)\n";
  for (ratio_t r : { ratio_t{-1}, ratio_t{-0.5}, ratio_t{0}, ratio_t{0.5}, ratio_t{1} })
    std::cout << "  " << r
              << "     " << math::asin(r)
              << "     " << math::acos(r) << "\n";

  return 0;
}
