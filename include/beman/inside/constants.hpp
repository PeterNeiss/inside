// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_CONSTANTS_HPP
#define BEMAN_INSIDE_CONSTANTS_HPP

#include <beman/inside/detail/config.hpp>

#include <beman/inside/inside.hpp>
#include <beman/inside/units.hpp>
#include <beman/inside/detail/math_constants.hpp>

//---------------------------------------------------------------------------
// constants.hpp — mathematical and physical constants.
//
// beman::inside::math            mathematical constants as point insides:
//                                 pi, e, sqrt2, ln2, phi, egamma, … as
//                                 rationals within 2^-57 of the value
//                                 (detail/math_constants.hpp; cmath.hpp has
//                                 them too).
// beman::inside::units::constants physical constants as quantities of their
//                                 ISQ kind, each the number 1 of the
//                                 constant's own unit, so products stay
//                                 exact and .in(u) gives the SI number.
//
// Exact: the SI 2019 defining constants and what follows from them (ħ, R, F,
// K_J, R_K, G₀, Φ₀, σ, c₁, c₂), standard gravity and the standard atmosphere.
// Measured: the CODATA 2022 recommended values (codata2022::), held exactly as
// published; their uncertainty is not represented.
//---------------------------------------------------------------------------
namespace beman::inside::units {
//---------------------------------------------------------------------------
// The constants' units. Each is a named unit whose magnitude is the constant,
// in the namespace of its source, next to si2019's defining constants.
//---------------------------------------------------------------------------
namespace si2019_defs {
inline constexpr named_unit standard_atmosphere{"atm", mag<101'325> * si::pascal};
inline constexpr named_unit reduced_planck_constant{
    "ħ", si2019::planck_constant / (mag<2> * mag_pi), {.ascii = "hbar"}};
inline constexpr named_unit molar_gas_constant{"R", si2019::avogadro_constant * si2019::boltzmann_constant};
inline constexpr named_unit faraday_constant{"F", si2019::avogadro_constant * si2019::elementary_charge};
inline constexpr named_unit josephson_constant{"K_J", mag<2> * si2019::elementary_charge / si2019::planck_constant};
inline constexpr named_unit von_klitzing_constant{"R_K", si2019::planck_constant / square(si2019::elementary_charge)};
inline constexpr named_unit conductance_quantum{
    "G₀", mag<2> * square(si2019::elementary_charge) / si2019::planck_constant, {.ascii = "G_0"}};
inline constexpr named_unit magnetic_flux_quantum{
    "Φ₀", si2019::planck_constant / (mag<2> * si2019::elementary_charge), {.ascii = "Phi_0"}};
inline constexpr named_unit stefan_boltzmann_constant{
    "σ",
    mag<2> * mag_pi * mag_pi * mag_pi * mag_pi * mag_pi * square(square(si2019::boltzmann_constant)) /
        (mag<15> * cubic(si2019::planck_constant) * square(si2019::speed_of_light_in_vacuum)),
    {.ascii = "sigma"}};
inline constexpr named_unit first_radiation_constant{
    "c₁", mag<2> * mag_pi * si2019::planck_constant * square(si2019::speed_of_light_in_vacuum), {.ascii = "c_1"}};
inline constexpr named_unit second_radiation_constant{
    "c₂", si2019::planck_constant * si2019::speed_of_light_in_vacuum / si2019::boltzmann_constant, {.ascii = "c_2"}};
} // namespace si2019_defs

namespace si2019 {
inline constexpr auto standard_atmosphere       = unit_c<si2019_defs::standard_atmosphere>;
inline constexpr auto reduced_planck_constant   = unit_c<si2019_defs::reduced_planck_constant>;
inline constexpr auto molar_gas_constant        = unit_c<si2019_defs::molar_gas_constant>;
inline constexpr auto faraday_constant          = unit_c<si2019_defs::faraday_constant>;
inline constexpr auto josephson_constant        = unit_c<si2019_defs::josephson_constant>;
inline constexpr auto von_klitzing_constant     = unit_c<si2019_defs::von_klitzing_constant>;
inline constexpr auto conductance_quantum       = unit_c<si2019_defs::conductance_quantum>;
inline constexpr auto magnetic_flux_quantum     = unit_c<si2019_defs::magnetic_flux_quantum>;
inline constexpr auto stefan_boltzmann_constant = unit_c<si2019_defs::stefan_boltzmann_constant>;
inline constexpr auto first_radiation_constant  = unit_c<si2019_defs::first_radiation_constant>;
inline constexpr auto second_radiation_constant = unit_c<si2019_defs::second_radiation_constant>;
} // namespace si2019

// CODATA 2022 recommended values (Mohr et al., Rev. Mod. Phys. 97, 025002).
namespace codata2022_defs {
inline constexpr named_unit gravitational_constant{
    "G", mag<rational{667'430, 10'000'000'000'000'000}> * cubic(si::metre) / (si::kilogram * square(si::second))};
inline constexpr named_unit fine_structure_constant{
    "α", mag<rational{72'973'525'643, 10'000'000'000'000}> * one, {.ascii = "alpha"}};
inline constexpr named_unit rydberg_constant{
    "R_∞", mag<rational{10'973'731'568'157, 1'000'000}> / si::metre, {.ascii = "R_inf"}};
inline constexpr named_unit vacuum_magnetic_permeability{"μ₀",
                                                         mag<rational{125'663'706'127, 100'000'000'000'000'000}> *
                                                             si::newton / square(si::ampere),
                                                         {.ascii = "mu_0"}};
inline constexpr named_unit characteristic_impedance_of_vacuum{
    "Z₀", mag<rational{376'730'313'412, 1'000'000'000}> * si::ohm, {.ascii = "Z_0"}};
inline constexpr named_unit electron_mass{"m_e", mag<9.1093837139e-31_g> * si::kilogram};
inline constexpr named_unit proton_mass{"m_p", mag<1.67262192595e-27_g> * si::kilogram};
inline constexpr named_unit neutron_mass{"m_n", mag<1.67492750056e-27_g> * si::kilogram};
inline constexpr named_unit atomic_mass_constant{"m_u", mag<1.66053906892e-27_g> * si::kilogram};
inline constexpr named_unit bohr_radius{"a₀", mag<5.29177210544e-11_g> * si::metre, {.ascii = "a_0"}};
inline constexpr named_unit vacuum_electric_permittivity{
    "ε₀", mag<8.8541878188e-12_g> * si::farad / si::metre, {.ascii = "epsilon_0"}};
} // namespace codata2022_defs

namespace codata2022 {
inline constexpr auto gravitational_constant             = unit_c<codata2022_defs::gravitational_constant>;
inline constexpr auto fine_structure_constant            = unit_c<codata2022_defs::fine_structure_constant>;
inline constexpr auto rydberg_constant                   = unit_c<codata2022_defs::rydberg_constant>;
inline constexpr auto vacuum_magnetic_permeability       = unit_c<codata2022_defs::vacuum_magnetic_permeability>;
inline constexpr auto characteristic_impedance_of_vacuum = unit_c<codata2022_defs::characteristic_impedance_of_vacuum>;
inline constexpr auto electron_mass                      = unit_c<codata2022_defs::electron_mass>;
inline constexpr auto proton_mass                        = unit_c<codata2022_defs::proton_mass>;
inline constexpr auto neutron_mass                       = unit_c<codata2022_defs::neutron_mass>;
inline constexpr auto atomic_mass_constant               = unit_c<codata2022_defs::atomic_mass_constant>;
inline constexpr auto bohr_radius                        = unit_c<codata2022_defs::bohr_radius>;
inline constexpr auto vacuum_electric_permittivity       = unit_c<codata2022_defs::vacuum_electric_permittivity>;
} // namespace codata2022

//---------------------------------------------------------------------------
// The constants as quantities: the number 1 of the constant's unit, under its
// ISQ kind where the kind is unambiguous.
//---------------------------------------------------------------------------
namespace detail {
template <reference R>
inline constexpr quantity<R, std::remove_cvref_t<decltype(just<1>)>> one_of{just<1>};
} // namespace detail

namespace constants {
// SI 2019 defining constants, standard gravity and the standard atmosphere.
inline constexpr auto hyperfine_structure_transition_frequency_of_cs =
    detail::one_of<isq::frequency[si2019::hyperfine_structure_transition_frequency_of_cs]>;
inline constexpr auto speed_of_light_in_vacuum = detail::one_of<isq::speed[si2019::speed_of_light_in_vacuum]>;
inline constexpr auto luminous_efficacy        = detail::one_of<si2019::luminous_efficacy>;
inline constexpr auto standard_gravity    = detail::one_of<isq::acceleration_of_free_fall[si2019::standard_gravity]>;
inline constexpr auto standard_atmosphere = detail::one_of<isq::pressure[si2019::standard_atmosphere]>;
inline constexpr auto planck_constant     = detail::one_of<isq::action[si2019::planck_constant]>;
inline constexpr auto reduced_planck_constant   = detail::one_of<isq::action[si2019::reduced_planck_constant]>;
inline constexpr auto elementary_charge         = detail::one_of<isq::electric_charge[si2019::elementary_charge]>;
inline constexpr auto boltzmann_constant        = detail::one_of<si2019::boltzmann_constant>;
inline constexpr auto avogadro_constant         = detail::one_of<si2019::avogadro_constant>;
inline constexpr auto molar_gas_constant        = detail::one_of<si2019::molar_gas_constant>;
inline constexpr auto faraday_constant          = detail::one_of<si2019::faraday_constant>;
inline constexpr auto josephson_constant        = detail::one_of<si2019::josephson_constant>;
inline constexpr auto von_klitzing_constant     = detail::one_of<isq::resistance[si2019::von_klitzing_constant]>;
inline constexpr auto conductance_quantum       = detail::one_of<isq::conductance[si2019::conductance_quantum]>;
inline constexpr auto magnetic_flux_quantum     = detail::one_of<isq::magnetic_flux[si2019::magnetic_flux_quantum]>;
inline constexpr auto stefan_boltzmann_constant = detail::one_of<si2019::stefan_boltzmann_constant>;
inline constexpr auto first_radiation_constant  = detail::one_of<si2019::first_radiation_constant>;
inline constexpr auto second_radiation_constant = detail::one_of<si2019::second_radiation_constant>;

// CODATA 2022 recommended values.
inline constexpr auto gravitational_constant       = detail::one_of<codata2022::gravitational_constant>;
inline constexpr auto fine_structure_constant      = detail::one_of<codata2022::fine_structure_constant>;
inline constexpr auto rydberg_constant             = detail::one_of<codata2022::rydberg_constant>;
inline constexpr auto vacuum_magnetic_permeability = detail::one_of<codata2022::vacuum_magnetic_permeability>;
inline constexpr auto characteristic_impedance_of_vacuum =
    detail::one_of<isq::impedance[codata2022::characteristic_impedance_of_vacuum]>;
inline constexpr auto electron_mass                = detail::one_of<isq::mass[codata2022::electron_mass]>;
inline constexpr auto proton_mass                  = detail::one_of<isq::mass[codata2022::proton_mass]>;
inline constexpr auto neutron_mass                 = detail::one_of<isq::mass[codata2022::neutron_mass]>;
inline constexpr auto atomic_mass_constant         = detail::one_of<isq::mass[codata2022::atomic_mass_constant]>;
inline constexpr auto bohr_radius                  = detail::one_of<isq::radius[codata2022::bohr_radius]>;
inline constexpr auto vacuum_electric_permittivity = detail::one_of<codata2022::vacuum_electric_permittivity>;
} // namespace constants
} // namespace beman::inside::units

#endif // BEMAN_INSIDE_CONSTANTS_HPP
