// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// sin and cos of huge arguments, correctly rounded:
// 10^18 rad is exact as an integer, and the engine reduces it by 2π exactly.
// The double route fails twice: 2^62 − 1 is no double, and fmod(x, 2π) uses a 2π off by 2^-51.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>

#include <inside/math.hpp>
#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units/io.hpp>
#include <inside/units/math.hpp>

using namespace ins;

int main()
{
  using radians = quantity<si::radian, inside<{0, (std::int64_t{1} << 62) - 1}>>; // whole rad up to 2^62 − 1
  struct row
  {
    radians      x;
    std::int64_t sin_slot, cos_slot; // the correctly rounded results × 2^52
  };
  const row rows[] = {
    {radians{1'000'000'000'000'000'000  , si::radian}, -4'471'936'262'876'823, 533'100'051'040'090   },
    {radians{(std::int64_t{1} << 62) - 1, si::radian}, 985'025'092'865'520   , -4'394'557'448'717'327},
  };
  for (const row& r : rows)
  {
    const radians x {r.x};
    const auto    s {math::sin<per<(std::int64_t{1} << 52)>>(x)}; // [-1, 1] on a 2^-52 grid
    const auto    c {math::cos<per<(std::int64_t{1} << 52)>>(x)};
    const double  xd{x.numerical_value_in(si::radian).to<double>()};

    // The results print exactly: a 2^-52 grid point has 52 decimals.
    std::cout << "x = " << x << "\n  sin                    " << s << "\n  cos                    " << c << "\n";
    std::printf("  std::sin(double)       %.16f\n  sin(fmod(x, 2*pi))     %.16f\n", std::sin(xd),
                std::sin(std::fmod(xd, 2 * 3.141592653589793)));

    constexpr auto scale{just<(std::int64_t{1} << 52)>}; // s · 2^52 is a whole number, exactly
    if (s * scale != r.sin_slot || c * scale != r.cos_slot)
      return 1;
  }
  return 0;
}
