// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A flight to the Moon with units on insides: the masses add exactly across
// t and kg, every irrational step (log, sqrt, π) rounds once onto a grid the
// program names, and the quantity kinds keep an energy from becoming a
// torque.
//   1. The stage's masses in t and kg: m0 and the mass after the burn, in kg.
//   2. The Δv the stage can give (rocket equation, Δv = v_e·ln(m0/mf)): the
//      mass ratio is a plain number.
//   3. Parking orbit at 185 km: altitude + Earth radius is a length; the
//      circular speed √(μ/r) takes the unit's root, km²/s² → km/s.
//   4. Trans-lunar injection: the vis-viva speed at perigee (the grids prove
//      its square positive, so the sqrt is a plain value), the Δv it needs,
//      and the time of the half orbit to the Moon, π·a·√(a/μ) — π stays in
//      the unit until the one rounding.
//   5. The spacecraft's kinetic energy after the burn (mass·speed² is a
//      kinetic energy), and its initial acceleration in g₀.

#include <cstdint>
#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/units_math.hpp>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
constexpr auto km   = si::kilo(si::metre);
constexpr auto km_s = km / si::second;

template <std::uint64_t D>
using fine = inside<{{0, 200}, per<D>}, round_nearest>; // a value in [0, 200] rounded to 1/D
} // namespace

int main() {
    // 1. Dry mass 15 t, payload 4500 kg, propellant 85 t. The sums run in kg.
    const quantity<si::tonne, inside<{1, 50}>>        dry{15};
    const quantity<si::kilogram, inside<{0, 10'000}>> payload{4500};
    const quantity<si::tonne, inside<{0, 200}>>       propellant{85};
    const auto                                        m_final = dry + payload;
    const auto                                        m0      = m_final + propellant;
    std::cout << "m0        = " << m0 << ", after the burn " << m_final << '\n';
    if (to_string(m0) != "104500 kg" || to_string(m_final) != "19500 kg")
        return 1;

    // 2. m0 / mf is a number (kg/kg): exactly 209/39. Its log rounds once onto
    //    a 1e-6 grid, and v_e = 3.1 km/s scales it into a Δv.
    const inside<{{1, 300}, 0}>                     ratio = m0 / m_final;
    const auto                                      ln    = math::log_into<fine<1'000'000>>(ratio);
    const quantity<km_s, inside<{{1, 5}, per<10>}>> v_e{rational{31, 10}};
    const auto                                      dv_stage = v_e * ln;
    std::cout << "dv stage  = " << dv_stage.in(si::metre / si::second) << '\n';
    if (to_string(dv_stage) != "5.2041963 km/s")
        return 1;

    // 3. Altitude + radius: their common kind is length. μ of the Earth is
    //    exact (398600.4418 km³/s²); √(μ/r) rounds once onto a 1e-6 km/s grid.
    const quantity<isq::altitude[km], inside<{100, 1000}>> altitude{185};
    const quantity<isq::radius[km], inside<{6300, 6400}>>  earth{6371};
    const auto                                             r = altitude + earth;
    static_assert(decltype(r)::reference.Spec->Name == "length");
    constexpr auto mu     = just<rational{3'986'004'418, 10'000}> * (cubic(km) / square(si::second));
    const auto     v_park = units::sqrt_into<fine<1'000'000>>(mu / r);
    std::cout << "v parking = " << v_park << '\n';
    if (to_string(v_park) != "7.797393 km/s")
        return 1;

    // 4. The transfer ellipse reaches the Moon's distance: a = (r + 384400 km)/2,
    //    exactly. Vis-viva at perigee, v² = 2μ/r − μ/a: the grids of r and a
    //    prove the difference positive, so its sqrt is a plain value (a grid
    //    that reached below 0 would make sqrt_into return an expected).
    const auto a      = (r + just<384'400> * km) / just<2>;
    const auto v_peri = units::sqrt_into<fine<1'000'000>>(just<2> * mu / r - mu / a);
    const auto dv_tli = v_peri - v_park;
    std::cout << "v perigee = " << v_peri << ", TLI needs " << dv_tli << '\n';
    if (to_string(v_peri) != "10.934330 km/s" || to_string(dv_tli) != "3.136937 km/s" || !(dv_stage > dv_tli))
        return 1;
    //    Half the ellipse's period, π·a·√(a/μ): √(s²/km²) is s/km, so
    //    a·√(a/μ) is a time; mag_pi puts π in the unit, and the cast to hours
    //    rounds once with the true π.
    const auto rt   = units::sqrt_into<fine<1'000'000'000>>(a / mu);
    const auto t_h  = value_cast<si::hour, inside<{{0, 200}, per<10>}, round_nearest>>(mag_pi * (a * rt));
    const auto days = value_cast<si::day, inside<{{0, 10}, per<100>}, round_nearest>>(mag_pi * (a * rt));
    std::cout << "transfer  = " << t_h << " (" << days << ")\n";
    if (to_string(t_h) != "119.5 h" || to_string(days) != "4.98 d")
        return 1;

    // 5. Kinetic energy after the burn, m·v² with v to 1 m/s: mass·speed² is a
    //    kinetic energy by its recipe, so it converts implicitly (in GJ). A
    //    torque has the same dimension but another recipe:
    //      quantity<isq::torque[si::newton * si::metre], …> τ = m_final * v * v;   // does not compile
    using gj_t   = inside<{{0, 10'000}, per<1000>}, round_nearest>;
    const auto v = value_cast<km_s, inside<{{0, 20}, per<1000>}, round_nearest>>(quantity_cast<isq::speed>(v_peri));
    const quantity<isq::kinetic_energy[si::giga(si::joule)], gj_t> ke =
        value_cast<si::giga(si::joule), gj_t>(m_final * v * v);
    std::cout << "KE        = " << ke << '\n';
    if (to_string(ke) != "2331.271 GJ")
        return 1;

    //    110 kN on 104.5 t, in units of standard gravity (exact: g₀ is rational).
    const quantity<si::kilo(si::newton), inside<{1, 1000}>> thrust{110};
    const auto acc = value_cast<si2019::standard_gravity, inside<{{0, 10}, per<1000>}, round_nearest>>(thrust / m0);
    std::cout << "a0        = " << acc << '\n';
    if (to_string(acc) != "0.107 g₀")
        return 1;
    return 0;
}
