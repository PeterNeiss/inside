// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Physical units on insides (units.hpp, units_math.hpp): every conversion is
// exact, and the grid of every result is known at compile time.
//   1. A trip in legs of km and m: the total runs in the common unit (m),
//      exactly; the average speed in km/h.
//   2. Temperatures as points: 20 ℃ is 293.15 K and 68 ℉, on shifted grids.
//   3. Quantity kinds: width·height is an area; a height is a length, not
//      the reverse.
//   4. A bearing in degrees, rounded once into radians, then the east and
//      north components of a 1.5 km leg.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units_io.hpp>
#include <beman/inside/units_math.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
constexpr auto km = si::kilo(si::metre);

using leg_km  = quantity<km, inside<{{0, 500}, per<1000>}>>;     // a leg, to the metre
using minutes = quantity<si::minute, inside<{1, 600}>>;          // its duration
using celsius = inside<{{-50, 50}, per<10>}, round_nearest>;     // to 0.1 ℃
using angle   = inside<{{0, 7}, per<1'000'000>}, round_nearest>; // radians, 1e-6
} // namespace

int main() {
    // 1. Three legs: 12.5 km, then 800 m, then 3 km. The sum is in metres:
    //    the gcd of the units, so nothing rounds.
    const leg_km                                 a{12.5};
    const quantity<si::metre, inside<{0, 1000}>> b{800};
    const leg_km                                 c{3};
    const auto                                   total = a + b + c;
    std::cout << "total:   " << total << '\n'; // 16300 m
    if (to_string(total) != "16300 m")
        return 1;

    // The time is 20 min; the speed comes out in m/min and converts to km/h
    // exactly (the grid is rescaled, the number kept).
    const minutes t{20};
    const auto    v   = total / t;
    const auto    kmh = v.in(km / si::hour);
    std::cout << "speed:   " << kmh << '\n'; // 48.9 km/h
    if (to_string(kmh) != "48.9 km/h")
        return 1;

    // 2. 20 ℃ as a point, then counted from absolute zero in kelvin, and in ℉.
    const auto room = point<si::degree_Celsius>(celsius{20});
    const auto k    = room.point_for(si::absolute_zero).in(si::kelvin);
    const auto f    = room.point_for(international::zeroth_degree_Fahrenheit).in(international::degree_Fahrenheit);
    std::cout << "room:    " << room << " = " << k << " = " << f << '\n'; // 20.0 ℃ = 293.15 K = 68.00 ℉
    if (to_string(k) != "293.15 K" || to_string(f) != "68.00 ℉" || !(k == f))
        return 1;

    // 3. Kinds: width·height converts to an area; a height to a length.
    using width  = quantity<isq::width[si::metre], inside<{0, 20}>>;
    using height = quantity<isq::height[si::metre], inside<{0, 10}>>;
    const quantity<isq::area[square(si::metre)], inside<{0, 200}>> floor = width{6} * height{3};
    const quantity<isq::length[si::metre], inside<{0, 10}>>        l     = height{3};
    // height h = l;                  // would not compile: a length is not a height
    // auto bad = l + just<1> * si::second; // would not compile: another dimension
    std::cout << "wall:    " << floor << ", " << l << '\n';
    if (floor.numerical_value() != 18)
        return 1;

    // 4. A bearing of 30° (from north) rounds once into radians; the leg's
    //    components are 1.5 km · sin and · cos, each on a 1e-6 grid.
    const auto bearing = value_cast<si::radian, angle>(quantity<si::degree, inside<{0, 360}>>{30});
    const auto east    = just<1.5> * km * units::sin(bearing);
    const auto north   = just<1.5> * km * units::cos(bearing);
    std::cout << "bearing: " << bearing << ", east " << east.in(si::metre) << ", north " << north.in(si::metre)
              << '\n'; // 0.523599 rad, east 750.0000 m, north 1299.0375 m
    if (to_string(bearing) != "0.523599 rad" || to_string(east.in(si::metre)) != "750.0000 m")
        return 1;
    return 0;
}
