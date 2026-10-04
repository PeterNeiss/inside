// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// 2-D playfield on a torus: sprite position wraps at the edges, with an
// on_wrap callback firing the "crossed edge" game event.
//
// Demonstrates:
//   - `wrap` policy on two fractional-notch axes (sub-pixel positions)
//   - `on_wrap` callback to drive a side-effect on edge crossing
//   - `inside_range` for iterating over integer-cell viewport coordinates

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/formats.hpp>

using namespace beman::inside;

// Position grid: 0..63 inclusive in 1/16-pixel steps. wrap-on-edge.
using pos_t = inside<{{0, 64}, per<16>}, wrap | round_nearest>;

// A signed per-tick movement delta, same 1/16 notch family as pos_t. A runtime
// step has a range — name it — so `x += d` stays inside += inside.
using pos_delta_t = inside<{{-64, 64}, per<16>}, round_nearest>;

// Integer cell coords for the viewport iterator (a separate grid because
// inside_range needs notch 1 with integer Lower).
using cell_t = inside<{0, 63}>;

struct sprite
{
  pos_t x{0}, y{0};
  counter<1'000'000> edge_x_crossings{0};
  counter<1'000'000> edge_y_crossings{0};

  void step(pos_delta_t dx, pos_delta_t dy)
  {
    // The move delta is an inside (same notch family as the position), so the
    // update stays inside += inside and routes through the round-nearest
    // assignment — the on_wrap callback still fires on each edge crossing.
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
  s.x = 60.5;
  s.y = 2.0;

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

  // inside_range over the integer viewport — typical for tilemap iteration.
  // Print the column indices where x would land on an integer notch given
  // the current sub-pixel position.
  std::cout << "\nviewport columns (inside_range<{0,63}>) — first 8:\n  ";
  int shown = 0;
  for (auto c : inside_range<{0, 63}>{})
  {
    if (shown++ >= 8) break;
    std::cout << c << " ";
  }
  std::cout << "...\n";

  return 0;
}
