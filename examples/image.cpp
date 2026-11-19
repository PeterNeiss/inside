// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Images and fields with std::mdspan: indices that are insides prove every access within the extents, a clamp
// index pads the edges of a blur, and a sensor's raw codes are viewed as kelvin without copying.

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <ranges>
#include <string>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/inside/mdspan.hpp>
#include <inside/math.hpp>
#include <inside/units/io.hpp>
#include <inside/units/mdspan.hpp>

using namespace ins;

using pixel = inside<{0, 255}>;
using smooth = inside<{0, 255}, round_half_away>; // a blurred pixel: rounded once
using row = inside<{0, 7}, clamp>;                // clamp: an index past an edge is the edge
using col = inside<{0, 7}, clamp>;
using offset = inside<{-1, 1}, clamp>;
using image = std::mdspan<pixel, extents_of<row, col>>; // a std::mdspan takes the indices as they are
using smooth_image = std::mdspan<smooth, extents_of<row, col>>;
// The sum of 9 pixels: sum counts at run time, so its type cannot show they fit; clamp names what would happen.
using window = inside<{0, 9 * 255}, clamp>;

// out = the 3×3 mean of in: each 9-pixel sum is exact, then rounded once into the output pixel.
void blur(const image& in, const smooth_image& out) {
    for (auto [i, j] : indices(in)) {
        const auto around = std::views::cartesian_product(values<offset>, values<offset>)
                          | std::views::transform([&](auto d) {
                                const auto [dr, dc] = d;
                                return in[row{i + dr}, col{j + dc}]; // past an edge: the edge
                            });
        out[i, j] = sum<window>(around) / 9_ins;
    }
}

int main() {
    // 1. A dark 8×8 image with a bright 4×4 square and one bright corner pixel, and its box blur.
    std::array<pixel, 64> px{};
    std::ranges::fill(px, pixel{10});
    const image img{px.data()};
    for (auto [r, c] : indices(img))
        if (r >= 2 && r <= 5 && c >= 2 && c <= 5)
            img[r, c] = pixel{200};
    img[row{0}, col{0}] = pixel{255};

    std::array<smooth, 64> bl{};
    const smooth_image blurred{bl.data()};
    blur(img, blurred);
    std::cout << "blurred:\n";
    for (auto [r, c] : indices(blurred))
        std::cout << (c == 0 ? "  " : " ") << std::string(3 - to_string(blurred[r, c]).size(), ' ')
                  << blurred[r, c] << (c == 7 ? "\n" : "");

    // The corner reads itself 4 times (clamped), its neighbours twice, the diagonal once: 1070/9, rounded to 119.
    const auto corner = blurred[row{0}, col{0}];
    const auto total = sum(img); // exact, on [0, 64·255]
    static_assert(upper_of<decltype(total)> == 64 * 255);
    const auto mean = total / 64_ins;
    std::cout << "corner " << corner << ", centre " << blurred[row{3}, col{3}] << ", mean " << mean << "\n";
    if (corner != 119 || blurred[row{3}, col{3}] != 200 || to_string(mean) != "61.328125")
        return 1;

    // 2. Interior pixels need no clamp: 1 … 6 plus or minus 1 is on 0 … 7, proven by the grids.
    //    The largest step between a pixel's left and right neighbours:
    using inner_index = inside<{1, 6}>;
    using image_index = inside<{0, 7}>;
    pixel edge = 0;
    for (auto [r, c] : std::views::cartesian_product(values<inner_index>, values<inner_index>)) {
        const auto step = math::abs(img[r, image_index{c + 1_ins}] - img[r, image_index{c - 1_ins}]);
        if (step > edge)
            edge = step;
    }
    std::cout << "largest step " << edge << "\n";
    if (edge != 190)
        return 1;

    // 3. A heat map: 4×6 raw codes in 0.1 K steps from 200 K, as a sensor sends them, read as kelvin.
    using cell_kelvins = quantity<si::kelvin, inside<{{200, 400}, per<10>}>>;
    using kelvin_view = std::mdspan<cell_kelvins, std::extents<std::size_t, 4, 6>, std::layout_right,
                                    raw_accessor<cell_kelvins, const std::uint16_t>>;
    const std::array<std::uint16_t, 24> codes{
        731, 733, 735, 738, 741, 744,
        732, 736, 742, 751, 760, 766,
        733, 739, 749, 765, 781, 790,
        733, 740, 752, 771, 789, 801,
    };
    const kelvin_view heat{codes.data()};
    const auto heat_total = sum(heat);
    std::cout << "heat: " << heat[0, 0] << " to " << heat[3, 5] << ", mean " << (heat_total / 24_ins) << "\n";

    // The hottest cell, by comparing quantities; the indices of the extents 4 × 6 are inside<{0, 3}>, <{0, 5}>.
    extent_index_t<kelvin_view::extents_type, 0> hot_r;
    extent_index_t<kelvin_view::extents_type, 1> hot_c;
    for (auto [r, c] : indices(heat))
        if (heat[r, c] > heat[hot_r, hot_c])
            hot_r = r, hot_c = c;
    std::cout << "hottest cell (" << hot_r << ", " << hot_c << ") at " << heat[hot_r, hot_c] << "\n";

    // The rise from each cell to its right neighbour: two shifted views of one buffer, subtracted elementwise.
    // (A rise past 20 K would not fit: transform_into reports it.)
    using rise_kelvins = quantity<si::kelvin, inside<{{-20, 20}, per<10>}>>;
    std::array<rise_kelvins, 20> g{};
    const std::mdspan gradient{g.data(), std::extents<std::size_t, 4, 5>{}};
    if (!transform_into(std::submdspan(heat, std::full_extent, std::pair{0, 5}),
                        std::submdspan(heat, std::full_extent, std::pair{1, 6}), gradient,
                        [](cell_kelvins left, cell_kelvins right) { return right - left; }))
        return 1;
    const auto steepest = *std::ranges::max_element(g);
    std::cout << "steepest rise " << steepest << "\n";
    if (to_string(heat[0, 0]) != "273.1 K" || to_string(heat_total) != "6605.2 K" || hot_r != 3 || hot_c != 5
        || to_string(steepest) != "1.9 K")
        return 1;
    return 0;
}
