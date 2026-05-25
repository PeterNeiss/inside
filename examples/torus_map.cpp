// 2-D playfield on a torus: sprite position wraps at the edges, with an
// on_wrap callback firing the "crossed edge" game event.
//
// Demonstrates:
//   - `wrap` policy on two fractional-notch axes (sub-pixel positions)
//   - `on_wrap` callback to drive a side-effect on edge crossing
//   - `bound_range` for iterating over integer-cell viewport coordinates

#include <iostream>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

// Position grid: 0..63 inclusive in 1/16-pixel steps. wrap-on-edge.
using pos_t = bound<{{0, 64}, notch<1, 16>}, wrap | round_nearest>;

// Integer cell coords for the viewport iterator (a separate grid because
// bound_range needs notch 1 with integer Lower).
using cell_t = bound<{0, 63}>;

struct sprite
{
  pos_t x{0}, y{0};
  int edge_x_crossings = 0;
  int edge_y_crossings = 0;

  void step(double dx, double dy)
  {
    // policy_ref::operator+= now accepts a `real` RHS (rational, float,
    // double) and routes through the bound's round-nearest assignment —
    // the on_wrap callback still fires on each edge crossing.
    x.on_wrap([&](auto&, auto carry) {
      (void)carry;
      ++edge_x_crossings;
    }) += dx;
    y.on_wrap([&](auto&, auto carry) {
      (void)carry;
      ++edge_y_crossings;
    }) += dy;
  }
};

int main()
{
  sprite s{};
  s.x = pos_t{60.5};
  s.y = pos_t{2.0};

  // Five movement ticks: enough to wrap once in x, twice in y.
  double moves[][2] = {
    { 1.5,    5.0  },
    { 4.0,   30.0  },
    { 1.5,   40.0  },
    { 8.25, -20.0  },
    {-3.0,   12.0  },
  };

  std::cout << "step   x        y      crossings (x, y)\n";
  for (auto& m : moves)
  {
    s.step(m[0], m[1]);
    std::cout << " " << s.x << "    " << s.y
              << "     (" << s.edge_x_crossings
              << ", " << s.edge_y_crossings << ")\n";
  }

  // bound_range over the integer viewport — typical for tilemap iteration.
  // Print the column indices where x would land on an integer notch given
  // the current sub-pixel position.
  std::cout << "\nviewport columns (bound_range<{0,63}>) — first 8:\n  ";
  int shown = 0;
  for (auto c : bound_range<{0, 63}>{})
  {
    if (shown++ >= 8) break;
    std::cout << c << " ";
  }
  std::cout << "...\n";

  return 0;
}
