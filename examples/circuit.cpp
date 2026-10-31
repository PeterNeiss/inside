// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Ohm's law with units on insides: resistor values in kΩ and Ω add exactly,
// a quotient is rounded only where the program says so, and a divisor that
// may be 0 Ω gives an error value instead of a garbage current.
//   1. Three resistors in series, in kΩ and Ω: the sum runs in Ω, exactly.
//   2. The current from a 9 V supply, rounded once onto a µA grid.
//   3. A potentiometer that can reach 0 Ω: the division is a std::expected.
//   4. The power V·I, a quantity in watts (V·A converts implicitly).
//   5. An RC low-pass: τ = R·C exactly, then the cutoff 1/(2πτ) in Hz.

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units/io.hpp>
#include <beman/inside/units/math.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
constexpr auto kohm = si::kilo(si::ohm);
constexpr auto mA   = si::milli(si::ampere);

using kohm_t  = quantity<kohm, inside<{{1, 100}, per<10>}>>;     // 0.1 kΩ steps, at least 1 kΩ
using ohm_t   = quantity<si::ohm, inside<{1, 1000}>>;            // whole ohms
using volts   = quantity<si::volt, inside<{1, 12}>>;             // a supply
using current = inside<{{0, 20}, per<1000>}, round_nearest>;     // mA to the µA
using hertz_t = inside<{{0, 100'000}, per<100>}, round_nearest>; // Hz to 0.01 Hz
} // namespace

int main() {
    // 1. 4.7 kΩ + 330 Ω + 1.2 kΩ: the common unit is the ohm, so the sum is
    //    6230 Ω on the exact hull of the three grids.
    const kohm_t r1{rational{47, 10}};
    const ohm_t  r2{330};
    const kohm_t r3{rational{12, 10}};
    const auto   r = r1 + r2 + r3;
    std::cout << "R      = " << r << '\n';
    if (to_string(r) != "6230 Ω")
        return 1;

    // 2. I = V / R. Every R is at least 1 kΩ, so the quotient is a plain value
    //    (V/Ω, exactly 9/6230 A); it rounds once, onto a µA grid in mA.
    const volts supply{9};
    const auto  i = value_cast<mA, current>(supply / r);
    std::cout << "I      = " << i << '\n'; // 1.445 mA
    if (to_string(i) != "1.445 mA")
        return 1;

    // 3. A potentiometer's grid holds 0 Ω: V / R can divide by zero, so the
    //    quotient is std::expected and the zero setting is an error value.
    const quantity<si::ohm, inside<{0, 10'000}>> pot{0};
    const auto                                   ip = supply / pot;
    if (ip)
        return 1;
    std::cout << "I(pot) = error: " << ip.error() << '\n';
    if (ip.error() != errc::division_by_zero)
        return 1;

    // 4. P = V·I: V·mA is a power, so it converts to watts (exactly: ×1/1000).
    const quantity<si::milli(si::watt), inside<{{0, 300}, per<1000>}>> p = supply * i;
    std::cout << "P      = " << p << '\n'; // 13.005 mW
    if (to_string(p) != "13.005 mW")
        return 1;

    // 5. τ = R·C with C = 100 nF: Ω·nF is a time, 0.623 ms exactly (printed to
    //    the notch the product carries, 1 Ω·nF = 10^-6 ms).
    const quantity<si::nano(si::farad), inside<{1, 1000}>> c{100};
    const auto                                             tau = (r * c).in(si::milli(si::second));
    std::cout << "tau    = " << tau << '\n';
    if (to_string(tau) != "0.623000 ms")
        return 1;
    //    The cutoff 1/(2πτ) is a frequency: 1/s names one, and the cast picks
    //    frequency (Hz) — the same 1/s as activity (Bq), which would not add
    //    to it. mag_pi keeps π in the unit, so the result rounds once, with
    //    the true π, onto a 0.01 Hz grid.
    const auto f  = just<1> / (just<2> * (mag_pi * tau));
    const auto fc = value_cast<si::hertz, hertz_t>(quantity_cast<isq::frequency>(f));
    std::cout << "f_c    = " << fc << '\n'; // 255.47 Hz
    if (to_string(fc) != "255.47 Hz")
        return 1;
    return 0;
}
