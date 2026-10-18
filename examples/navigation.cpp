// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Polar coordinates, range and bearing — the inverse-trig and hypot corner of
// beman::inside::math. Every result is the correctly rounded point of its
// output grid, the same on every platform and at compile time; angles are
// quantities in radians, and a bearing converts to degrees with one rounding.
//   1. Cartesian → polar with `hypot` and `atan2`, and back with `sin`/`cos`
//      (dimensionless coordinates on a unit square).
//   2. Range (m) and compass bearing (from north, clockwise) to targets on a
//      local east/north plane in metres.
//   3. `asin` / `acos` recover an angle in rad from a ratio in [-1, 1].

#include <iostream>

#include <beman/inside/cmath.hpp>
#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/units_math.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

int main() {
    // 1. Polar round trip on a Q.14 unit square.
    using coord_t = inside<{{-1, 1}, per<16384>}, round_nearest>;
    using angle_t = inside<{{-4, 4}, per<16384>}, round_nearest>;
    std::cout << "(x, y) -> (r, theta) -> (x, y)\n";
    for (auto [x, y] : {std::pair{coord_t{1}, coord_t{0}},
                        {coord_t{-1}, coord_t{1}},
                        {coord_t{just<3, 5>}, coord_t{just<4, 5>}},
                        {coord_t{0.5}, coord_t{-0.5}}}) {
        const auto    r     = math::hypot(x, y);
        const auto    theta = math::atan2_into<angle_t>(y, x);
        const coord_t rx = mul_into<coord_t>(r, math::cos(theta)), ry = mul_into<coord_t>(r, math::sin(theta));
        std::cout << "  (" << x << ", " << y << ") -> (" << r << ", " << theta << ") -> (" << rx << ", " << ry
                  << ")\n";
        if (math::abs(rx - x) > just<2, 16384> || math::abs(ry - y) > just<2, 16384>)
            return 1;
    }

    // 2. Range and bearing, metres at 1/256 m. The bearing runs from north
    //    clockwise, atan2(east, north), on a 2^-20 rad grid; degrees are a
    //    factor of π away, so value_cast rounds that once more, onto 0.1°.
    using pos_t     = quantity<si::metre, inside<{{-1024, 1024}, per<256>}, round_nearest>>;
    using fine_t    = inside<{{-4, 4}, per<1 << 20>}, round_nearest>;
    using compass_t = inside<{{-180, 180}, per<10>}, round_nearest>;
    std::cout << "\ntarget (east, north)   range       bearing\n";
    for (auto [e, n] : {std::pair{pos_t{30}, pos_t{40}}, {pos_t{0}, pos_t{50}}, {pos_t{-30}, pos_t{-40}}}) {
        const auto range   = units::hypot(e, n);
        const auto bearing = units::atan2_into<fine_t>(e, n);
        const auto deg     = value_cast<si::degree, compass_t>(bearing);
        std::cout << "  (" << e << ", " << n << ")" << "\t" << range << "\t" << bearing << " = " << deg << "\n";
        if (range != just<50> * si::metre) // 3-4-5 triangles: exact
            return 1;
    }
    if (to_string(value_cast<si::degree, compass_t>(units::atan2_into<fine_t>(pos_t{-30}, pos_t{-40}))) != "-143.1°")
        return 1;

    // 3. Inverse trig on [-1, 1]: angles in rad.
    using ratio_t = inside<{{-1, 1}, per<4096>}, round_nearest>;
    std::cout << "\nr      asin(r)    acos(r)\n";
    for (const ratio_t r : {ratio_t{-1}, ratio_t{-0.5}, ratio_t{0}, ratio_t{0.5}, ratio_t{1}})
        std::cout << "  " << r << "\t" << units::asin(r) << "\t" << units::acos(r) << "\n";
    if (units::asin(ratio_t{0}) != just<0> * si::radian || units::acos(ratio_t{1}) != just<0> * si::radian)
        return 1;
    return 0;
}
