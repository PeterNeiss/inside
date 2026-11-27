// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Physical units on insides: every conversion is exact, and the grid of every result is known at compile time;
// text reads back with its unit, in any unit that converts exactly.

#include <iostream>

#include <inside/inside.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>
#include <inside/units/math.hpp>

using namespace ins;

namespace
{
  constexpr auto km = si::kilo(si::metre);
} // namespace

int main()
{
  // 1. Legs of 12.5 km, 800 m and 3 km: the sum runs in metres, the gcd of the units, so nothing rounds.
  using leg_km     = id<"[0..500] 1/1000 [km]">; // to the metre
  using leg_metres = id<"[0..1000] [m]">;

  const leg_km     a    {12.5, km};
  const leg_metres b    {800 , si::metre};
  const leg_km     c    {3   , km};
  const auto       total{a + b + c};
  std::cout << "total:   " << total << '\n'; // 16300 m
  if (to_string(total) != "16300 m")
    return 1;

  // 20 min: the speed comes out in m/min and converts to km/h exactly
  // (the grid is rescaled, the number kept).
  using minutes = id<"[1..600] [min]">;
  const minutes t  {20, si::minute};
  const auto    v  {total / t};
  const auto    kmh{v.in(km / si::hour)};
  std::cout << "speed:   " << kmh << '\n'; // 48.9 km/h
  if (to_string(kmh) != "48.9 km/h")
    return 1;

  // 2. 20 ℃ as a point: 293.15 K counted from absolute zero, and 68 ℉, on shifted grids.
  using celsius = id<"[-50..50] 1/10 [`C] @ice_point round_half_away">;
  const celsius room{20, si::degree_Celsius}; // to 0.1 ℃
  const auto k   {room.point_for(si::absolute_zero).in(si::kelvin)};
  const auto f   {room.point_for(international::zeroth_degree_Fahrenheit).in(international::degree_Fahrenheit)};
  std::cout << "room:    " << room << " = " << k << " = " << f << '\n'; // 20.0 ℃ = 293.15 K = 68.00 ℉
  if (to_string(k) != "293.15 K" || to_string(f) != "68.00 ℉" || !(k == f))
    return 1;

  // 3. Kinds: width·height converts to an area, and a height to a length, not the reverse.
  using width         = id<"[0..20] [m]{width}">;
  using height        = id<"[0..10] [m]{height}">;
  using square_metres = id<"[0..200] [m^2]{area}">;
  using length_metres = id<"[0..10] [m]{length}">;
  const square_metres floor = width{6, isq::width[si::metre]} * height{3, isq::height[si::metre]};
  const length_metres l     = height{3, isq::height[si::metre]};
  // height h = l;                        // would not compile: a length is not a height
  // auto bad = l + si::second; // would not compile: another dimension
  std::cout << "wall:    " << floor << ", " << l << '\n';
  if (floor.numerical_value_in(square(si::metre)) != 18)
    return 1;

  // 4. A bearing of 30° from north rounds once into radians;
  // the east and north components of a 1.5 km leg are 1.5 km · sin and · cos, each on a 1e-6 grid.
  using degrees    = id<"[0..360] [deg]">;
  using millionths = id<"[0..7] 1/1'000'000 round_half_away">;
  const auto bearing{value_cast<si::radian, millionths>(degrees{30, si::degree})};
  const auto east   {just<1.5> * km * math::sin(bearing)};
  const auto north  {just<1.5> * km * math::cos(bearing)};
  std::cout << "bearing: " << bearing << ", east " << east.in(si::metre) << ", north " << north.in(si::metre)
            << '\n'; // 0.523599 rad, east 750.0000 m, north 1299.0375 m
  if (to_string(bearing) != "0.523599 rad" || to_string(east.in(si::metre)) != "750.0000 m")
    return 1;

  // 5. Text names its unit, any that converts exactly: 1.5 h and 90 s into minutes, 36 km/h into m/s,
  // 350 ℉ into a ℃ point. A bare number, another dimension or a unit nothing defines is an error, not a guess.
  using read_minutes = id<"[0..600] 1/10 [min]">;
  using read_speed   = id<"[0..100] 1/100 [m/s]">;
  using oven         = id<"[0..300] <9> [`C] @ice_point">;
  const auto hours  {parse<read_minutes>("1.5 h")};
  const auto seconds{parse<read_minutes>("90 s")};
  const auto pace   {parse<read_speed>("36 km/h")};
  const auto baking {parse<oven>("350 ℉")};
  std::cout << "read:    " << *hours << ", " << *seconds << ", " << *pace << ", " << *baking
            << '\n'; // 90.0 min, 1.5 min, 10.00 m/s, 530/3 ℃
  std::cout << "errors:  " << error_message(parse<read_minutes>("90").error()) << "; "
            << error_message(parse<read_minutes>("90 m").error()) << "; "
            << error_message(parse<read_minutes>("90 zz").error()) << '\n';
  if (to_string(*hours) != "90.0 min" || to_string(*seconds) != "1.5 min" || to_string(*pace) != "10.00 m/s"
      || to_string(*baking) != "530/3 ℃" || parse<read_minutes>("90").error() != error::missing_unit)
    return 1;
  return 0;
}
