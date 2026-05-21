// PID controller with fixed-point error terms and a saturating output.
//
// Demonstrates:
//   - Fixed-point grids for error / integral / derivative / gain
//   - `add_all` to fold the three weighted terms into a single sum
//   - `clamp_round` to saturate the actuator command to its physical range
//   - `on_clamp` on the integrator to detect anti-wind-up saturation

#include <iostream>

#include "bound/bound.hpp"
#include "bound/print.hpp"

using namespace bnd;

// Sensor error in [-10, 10] with 1/16 resolution.
using err_t = bound<{{-10, 10}, notch<1, 16>}, round_nearest | clamp>;

// Integrator accumulates errors; clamp-saturating to prevent wind-up.
using integ_t = bound<{{-200, 200}, notch<1, 16>}, round_nearest | clamp>;

// Gain coefficients: [0, 4] with 1/256 step.
using gain_t = bound<{{0, 4}, notch<1, 256>}, round_nearest>;

// Actuator command in [-100, 100] integer steps, clamp-saturating.
using output_t = bound<{-100, 100}, clamp>;

struct pid
{
  gain_t kp{1.5};
  gain_t ki{0.125};
  gain_t kd{0.5};

  integ_t integral{0};
  err_t   previous{0};
  int     windup_events = 0;

  output_t step(err_t err)
  {
    // on_clamp fires when the integrator hits its saturation boundary —
    // classic wind-up indicator. The callback receives the overshoot so
    // we could derate the gain adaptively; here we just count events.
    // `policy_ref::operator+=` now takes a boundable RHS directly — no
    // need to drop to double for the integrator update.
    integral.on_clamp([&](auto& self, auto overshoot) {
      (void)self; (void)overshoot;
      ++windup_events;
    }) += err;

    // Three weighted terms — each is a bound on a wider grid than err.
    auto p_term = kp * err;
    auto i_term = ki * integral;
    auto d_term = kd * (err - previous);
    previous = err;

    // add_all folds variadically; each pairwise + widens the grid further.
    auto raw = add_all(p_term, i_term, d_term);

    // Cross the API boundary: convert the rational-friendly raw to a
    // double and saturate-round into the integer actuator grid.
    return clamp_round<output_t>(double(raw));
  }
};

int main()
{
  pid loop;

  // Disturbance series with a sustained bias to provoke wind-up.
  double errors[] = { 5.0, 4.5, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0,
                      2.0, 0.5, -1.0, -2.0 };

  std::cout << "err      integ     cmd\n";
  for (double e : errors)
  {
    err_t err{e};
    auto cmd = loop.step(err);
    std::cout << err << "    "
              << loop.integral << "   "
              << cmd << "\n";
  }
  std::cout << "\nintegrator saturation events: " << loop.windup_events << "\n";

  // Standalone demo of the clamp+round composite casts. These read more
  // naturally inside std::transform than `bound<G, clamp>{value}`.
  std::cout << "\nclamp_round<output_t>( 150.7) = "
            << clamp_round<output_t>(150.7) << "\n";
  std::cout << "clamp_round<output_t>(-200.4) = "
            << clamp_round<output_t>(-200.4) << "\n";
  std::cout << "clamp_floor<output_t>( -42.6) = "
            << clamp_floor<output_t>(-42.6) << "\n";
  std::cout << "clamp_ceil <output_t>( -42.6) = "
            << clamp_ceil <output_t>(-42.6) << "\n";

  return 0;
}
