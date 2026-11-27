// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Polar coordinates, range and bearing with hypot, atan2, asin and acos:
// every result is the correctly rounded point of its output grid, the same on every platform.
// atan2 of two lengths is an angle in rad, and so are asin and acos of a ratio; named in degrees, it rounds once there.

#include <iostream>

#include <inside/math.hpp>
#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units/io.hpp>
#include <inside/units/math.hpp>

using namespace ins;

int main()
{
  // 1. A polar round trip on a 1 m square to 2^-14 m, theta onto 2^-14 rad with its range [-π, π] deduced.
  constexpr auto m = si::metre;
  using coordinate = id<"[-1..1] 1/16384 [m] round_half_away">;
  std::cout << "(x, y) -> (r, theta) -> (x, y)\n";
  for (auto [x, y] : {std::pair{coordinate{1, m}         , coordinate{0, m}         },
                               {coordinate{-1, m}        , coordinate{1, m}         },
                               {coordinate{just<3, 5>, m}, coordinate{just<4, 5>, m}},
                               {coordinate{0.5, m}       , coordinate{-0.5, m}      }})
  {
    const auto r    {math::hypot(x, y)};
    const auto theta{math::atan2<per<(1 << 14)>>(y, x)};
    // r and cos θ are each rounded, so their product can land a notch past the range:
    // clamp says where it goes.
    const coordinate rx{mul<coordinate>(r, math::cos(theta), clamp)};
    const coordinate ry{mul<coordinate>(r, math::sin(theta), clamp)};
    std::cout << "  (" << x << ", " << y << ") -> (" << r << ", " << theta << ") -> (" << rx << ", " << ry
              << ")\n";
    if (math::abs(rx - x) > just<2, (1 << 14)> * m || math::abs(ry - y) > just<2, (1 << 14)> * m)
      return 1;
  }

  // 2. Range and compass bearing (from north, clockwise) on a local east/north plane in metres.
  // The bearing atan2(east, north) goes straight into degrees, rounded once onto 0.1°.
  using metres  = id<"[-1024..1024] 1/256 [m] round_half_away">;
  using bearing = id<"[-180..180] 1/10 [deg] round_half_away">;
  std::cout << "\ntarget (east, north)   range       bearing\n";
  for (auto [e, n] : {std::pair{metres{30 , si::metre}, metres{40 , si::metre}},
                               {metres{0  , si::metre}, metres{50 , si::metre}},
                               {metres{-30, si::metre}, metres{-40, si::metre}}})
  {
    const auto range{math::hypot(e, n)};
    const auto to   {math::atan2<bearing>(e, n)};
    std::cout << "  (" << e << ", " << n << ")" << "\t" << range << "\t" << to << "\n";
    if (range != just<50> * si::metre) // 3-4-5 triangles: exact
      return 1;
  }
  const auto south_west{math::atan2<bearing>(metres{-30, si::metre}, metres{-40, si::metre})};
  if (to_string(south_west) != "-143.1°")
    return 1;

  // 3. asin and acos recover an angle in rad from a ratio in [-1, 1], a dimensionless quantity (a sine: m/m).
  using ratio = id<"[-1..1] 1/4096 [1] round_half_away">;
  std::cout << "\nr      asin(r)    acos(r)\n";
  for (const ratio r : {ratio{-1, one}, ratio{-0.5, one}, ratio{0, one}, ratio{0.5, one}, ratio{1, one}})
    std::cout << "  " << r << "\t" << math::asin(r) << "\t" << math::acos(r) << "\n";
  if (math::asin(ratio{0, one}) != just<0> * si::radian || math::acos(ratio{1, one}) != just<0> * si::radian)
    return 1;
  return 0;
}
