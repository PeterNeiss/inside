// Bound-space 2-D geometry with `dot`, `cross`, and `lerp`.
//
// The three vector helpers keep planar geometry inside the bounded world — no
// dropping to a raw scalar to form a dot product, a 2-D cross (the z-component,
// the classic "which side of a line" test), or a linear interpolation. Each
// widens its result grid like the underlying `+`/`*`, so the result is a plain
// `bound` that provably can't overflow.
//
// Demonstrates:
//   - `dot`   — projection magnitude and a perpendicularity test (dot == 0)
//   - `cross` — signed area / left-vs-right side of a directed segment
//   - `lerp`  — interpolating a point along a path, t a [0, 1] fixed-point bound

#include <iostream>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

int main()
{
  // Screen-ish coordinates, integer pixels in [-100, 100].
  using coord = bound<{-100, 100}>;

  // Two vectors a = (3, 4), b = (4, -3) — chosen to be perpendicular.
  coord ax{3}, ay{4};
  coord bx{4}, by{-3};

  std::cout << "a = (" << ax << ", " << ay << ")   b = (" << bx << ", " << by << ")\n\n";

  // dot(a, b) = ax*bx + ay*by. Zero ⇒ perpendicular.
  auto ab = dot(ax, ay, bx, by);
  std::cout << "dot(a, b)      = " << ab
            << (ab == 0 ? "   (perpendicular)\n" : "\n");

  // |a|^2 via dot(a, a) — stays exact in bound-space, no sqrt needed for a
  // length comparison.
  std::cout << "dot(a, a)=|a|^2 = " << dot(ax, ay, ax, ay) << "   (= 25 = 5^2)\n";

  // cross(a, b) = ax*by - ay*bx — the z-component of a×b. Its sign answers
  // "is b to the left (+) or right (-) of a?". Also twice the signed area of
  // the triangle (0, a, b).
  std::cout << "cross(a, b)    = " << cross(ax, ay, bx, by) << "   (sign = side of a)\n";

  // Side test for three points P, Q, R: cross(Q-P, R-P).
  auto side = [](coord px, coord py, coord qx, coord qy, coord rx, coord ry)
  {
    // Edge vectors widen to a signed grid; cross widens further. No overflow.
    auto c = cross(qx - px, qy - py, rx - px, ry - py);
    return c > 0 ? "left" : c < 0 ? "right" : "collinear";
  };
  std::cout << "\nside of segment (0,0)->(10,0):\n";
  std::cout << "  point (5, 3)   is " << side(0, 0, 10, 0, 5, 3)  << "\n";
  std::cout << "  point (5,-3)   is " << side(0, 0, 10, 0, 5, -3) << "\n";
  std::cout << "  point (5, 0)   is " << side(0, 0, 10, 0, 5, 0)  << "\n";

  // lerp(a, b, t) = a + (b - a) * t. `t` is a [0, 1] fixed-point bound, so the
  // interpolation never leaves bound-space; dyadic t values are exact.
  using axis = bound<{0, 100}>;
  using t_t  = bound<{{0, 1}, notch<1, 16>}, round_nearest>;
  axis x0{20}, x1{80};
  std::cout << "\nlerp x from " << x0 << " to " << x1 << ":\n";
  for (t_t t : { t_t{0}, t_t{0.25}, t_t{0.5}, t_t{0.75}, t_t{1} })
    std::cout << "  t = " << t << "  ->  " << lerp(x0, x1, t) << "\n";

  return 0;
}
