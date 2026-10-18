// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// 2-D playfield on a torus: sprite position wraps at the edges, with an
// on_wrap callback firing the "crossed edge" game event. Screen distance is a
// base dimension of its own with the unit px, so a position never mixes with
// a metre or a plain number; the viewport cells are indices and stay plain.
//
// Demonstrates:
//   - `wrap` policy on two fractional-notch axes (sub-pixel positions)
//   - `on_wrap` callback to drive a side-effect on edge crossing
//   - `values<T>` for iterating over whole-pixel viewport cells

#include <iostream>
#include <ranges>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/formats.hpp>
#include <beman/inside/units.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
inline constexpr base_dimension screen_dim{"screen"};
inline constexpr quantity_spec  screen_def{"screen_distance", screen_dim};
inline constexpr named_unit     px_def{"px", spec_c<screen_def>};
inline constexpr auto           px = unit_c<px_def>;
} // namespace

// Position grid: [0, 64) px in 1/16-pixel steps, so wrap's period is 64 px.
using pos_t = quantity<px, inside<{{0, 64}, per<16>, bounds::half_open}, wrap | round_nearest>>;

// A signed per-tick movement delta, same 1/16 notch family as pos_t. A runtime
// step has a range — name it — so `x += d` stays quantity += quantity.
using pos_delta_t = quantity<px, inside<{{-64, 64}, per<16>}, round_nearest>>;

// Whole-pixel cells for the viewport.
using cell_t = inside<{0, 63}>;

struct sprite {
    pos_t                    x{0}, y{0};
    clamp_counter<1'000'000> edge_x_crossings{0};
    clamp_counter<1'000'000> edge_y_crossings{0};

    void step(pos_delta_t dx, pos_delta_t dy) {
        // The move delta is a quantity (same notch family as the position), so the
        // update stays quantity += quantity and routes through the round-nearest
        // assignment — the on_wrap callback still fires on each edge crossing.
        x.on_wrap([&](auto&, auto) { ++edge_x_crossings; }) += dx;
        y.on_wrap([&](auto&, auto) { ++edge_y_crossings; }) += dy;
    }
};

int main() {
    sprite s{};
    s.x = pos_t{60.5};
    s.y = pos_t{2.0};

    // Five movement ticks: enough to wrap once in x, twice in y.
    double moves[][2] = {
        {1.5, 5.0},
        {4.0, 30.0},
        {1.5, 40.0},
        {8.25, -20.0},
        {-3.0, 12.0},
    };

    std::cout << "step   x        y      crossings (x, y)\n";
    for (auto& m : moves) {
        s.step(pos_delta_t{m[0]}, pos_delta_t{m[1]});
        std::cout << " " << s.x << "    " << s.y << "     (" << s.edge_x_crossings << ", " << s.edge_y_crossings
                  << ")\n";
    }

    // Every cell of the viewport, as for a tilemap pass; the first 8 here.
    std::cout << "\nviewport columns (values<cell_t>) — first 8:\n  ";
    for (cell_t c : values<cell_t> | std::views::take(8))
        std::cout << c << " ";
    std::cout << "...\n";

    if (to_string(s.x) != "8.75 px" || to_string(s.y) != "5 px" || s.edge_x_crossings != 1 || s.edge_y_crossings != 3)
        return 1;
    return 0;
}
