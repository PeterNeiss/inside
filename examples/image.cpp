// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Images and fields with std::mdspan (inside/mdspan.hpp, units/mdspan.hpp).
//   1. An 8×8 grayscale image and a 3×3 box blur. Rows and columns are
//      insides on 0 … 7, which a std::mdspan takes as they are: the index
//      type proves every access within the extents, and its clamp policy
//      pads the edges. Each 9-pixel sum is exact, on [0, 2295], and rounded
//      once into the output pixel.
//   2. Interior pixels: an index on 1 … 6 and its neighbours, proven within
//      the image by the grids alone.
//   3. A heat map: a sensor's raw deci-kelvin codes viewed as kelvin without
//      copying. The mean is exact, the hottest cell is found by comparing
//      quantities, and the gradient between neighbours is a transform_into
//      over two shifted submdspans.

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <ranges>
#include <string>

#include <beman/inside/inside.hpp>
#include <beman/inside/inside/io.hpp>
#include <beman/inside/inside/mdspan.hpp>
#include <beman/inside/math.hpp>
#include <beman/inside/units/io.hpp>
#include <beman/inside/units/mdspan.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

using pixel  = inside<{0, 255}>;
using smooth = inside<{0, 255}, round_nearest>; // a blurred pixel: rounded once
// An index: a std::mdspan takes it as it is. clamp: a value past an edge,
// stored into the index, is the edge.
using row          = inside<{0, 7}, clamp>;
using col          = inside<{0, 7}, clamp>;
using offset       = inside<{-1, 1}, clamp>;
using image        = std::mdspan<pixel, extents_of<row, col>>;
using smooth_image = std::mdspan<smooth, extents_of<row, col>>;
// The sum of 9 pixels. sum<window> counts at run time, so its type cannot
// show that 9 pixels of at most 255 fit: clamp names what a larger total
// would do (it never happens here).
using window = inside<{0, 9 * 255}, clamp>;

// out = the 3×3 mean of in, each output pixel rounded once.
void blur(const image& in, const smooth_image& out) {
    for (auto [i, j] : indices(in)) {
        const auto around =
            std::views::cartesian_product(values<offset>, values<offset>) | std::views::transform([&](auto d) {
                const auto [dr, dc] = d;
                return in[row{i + dr}, col{j + dc}]; // past an edge: the edge
            });
        out[i, j] = sum<window>(around) / 9_ins; // exact, then rounded to the nearest pixel
    }
}

int main() {
    // 1. A dark image with a bright 4×4 square and one bright corner pixel.
    std::array<pixel, 64> px{};
    std::ranges::fill(px, pixel{10});
    const image img{px.data()};
    for (auto [r, c] : indices(img))
        if (r >= 2 && r <= 5 && c >= 2 && c <= 5)
            img[r, c] = pixel{200};
    img[row{0}, col{0}] = pixel{255};

    std::array<smooth, 64> bl{};
    const smooth_image     blurred{bl.data()};
    blur(img, blurred);
    std::cout << "blurred:\n";
    for (auto [r, c] : indices(blurred))
        std::cout << (c == 0 ? "  " : " ") << std::string(3 - to_string(blurred[r, c]).size(), ' ') << blurred[r, c]
                  << (c == 7 ? "\n" : "");

    // The corner reads itself 4 times (clamped), its two neighbours twice and
    // the diagonal once: (4·255 + 4·10 + 10) / 9 = 1070/9, rounded to 119.
    const auto corner = blurred[row{0}, col{0}];
    // The mean: an exact total on [0, 64·255] over 64, exactly.
    const auto total = sum(img);
    static_assert(upper_of<decltype(total)> == 64 * 255);
    const auto mean = total / 64_ins;
    std::cout << "corner " << corner << ", centre " << blurred[row{3}, col{3}] << ", mean " << mean << "\n";
    if (corner != 119 || blurred[row{3}, col{3}] != 200 || to_string(mean) != "61.328125")
        return 1;

    // 2. Interior pixels need no clamp: an index on 1 … 6 plus or minus 1 is
    // on 0 … 7, so the store into the image's index type is proven by the
    // grids. The largest step between a pixel's left and right neighbours:
    using inner = inside<{1, 6}>;
    using index = inside<{0, 7}>;
    pixel edge  = 0;
    for (auto [r, c] : std::views::cartesian_product(values<inner>, values<inner>)) {
        const auto step = math::abs(img[r, index{c + 1_ins}] - img[r, index{c - 1_ins}]);
        if (step > edge)
            edge = step;
    }
    std::cout << "largest step " << edge << "\n";
    if (edge != 190)
        return 1;

    // 3. A heat map: 4×6 raw codes in steps of 0.1 K from 200 K, as a sensor
    // sends them, read as kelvin through raw_accessor.
    using kelvin = quantity<si::kelvin, inside<{{200, 400}, per<10>}>>;
    const std::array<std::uint16_t, 24> codes{
        731, 733, 735, 738, 741, 744, //
        732, 736, 742, 751, 760, 766, //
        733, 739, 749, 765, 781, 790, //
        733, 740, 752, 771, 789, 801, //
    };
    const std::
        mdspan<kelvin, std::extents<std::size_t, 4, 6>, std::layout_right, raw_accessor<kelvin, const std::uint16_t>>
               heat{codes.data()};
    const auto heat_total = sum(heat);
    std::cout << "heat: " << heat[0, 0] << " to " << heat[3, 5] << ", mean " << (heat_total / 24_ins) << "\n";

    // The indices of the extents 4 × 6: inside<{0, 3}>, inside<{0, 5}>.
    extent_index_t<decltype(heat)::extents_type, 0> hot_r;
    extent_index_t<decltype(heat)::extents_type, 1> hot_c;
    for (auto [r, c] : indices(heat))
        if (heat[r, c] > heat[hot_r, hot_c])
            hot_r = r, hot_c = c;
    std::cout << "hottest cell (" << hot_r << ", " << hot_c << ") at " << heat[hot_r, hot_c] << "\n";

    // The rise from each cell to its right neighbour: two shifted views of
    // the same buffer, subtracted elementwise.
    using rise = quantity<si::kelvin, inside<{{-20, 20}, per<10>}>>;
    std::array<rise, 20> g{};
    const std::mdspan    gradient{g.data(), std::extents<std::size_t, 4, 5>{}};
    transform_into(std::submdspan(heat, std::full_extent, std::pair{0, 5}),
                   std::submdspan(heat, std::full_extent, std::pair{1, 6}),
                   gradient,
                   [](kelvin left, kelvin right) { return right - left; });
    const auto steepest = *std::ranges::max_element(g);
    std::cout << "steepest rise " << steepest << "\n";
    if (to_string(heat[0, 0]) != "273.1 K" || to_string(heat_total) != "6605.2 K" || hot_r != 3 || hot_c != 5 ||
        to_string(steepest) != "1.9 K")
        return 1;
    return 0;
}
