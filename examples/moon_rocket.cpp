// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// A flight to the Moon: masses add exactly across t and kg,
// every irrational step (log, sqrt, π) rounds once onto a notch the program names,
// and the quantity kinds keep an energy from becoming a torque.

#include <iostream>

#include <inside/inside.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>
#include <inside/units/math.hpp>

using namespace ins;

namespace
{
  constexpr auto km   = si::kilo(si::metre);
  constexpr auto km_s = km / si::second;
} // namespace

int main()
{
  // 1. The stage: dry mass 15 t, payload 4500 kg, propellant 85 t. The sums run in kg.
  using dry_tonnes        = quantity<si::tonne   , inside<{1, 50    }>>;
  using kilograms         = quantity<si::kilogram, inside<{0, 10'000}>>;
  using propellant_tonnes = quantity<si::tonne   , inside<{0, 200   }>>;

  const dry_tonnes        dry       {15  , si::tonne};
  const kilograms         payload   {4500, si::kilogram};
  const propellant_tonnes propellant{85  , si::tonne};
  const auto              m_final   {dry + payload};
  const auto              m0        {m_final + propellant};
  std::cout << "m0        = " << m0 << ", after the burn " << m_final << '\n';
  if (to_string(m0) != "104500 kg" || to_string(m_final) != "19500 kg")
    return 1;

  // 2. The rocket equation, Δv = v_e·ln(m0/mf):
  // m0/mf is a plain number, exactly 209/39 (a denominator up to 60000, mf in kg).
  // Over the masses' grids it can pass 300, so it is checked into the ratio's range.
  using mass_ratio = inside<{{1, 300}, max_denominator{60'000}}>;
  using km_per_s   = quantity<km_s, inside<{{1, 5}, per<10>}>>;
  const auto checked_ratio{mass_ratio::try_make((m0 / m_final).numerical_value_in(one))};
  if (!checked_ratio)
    return 1;
  const auto     ratio   {*checked_ratio};
  const auto     ln      {math::log<per<1'000'000>>(ratio)}; // rounded once onto 1e-6
  const km_per_s v_e     {just<31, 10> * km_s};
  const auto     dv_stage{v_e * ln};
  std::cout << "dv stage  = " << dv_stage.in(si::metre / si::second) << '\n';
  if (to_string(dv_stage) != "5.2041963 km/s")
    return 1;

  // 3. A parking orbit at 185 km: altitude + radius is a length,
  // and √(μ/r) takes the unit's root (km²/s² → km/s), rounded once onto 1e-6 km/s.
  // μ of the Earth is exact.
  using altitude_km = quantity<isq::altitude[km], inside<{100 , 1000}>>;
  using radius_km   = quantity<isq::radius[km]  , inside<{6300, 6400}>>;

  const altitude_km altitude{185 , isq::altitude[km]};
  const radius_km   earth   {6371, isq::radius[km]};
  const auto        r       {altitude + earth};
  static_assert(r.kind == isq::length);
  constexpr auto mu    {just<3'986'004'418, 10'000> * (cubic(km) / square(si::second))};
  const auto     v_park{math::sqrt<per<1'000'000>>(mu / r)};
  std::cout << "v parking = " << v_park << '\n';
  if (to_string(v_park) != "7.797393 km/s")
    return 1;

  // 4. Trans-lunar injection: a = (r + 384400 km)/2 exactly, and vis-viva v² = 2μ/r − μ/a at perigee.
  // The grids prove v² positive, so its sqrt is a plain value (sqrt takes no grid that reaches below 0).
  const auto a     {(r + just<384'400> * km) / just<2>};
  const auto v_peri{math::sqrt<per<1'000'000>>(just<2> * mu / r - mu / a)};
  const auto dv_tli{v_peri - v_park};
  std::cout << "v perigee = " << v_peri << ", TLI needs " << dv_tli << '\n';
  if (to_string(v_peri) != "10.934330 km/s" || to_string(dv_tli) != "3.136937 km/s" || !(dv_stage > dv_tli))
    return 1;

  // Half the ellipse's period, π·a·√(a/μ): √(s²/km²) is s/km, so a·√(a/μ) is a time;
  // pi keeps π in the unit until the cast to hours rounds once with the true π.
  using tenths     = inside<{{0, 200}, per<10> }, round_half_away>;
  using hundredths = inside<{{0, 10 }, per<100>}, round_half_away>;
  const auto rt {math::sqrt<per<1'000'000'000>>(a / mu)};
  const auto t_h{value_cast<si::hour, tenths>(pi * (a * rt))};
  const auto t_d{value_cast<si::day, hundredths>(pi * (a * rt))};
  std::cout << "transfer  = " << t_h << " (" << t_d << ")\n";
  if (to_string(t_h) != "119.5 h" || to_string(t_d) != "4.98 d")
    return 1;

  // 5. Kinetic energy after the burn, m·v² with v to 1 m/s:
  // mass·speed² is a kinetic energy by its recipe, so it converts implicitly (in GJ).
  // A torque has the same dimension but another recipe:
  //   quantity<isq::torque[si::newton * si::metre], …> τ = m_final * v * v; // does not compile
  // (A value_cast into a grid that may not hold the value is an expected.)
  using speed          = quantity<km_s, inside<{{0, 20}, per<1000>}, round_half_away>>;
  using kinetic_energy = quantity<isq::kinetic_energy[si::giga(si::joule)],
                                  inside<{{0, 10'000}, per<1000>}, round_half_away>>;
  const auto v         {value_cast<speed>(kind_cast<isq::speed>(v_peri))};
  const auto checked_ke{value_cast<si::giga(si::joule), kinetic_energy::rep>(m_final * v * v)};
  if (!checked_ke)
    return 1;
  const kinetic_energy ke = *checked_ke;
  std::cout << "KE        = " << ke << '\n';
  if (to_string(ke) != "2331.271 GJ")
    return 1;

  // 110 kN on 104.5 t, in units of standard gravity (exact: g₀ is rational).
  using kilonewtons  = quantity<si::kilo(si::newton), inside<{1, 1000}>>;
  using acceleration = quantity<si2019::standard_gravity, inside<{{0, 10}, per<1000>}, round_half_away>>;
  const kilonewtons thrust{110, si::kilo(si::newton)};
  const auto        acc   {value_cast<acceleration>(thrust / m0)};
  if (!acc)
    return 1;
  std::cout << "a0        = " << *acc << '\n';
  if (to_string(*acc) != "0.107 g₀")
    return 1;
  return 0;
}
