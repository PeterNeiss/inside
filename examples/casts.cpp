// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// One-shot stores whose intent reads at the call site: the flag constructor B{v, F}
// (clamp, wrap, unsafe and a rounding direction, replacing B's own flags of that kind),
// at home in a std::ranges::transform lambda.

#include <algorithm>
#include <iostream>
#include <ranges>
#include <vector>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;

int main()
{
  // A coarse grid, 0..10 in steps of 2, so the three rounding directions differ visibly.
  using coarse = inside<{{0, 10}, 2}>;

  std::vector<double> raw = {-1.0, 2.4, 3.5, 7.9, 12.0};
  std::vector<coarse> down(raw.size()), up(raw.size()), nearest(raw.size());

  // clamp with a rounding mode, in transform.
  // A run-time double may be NaN, which no clamp places:
  // the policy reports it into ec (the store leaves 0).
  error ec{};
  std::ranges::transform(raw, down.begin(),
                         [&](double v) { return coarse{v, make_policy<clamp | round_floor>(ec)}; });
  std::ranges::transform(raw, up.begin(),
                         [&](double v) { return coarse{v, make_policy<clamp | round_ceil>(ec)}; });
  std::ranges::transform(raw, nearest.begin(),
                         [&](double v) { return coarse{v, make_policy<clamp | round_half_away>(ec)}; });
  if (ec != error{})
  {
    std::cout << "a raw value is not a number\n";
    return 1;
  }

  std::cout << "coarse{v, clamp | round_*} into inside<{{0,10},2}> (out-of-range saturates):\n";
  std::cout << "   raw     floor   ceil    round\n";
  for (std::size_t i = 0; i < raw.size(); ++i)
    std::cout << "  " << raw[i] << "\t " << down[i] << "\t " << up[i] << "\t " << nearest[i] << "\n";

  // wrap: a hue angle folded back into [0°, 359°].
  using hue = quantity<si::degree, inside<{0, 359}>>;
  std::cout << "\nhue{370°, wrap} = " << hue{370, si::degree, wrap} << "   hue{-30°, wrap} = "
            << hue{-30, si::degree, wrap} << "\n";

  // clamp saturates whatever the target's own policy;
  // unsafe trusts the caller and checks nothing (UB if out).
  using percentage = quantity<percent, inside<{0, 100}>>;
  std::cout << "percentage{150 %, clamp} = "   << percentage{150, percent, clamp}
            << "   percentage{-5 %, clamp} = " << percentage{-5, percent, clamp} << "\n";
  std::cout << "percentage{42 %, unsafe} = " << percentage{42, percent, unsafe}
            << "   (trusted — no runtime check)\n";

  // The same intents for one store on a strict inside: x.with<F>() opts in for that write only.
  std::cout << "\nper-operation overrides on a strict inside<{0,100}>:\n";
  inside<{0, 100}> q{0};
  q.with<round_floor>() = 3.7;
  std::cout << "  with<round_floor>()       = 3.7  -> " << q << "\n";
  q.with<round_ceil>() = 3.2;
  std::cout << "  with<round_ceil>()        = 3.2  -> " << q << "\n";
  q.with<round_toward_zero>() = 9.9;
  std::cout << "  with<round_toward_zero>() = 9.9  -> " << q << "\n";
  q.with<round_half_even>() = 2.5;
  std::cout << "  with<round_half_even>()   = 2.5  -> " << q << "\n";

  quantity<si::degree, inside<{0, 359}>> angle{0, si::degree}; // a quantity takes the same one-shot flags
  angle.with<wrap>() = just<400> * si::degree;
  std::cout << "  with<wrap>()              = 400° -> " << angle << "\n";
  return 0;
}
