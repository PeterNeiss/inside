// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// 2-D geometry with dot, cross and lerp on pixel coordinates, px being a unit of our own: each result widens its
// grid like + and *, so it provably cannot overflow, and dot and cross come out in px².

#include <iostream>
#include <string>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;

namespace {
inline constexpr auto px = base_unit<"px", "screen">;
inline constexpr auto zero_area = just<0> * square(px);
} // namespace

int main() {
    using pixels = quantity<px, inside<{-100, 100}>>; // screen coordinates, whole pixels

    // a = (3, 4) and b = (4, -3), chosen perpendicular.
    pixels ax{3, px}, ay{4, px};
    pixels bx{4, px}, by{-3, px};
    std::cout << "a = (" << ax << ", " << ay << ")   b = (" << bx << ", " << by << ")\n\n";

    // dot(a, b) = 0: perpendicular. dot(a, a) = |a|² is exact, with no sqrt needed to compare lengths.
    auto ab = ax * bx + ay * by;
    std::cout << "dot(a, b)      = " << ab << (ab == zero_area ? "   (perpendicular)\n" : "\n");
    std::cout << "dot(a, a)=|a|^2 = " << (ax * ax + ay * ay) << "   (= (5 px)^2)\n";

    // cross(a, b), the z of a×b: its sign says whether b is left (+) or right (-) of a.
    std::cout << "cross(a, b)    = " << (ax * by - ay * bx) << "   (sign = side of a)\n";

    // R's side of the segment P→Q is the sign of cross(Q − P, R − P); every grid widens, nothing overflows.
    auto side = [](pixels p_x, pixels p_y, pixels q_x, pixels q_y, pixels r_x, pixels r_y) {
        auto c = (q_x - p_x) * (r_y - p_y) - (q_y - p_y) * (r_x - p_x);
        return c > zero_area ? "left" : c < zero_area ? "right" : "collinear";
    };
    std::cout << "\nside of segment (0,0)->(10,0):\n";
    const pixels o{0, px}, ten{10, px}, five{5, px}, up{3, px}, down{-3, px};
    std::cout << "  point (5, 3)   is " << side(o, o, ten, o, five, up) << "\n";
    std::cout << "  point (5,-3)   is " << side(o, o, ten, o, five, down) << "\n";
    std::cout << "  point (5, 0)   is " << side(o, o, ten, o, five, o) << "\n";

    // lerp(a, b, t) = a + (b − a) t with t on a [0, 1] grid: it stays inside, and dyadic t is exact.
    using screen_x = quantity<px, inside<{0, 100}>>;
    using fraction = inside<{{0, 1}, per<16>}, round_half_away>;
    screen_x x0{20, px}, x1{80, px};
    std::cout << "\nlerp x from " << x0 << " to " << x1 << ":\n";
    for (cursor<fraction, per<4>> t; t != end(t); ++t) // 0, 0.25, …, 1
        std::cout << "  t = " << t << "  ->  " << lerp(x0, x1, *t) << "\n";

    if (ab != zero_area || to_string(ax * by - ay * bx) != "-25 px²"
        || to_string(lerp(x0, x1, fraction{0.25})) != "35 px" || std::string{side(o, o, ten, o, five, up)} != "left")
        return 1;
    return 0;
}
