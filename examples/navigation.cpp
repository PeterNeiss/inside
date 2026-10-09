// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Polar coordinates, range and bearing — the inverse-trig and hypot corner of
// beman::inside::math. Every result is the correctly rounded point of its
// output grid, the same on every platform and at compile time; angles are in
// radians, as in <cmath>.
//   1. Cartesian → polar with `hypot` and `atan2`, and back with `sin`/`cos`.
//   2. Range and bearing to targets on a local east/north plane.
//   3. `asin` / `acos` recover an angle from a ratio in [-1, 1].

#include <iostream>

#include <beman/inside/cmath.hpp>
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

int main() {
    // 1. Polar round trip on a Q.14 unit square.
    using coord_t = inside<{{-1, 1}, per<16384>}, round_nearest>;
    using angle_t = inside<{{-4, 4}, per<16384>}, round_nearest>;
    std::cout << "(x, y) -> (r, theta) -> (x, y)\n";
    for (auto [x, y] : {std::pair{coord_t{1}, coord_t{0}},
                        {coord_t{-1}, coord_t{1}},
                        {coord_t{frac<3, 5>}, coord_t{frac<4, 5>}},
                        {coord_t{0.5}, coord_t{-0.5}}}) {
        const auto    r = math::hypot(x, y);
        const angle_t theta{math::atan2(y, x)};
        const coord_t rx{r * math::cos(theta)}, ry{r * math::sin(theta)};
        std::cout << "  (" << x << ", " << y << ") -> (" << r << ", " << theta << ") -> (" << rx << ", " << ry
                  << ")\n";
        if (abs(rational{rx} - rational{x}) > rational{2, 16384} ||
            abs(rational{ry} - rational{y}) > rational{2, 16384})
            return 1;
    }

    // 2. Range and bearing, metres at 1/256 m.
    using pos_t = inside<{{-1024, 1024}, per<256>}, round_nearest>;
    std::cout << "\ntarget (east, north)   range (m)   bearing (rad)\n";
    for (auto [e, n] : {std::pair{pos_t{30}, pos_t{40}}, {pos_t{0}, pos_t{50}}, {pos_t{-30}, pos_t{-40}}}) {
        const auto range = math::hypot(e, n);
        std::cout << "  (" << e << ", " << n << ")" << "\t\t" << range << "\t" << math::atan2(n, e) << "\n";
        if (range != 50) // 3-4-5 triangles: exact
            return 1;
    }

    // 3. Inverse trig on [-1, 1].
    using ratio_t = inside<{{-1, 1}, per<4096>}, round_nearest>;
    std::cout << "\nr      asin(r)    acos(r)\n";
    for (const ratio_t r : {ratio_t{-1}, ratio_t{-0.5}, ratio_t{0}, ratio_t{0.5}, ratio_t{1}})
        std::cout << "  " << r << "\t" << math::asin(r) << "\t" << math::acos(r) << "\n";
    if (math::asin(ratio_t{0}) != 0 || math::acos(ratio_t{1}) != 0)
        return 1;
    return 0;
}
