// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Positions across the solar system to the nanometre:
// ±10^13 m (past Neptune) on a 10^-9 m grid is 2·10^22 points, a wide raw,
// exact where a double's 53 bits resolve only about 2 mm at that distance.

#include <cstdio>
#include <iostream>

#include <inside/math.hpp>
#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units/io.hpp>
#include <inside/units/math.hpp>

using namespace ins;

inline constexpr auto nm = per<1'000'000'000>;
using position_metres = quantity<si::metre, inside<{{-10'000'000'000'000, 10'000'000'000'000}, nm}>>; // ±10^13 m
using leg_metres      = quantity<si::metre, inside<{{-10'000'000        , 10'000'000        }, nm}>>; // ±10 000 km

int main()
{
  // 1. From Saturn's distance, a million legs of 1000.000000001 km, each a raw add of two 64-bit limbs;
  // the same sum in double loses the nanometre.
  const position_metres start{1'433'000'000'000, si::metre};
  const leg_metres      step = just<1000> * si::kilo(si::metre) + si::nano(si::metre); // 1e6 m + 1 nm
  // The accumulator's type cannot show that a run-time count of legs fits,
  // so it clamps at ±10^13 m (never here).
  using clamped_position = quantity<si::metre, inside<grid_of<position_metres::rep>, clamp>>;
  clamped_position x = start;
  double           xd{1.433e12};
  for (int i = 0; i < 1'000'000; ++i)
  {
    x  += step;
    xd += 1e6 + 1e-9;
  }
  std::cout << "exact   " << x << "\n";
  std::printf("double  %.9f m\n", xd);
  if (to_string(x) != "2433000000000.001000000 m") // 1.433e12 + 1e12 + 1e6·1e-9 m, to the nanometre
    return 1;

  // 2. The distance to 3·10^12 m east and 4·10^12 m north is exactly 5·10^12 m,
  // and hypot (to 1 mm) returns that.
  const position_metres east{3'000'000'000'000, si::metre}, north{4'000'000'000'000, si::metre};
  const auto            d{math::hypot<per<1000>>(east, north)};
  std::cout << "hypot   " << d << "\n";
  if (d != just<5'000'000'000'000> * si::metre)
    return 1;
  return 0;
}
