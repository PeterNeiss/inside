// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Polar coordinates, range and bearing with hypot, atan2, asin and acos: every result is the correctly rounded
// point of its output grid, the same on every platform. atan2 of two lengths is an angle in rad.

#include <iostream>

#include <inside/math.hpp>
#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units/io.hpp>
#include <inside/units/math.hpp>

using namespace ins;

int main() {
    // 1. A polar round trip on a Q.14 unit square, theta onto 2^-14 with its range [-π, π] deduced.
    using coordinate = inside<{{-1, 1}, per<(1 << 14)>}, round_half_away>;
    std::cout << "(x, y) -> (r, theta) -> (x, y)\n";
    for (auto [x, y] : {std::pair{coordinate{1}, coordinate{0}},
                        {coordinate{-1}, coordinate{1}},
                        {coordinate{just<3, 5>}, coordinate{just<4, 5>}},
                        {coordinate{0.5}, coordinate{-0.5}}}) {
        const auto r = math::hypot(x, y);
        const auto theta = math::atan2<per<(1 << 14)>>(y, x);
        // r and cos θ are each rounded, so their product can land a notch past the range: clamp says where it goes.
        const coordinate rx = mul<coordinate>(r, math::cos(theta), clamp);
        const coordinate ry = mul<coordinate>(r, math::sin(theta), clamp);
        std::cout << "  (" << x << ", " << y << ") -> (" << r << ", " << theta << ") -> (" << rx << ", " << ry
                  << ")\n";
        if (math::abs(rx - x) > just<2, (1 << 14)> || math::abs(ry - y) > just<2, (1 << 14)>)
            return 1;
    }

    // 2. Range and compass bearing (from north, clockwise) on a local east/north plane in metres. The bearing
    //    atan2(east, north) is on 2^-20 rad; degrees are a factor of π away, so value_cast rounds once onto 0.1°.
    using metres = quantity<si::metre, inside<{{-1024, 1024}, per<(1 << 8)>}, round_half_away>>;
    using compass_degrees = inside<{{-180, 180}, per<10>}, round_half_away>;
    std::cout << "\ntarget (east, north)   range       bearing\n";
    for (auto [e, n] : {std::pair{metres{30, si::metre}, metres{40, si::metre}},
                        {metres{0, si::metre}, metres{50, si::metre}},
                        {metres{-30, si::metre}, metres{-40, si::metre}}}) {
        const auto range = math::hypot(e, n);
        const auto bearing = math::atan2<per<(1 << 20)>>(e, n);
        const auto deg = value_cast<si::degree, compass_degrees>(bearing);
        std::cout << "  (" << e << ", " << n << ")" << "\t" << range << "\t" << bearing << " = " << deg << "\n";
        if (range != just<50> * si::metre) // 3-4-5 triangles: exact
            return 1;
    }
    const auto south_west = math::atan2<per<(1 << 20)>>(metres{-30, si::metre}, metres{-40, si::metre});
    if (to_string(value_cast<si::degree, compass_degrees>(south_west)) != "-143.1°")
        return 1;

    // 3. asin and acos recover an angle in rad from a plain ratio in [-1, 1].
    using ratio = inside<{{-1, 1}, per<(1 << 12)>}, round_half_away>;
    std::cout << "\nr      asin(r)    acos(r)\n";
    for (const ratio r : {ratio{-1}, ratio{-0.5}, ratio{0}, ratio{0.5}, ratio{1}})
        std::cout << "  " << r << "\t" << math::asin(r) * si::radian << "\t" << math::acos(r) * si::radian << "\n";
    if (math::asin(ratio{0}) != 0 || math::acos(ratio{1}) != 0)
        return 1;
    return 0;
}
