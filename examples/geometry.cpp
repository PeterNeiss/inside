// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Inside-space 2-D geometry with `dot`, `cross`, and `lerp`.
//
// The three vector helpers keep planar geometry inside the bounded world — no
// dropping to a raw scalar to form a dot product, a 2-D cross (the z-component,
// the classic "which side of a line" test), or a linear interpolation. Each
// widens its result grid like the underlying `+`/`*`, so the result is a plain
// `inside` that provably can't overflow.
//
// Demonstrates:
//   - `dot`   — projection magnitude and a perpendicularity test (dot == 0)
//   - `cross` — signed area / left-vs-right side of a directed segment
//   - `lerp`  — interpolating a point along a path, t a [0, 1] fixed-point inside
//
// The coordinates are in px, a unit of our own on a screen dimension: dot and
// cross come out in px² (an area, compared with 0 px²), lerp in px, and t is
// a plain number.

#include <iostream>
#include <string>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/units.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
inline constexpr base_dimension screen_dim{"screen"};
inline constexpr quantity_spec  screen_def{"screen_distance", screen_dim};
inline constexpr named_unit     px_def{"px", spec_c<screen_def>};
inline constexpr auto           px        = unit_c<px_def>;
inline constexpr auto           zero_area = just<0> * square(px);
} // namespace

int main() {
    // Screen coordinates, integer pixels in [-100, 100] px.
    using coord = quantity<px, inside<{-100, 100}>>;

    // Two vectors a = (3, 4), b = (4, -3) — chosen to be perpendicular.
    coord ax{3}, ay{4};
    coord bx{4}, by{-3};

    std::cout << "a = (" << ax << ", " << ay << ")   b = (" << bx << ", " << by << ")\n\n";

    // dot(a, b) = ax*bx + ay*by. Zero ⇒ perpendicular.
    auto ab = dot(ax, ay, bx, by);
    std::cout << "dot(a, b)      = " << ab << (ab == zero_area ? "   (perpendicular)\n" : "\n");

    // |a|^2 via dot(a, a) — stays exact in inside-space, no sqrt needed for a
    // length comparison.
    std::cout << "dot(a, a)=|a|^2 = " << dot(ax, ay, ax, ay) << "   (= (5 px)^2)\n";

    // cross(a, b) = ax*by - ay*bx — the z-component of a×b. Its sign answers
    // "is b to the left (+) or right (-) of a?". Also twice the signed area of
    // the triangle (0, a, b).
    std::cout << "cross(a, b)    = " << cross(ax, ay, bx, by) << "   (sign = side of a)\n";

    // Side test for three points P, Q, R: cross(Q-P, R-P).
    auto side = [](coord px, coord py, coord qx, coord qy, coord rx, coord ry) {
        // Edge vectors widen to a signed grid; cross widens further. No overflow.
        auto c = cross(qx - px, qy - py, rx - px, ry - py);
        return c > zero_area ? "left" : c < zero_area ? "right" : "collinear";
    };
    std::cout << "\nside of segment (0,0)->(10,0):\n";
    const coord o{0}, ten{10}, five{5}, up{3}, down{-3};
    std::cout << "  point (5, 3)   is " << side(o, o, ten, o, five, up) << "\n";
    std::cout << "  point (5,-3)   is " << side(o, o, ten, o, five, down) << "\n";
    std::cout << "  point (5, 0)   is " << side(o, o, ten, o, five, o) << "\n";

    // lerp(a, b, t) = a + (b - a) * t. `t` is a [0, 1] fixed-point inside, so the
    // interpolation never leaves inside-space; dyadic t values are exact.
    using axis = quantity<px, inside<{0, 100}>>;
    using t_t  = inside<{{0, 1}, per<16>}, round_nearest>;
    axis x0{20}, x1{80};
    std::cout << "\nlerp x from " << x0 << " to " << x1 << ":\n";
    for (cursor<t_t, per<4>> t; t != end(t); ++t) // 0, 0.25, …, 1
        std::cout << "  t = " << t << "  ->  " << lerp(x0, x1, *t) << "\n";

    if (ab != zero_area || to_string(cross(ax, ay, bx, by)) != "-25 px²" ||
        to_string(lerp(x0, x1, t_t{0.25})) != "35 px" || std::string{side(o, o, ten, o, five, up)} != "left")
        return 1;
    return 0;
}
