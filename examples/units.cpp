// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Physical units on insides: every conversion is exact, and the grid of every result is known at compile time.

#include <iostream>

#include <inside/inside.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>
#include <inside/units/math.hpp>

using namespace ins;

namespace {
constexpr auto km = si::kilo(si::metre);
} // namespace

int main() {
    // 1. Legs of 12.5 km, 800 m and 3 km: the sum runs in metres, the gcd of the units, so nothing rounds.
    using leg_km = quantity<km, inside<{{0, 500}, per<1000>}>>; // to the metre
    using leg_metres = quantity<si::metre, inside<{0, 1000}>>;
    const leg_km a{12.5, km};
    const leg_metres b{800, si::metre};
    const leg_km c{3, km};
    const auto total = a + b + c;
    std::cout << "total:   " << total << '\n'; // 16300 m
    if (to_string(total) != "16300 m")
        return 1;

    // 20 min: the speed comes out in m/min and converts to km/h exactly (the grid is rescaled, the number kept).
    using minutes = quantity<si::minute, inside<{1, 600}>>;
    const minutes t{20, si::minute};
    const auto v = total / t;
    const auto kmh = v.in(km / si::hour);
    std::cout << "speed:   " << kmh << '\n'; // 48.9 km/h
    if (to_string(kmh) != "48.9 km/h")
        return 1;

    // 2. 20 ℃ as a point: 293.15 K counted from absolute zero, and 68 ℉, on shifted grids.
    using celsius = inside<{{-50, 50}, per<10>}, round_half_away>; // to 0.1 ℃
    const auto room = point<si::degree_Celsius>(celsius{20});
    const auto k = room.point_for(si::absolute_zero).in(si::kelvin);
    const auto f = room.point_for(international::zeroth_degree_Fahrenheit).in(international::degree_Fahrenheit);
    std::cout << "room:    " << room << " = " << k << " = " << f << '\n'; // 20.0 ℃ = 293.15 K = 68.00 ℉
    if (to_string(k) != "293.15 K" || to_string(f) != "68.00 ℉" || !(k == f))
        return 1;

    // 3. Kinds: width·height converts to an area, and a height to a length, not the reverse.
    using width = quantity<isq::width[si::metre], inside<{0, 20}>>;
    using height = quantity<isq::height[si::metre], inside<{0, 10}>>;
    using square_metres = quantity<isq::area[square(si::metre)], inside<{0, 200}>>;
    using length_metres = quantity<isq::length[si::metre], inside<{0, 10}>>;
    const square_metres floor = width{6, isq::width[si::metre]} * height{3, isq::height[si::metre]};
    const length_metres l = height{3, isq::height[si::metre]};
    // height h = l;                        // would not compile: a length is not a height
    // auto bad = l + just<1> * si::second; // would not compile: another dimension
    std::cout << "wall:    " << floor << ", " << l << '\n';
    if (floor.numerical_value_in(square(si::metre)) != 18)
        return 1;

    // 4. A bearing of 30° from north rounds once into radians; the east and north components of a 1.5 km leg are
    //    1.5 km · sin and · cos, each on a 1e-6 grid.
    using degrees = quantity<si::degree, inside<{0, 360}>>;
    using radians = inside<{{0, 7}, per<1'000'000>}, round_half_away>;
    const auto bearing = value_cast<si::radian, radians>(degrees{30, si::degree});
    const auto east = just<1.5> * km * math::sin(bearing);
    const auto north = just<1.5> * km * math::cos(bearing);
    std::cout << "bearing: " << bearing << ", east " << east.in(si::metre) << ", north " << north.in(si::metre)
              << '\n'; // 0.523599 rad, east 750.0000 m, north 1299.0375 m
    if (to_string(bearing) != "0.523599 rad" || to_string(east.in(si::metre)) != "750.0000 m")
        return 1;
    return 0;
}
