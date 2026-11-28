// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A PID temperature loop: errors and their integral in K, gains in %/K,
// and a heater command in % whose clamp and rounding policy saturates and snaps the summed terms in one store;
// on_clamp on the integrator counts wind-up.

#include <iostream>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>
#include <inside/inside/formats.hpp>

using namespace ins;
using namespace ins::formats;

using error_kelvins    = id<"[-10..10] 1/16 [K] round_half_away|clamp">;                                // 1/16 K
using integral_kelvins = id<"[-200..200] 1/16 [K] round_half_away|clamp">;                              // anti-wind-up

using percent_per_kelvin = id<"[0..4] 1/256 [%/K] round_half_away">;
using command_percent    = id<"[-100..100] [%] clamp|round_half_away">; // saturates and rounds

namespace
{
  inline constexpr auto windup = base_unit<"windup", "integrator_saturations">; // one step at the integrator's limit
} // namespace

struct pid
{
  percent_per_kelvin kp{1.5  , percent / si::kelvin};
  percent_per_kelvin ki{0.125, percent / si::kelvin};
  percent_per_kelvin kd{0.5  , percent / si::kelvin};

  integral_kelvins                           integral     {0, si::kelvin};
  error_kelvins                              previous     {0, si::kelvin};
  id<quantity_key<windup, clamp_counter<1'000'000>>> windup_events{0, windup};

  command_percent step(error_kelvins err)
  {
    // The integrator at its boundary is the classic wind-up sign;
    // on_clamp gets the overshoot, here just counted.
    integral.on_clamp([&](auto&, auto) { ++windup_events; }) += err;

    // Three weighted terms, each a % on a wider grid than err; add_all folds them, widening further.
    auto p_term{kp * err};
    auto i_term{ki * integral};
    auto d_term{kd * (err - previous)};
    previous = err;
    auto raw{add_all(p_term, i_term, d_term)};

    return command_percent{raw}; // the type's policy saturates and snaps to whole percent
  }
};

int main()
{
  pid loop;
  double errors[] = {5.0, 4.5, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 2.0, 0.5, -1.0, -2.0}; // a sustained bias

  std::cout << "err      integ     cmd\n";
  for (double e : errors)
  {
    const auto stored{error_kelvins::try_make(e, si::kelvin)}; // a NaN would be its error
    if (!stored)
      return 1;
    const error_kelvins err{*stored};
    auto                cmd{loop.step(err)};
    std::cout << err << "    " << loop.integral << "   " << cmd << "\n";
  }
  std::cout << "\nintegrator saturation events: " << loop.windup_events << "\n";

  if (to_string(loop.integral) != "41 K" || to_string(loop.step(error_kelvins{0, si::kelvin})) != "6 %")
    return 1;
  return 0;
}
