// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// PID controller with fixed-point error terms and a saturating output.
//
// Demonstrates:
//   - Fixed-point grids for error / integral / derivative / gain
//   - `add_all` to fold the three weighted terms into a single sum
//   - `clamp | round_nearest` on the output type to saturate AND snap the
//     actuator command to its physical range in one `output_t{raw}` write
//   - `on_clamp` on the integrator to detect anti-wind-up saturation
//   - Units: a temperature loop. The error and the integral (summed once per
//     tick) are temperature differences in K, the gains are in %/K, and the
//     actuator command is a heater power in % — gain × error is a % by its
//     own arithmetic.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/formats.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

// Sensor error in [-10, 10] K with 1/16 K resolution.
using err_t = quantity<si::kelvin, inside<{{-10, 10}, per<16>}, round_nearest | clamp>>;

// Integrator accumulates errors; clamp-saturating to prevent wind-up.
using integ_t = quantity<si::kelvin, inside<{{-200, 200}, per<16>}, round_nearest | clamp>>;

// Gain coefficients: [0, 4] %/K with 1/256 step.
using gain_t = quantity<percent / si::kelvin, inside<{{0, 4}, per<256>}, round_nearest>>;

// Actuator command in [-100, 100] % integer steps. `clamp | round_nearest`
// lets `output_t{raw}` saturate AND round the wider input in one step — no
// explicit `clamp_cast<output_t, round_nearest>(...)` cast needed.
using output_t = quantity<percent, inside<{-100, 100}, clamp | round_nearest>>;

struct pid {
    gain_t kp{1.5};
    gain_t ki{0.125};
    gain_t kd{0.5};

    integ_t                  integral{0};
    err_t                    previous{0};
    clamp_counter<1'000'000> windup_events{0};

    output_t step(err_t err) {
        // on_clamp fires when the integrator hits its saturation boundary —
        // classic wind-up indicator. The callback receives the overshoot so
        // we could derate the gain adaptively; here we just count events.
        integral.on_clamp([&](auto&, auto) { ++windup_events; }) += err;

        // Three weighted terms — each a % on a wider grid than err.
        auto p_term = kp * err;
        auto i_term = ki * integral;
        auto d_term = kd * (err - previous);
        previous    = err;

        // add_all folds variadically; each pairwise + widens the grid further.
        auto raw = add_all(p_term, i_term, d_term);

        // Cross the API boundary: assignment into `output_t` saturates (clamp)
        // and snaps to its integer notch (round_nearest) via the type's policy.
        return output_t{raw};
    }
};

int main() {
    pid loop;

    // Disturbance series with a sustained bias to provoke wind-up.
    double errors[] = {5.0, 4.5, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 2.0, 0.5, -1.0, -2.0};

    std::cout << "err      integ     cmd\n";
    for (double e : errors) {
        err_t err{e};
        auto  cmd = loop.step(err);
        std::cout << err << "    " << loop.integral << "   " << cmd << "\n";
    }
    std::cout << "\nintegrator saturation events: " << loop.windup_events << "\n";

    if (to_string(loop.integral) != "41 K" || to_string(loop.step(err_t{0})) != "6 %")
        return 1;
    return 0;
}
