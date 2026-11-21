// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A playfield on a torus: a sprite's sub-pixel position wraps at the edges,
// and on_wrap fires the "crossed edge" event.
// Screen distance is in px, a base dimension of its own, and edge crossings count in their own unit;
// the viewport cells are indices and stay plain.

#include <iostream>
#include <ranges>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/inside/formats.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;
using namespace ins::formats;

namespace
{
  inline constexpr auto px       = base_unit<"px", "screen">;
  inline constexpr auto crossing = base_unit<"crossing", "edge_crossings">;
} // namespace

// [0, 64) px in 1/16-pixel steps, so wrap's period is 64 px.
using pixels = quantity<px, inside<{{0, 64}, per<16>, bounds::half_open}, wrap | round_half_away>>;
// A run-time step has a range too: name it, so x += d stays quantity += quantity.
using pixel_delta = quantity<px, inside<{{-64, 64}, per<16>}, round_half_away>>;

struct sprite
{
  using crossings = quantity<crossing, clamp_counter<1'000'000>>;

  pixels    x{0, px}, y{0, px};
  crossings edge_x_crossings{0, crossing};
  crossings edge_y_crossings{0, crossing};

  // The move rounds to the nearest 1/16 px on the store, and on_wrap fires on each edge crossing.
  void step(pixel_delta dx, pixel_delta dy)
  {
    x.on_wrap([&](auto&, auto) { ++edge_x_crossings; }) += dx;
    y.on_wrap([&](auto&, auto) { ++edge_y_crossings; }) += dy;
  }
};

int main()
{
  sprite s{};
  s.x = pixels{60.5, px};
  s.y = pixels{2.0 , px};

  // Five moves: enough to wrap once in x and three times in y.
  double moves[][2] = {
    {1.5 , 5.0  },
    {4.0 , 30.0 },
    {1.5 , 40.0 },
    {8.25, -20.0},
    {-3.0, 12.0 },
  };

  std::cout << "step   x        y      crossings (x, y)\n";
  for (auto& m : moves)
  {
    const auto dx{pixel_delta::try_make(m[0], px)}, dy{pixel_delta::try_make(m[1], px)}; // NaN: its error
    if (!dx || !dy)
      return 1;
    s.step(*dx, *dy);
    std::cout << " " << s.x << "    " << s.y << "     (" << s.edge_x_crossings << ", " << s.edge_y_crossings
              << ")\n";
  }

  // Every cell of the viewport, as for a tilemap pass; the first 8 here.
  using cell = inside<{0, 63}>; // whole pixels
  std::cout << "\nviewport columns (values<cell>) — first 8:\n  ";
  for (cell c : values<cell> | std::views::take(8))
    std::cout << c << " ";
  std::cout << "...\n";

  if (to_string(s.x) != "8.75 px" || to_string(s.y) != "5 px" || s.edge_x_crossings != crossing
      || s.edge_y_crossings != just<3> * crossing)
    return 1;
  return 0;
}
