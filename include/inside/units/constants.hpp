// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INSIDE_UNITS_CONSTANTS_HPP
#define INSIDE_UNITS_CONSTANTS_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/inside.hpp>
#include <inside/units.hpp>

// units/constants.hpp: physical constants in ins::constants, quantities of their ISQ kind, each the number 1 of the
// constant's own unit, so products stay exact and .in(u) gives the SI number.
// The mathematical constants (pi, e, …) are in <inside/math/constants.hpp>, without units.
//
// Exact: the SI 2019 defining constants and what follows from them (ħ, R, F, K_J, R_K, G₀, Φ₀, σ, c₁, c₂),
// standard gravity and the standard atmosphere.
// Measured: the CODATA 2022 recommended values (codata2022::), held exactly as published;
// their uncertainty is not represented.
namespace ins
{
  // The constants' units. Each is a named unit whose magnitude is the constant, in the namespace of its source,
  // next to si2019's defining constants.
  namespace si2019
  {
    inline constexpr unit_def standard_atmosphere_def{"atm", detail::mag<101'325> * si::pascal};
    inline constexpr auto     standard_atmosphere = defined_by<standard_atmosphere_def>;
    inline constexpr unit_def reduced_planck_constant_def{"ħ",
                                                          si2019::planck_constant / (detail::mag<2> * detail::mag_pi),
                                                          {.ascii = "hbar"}};
    inline constexpr auto     reduced_planck_constant = defined_by<reduced_planck_constant_def>;
    inline constexpr unit_def molar_gas_constant_def{"R", si2019::avogadro_constant * si2019::boltzmann_constant};
    inline constexpr auto     molar_gas_constant = defined_by<molar_gas_constant_def>;
    inline constexpr unit_def faraday_constant_def{"F", si2019::avogadro_constant * si2019::elementary_charge};
    inline constexpr auto     faraday_constant = defined_by<faraday_constant_def>;
    inline constexpr unit_def josephson_constant_def{
      "K_J", detail::mag<2> * si2019::elementary_charge / si2019::planck_constant};
    inline constexpr auto     josephson_constant = defined_by<josephson_constant_def>;
    inline constexpr unit_def von_klitzing_constant_def{"R_K",
                                                        si2019::planck_constant / square(si2019::elementary_charge)};
    inline constexpr auto     von_klitzing_constant = defined_by<von_klitzing_constant_def>;
    inline constexpr unit_def conductance_quantum_def{
      "G₀", detail::mag<2> * square(si2019::elementary_charge) / si2019::planck_constant, {.ascii = "G_0"}};
    inline constexpr auto     conductance_quantum = defined_by<conductance_quantum_def>;
    inline constexpr unit_def magnetic_flux_quantum_def{
      "Φ₀", si2019::planck_constant / (detail::mag<2> * si2019::elementary_charge), {.ascii = "Phi_0"}};
    inline constexpr auto     magnetic_flux_quantum = defined_by<magnetic_flux_quantum_def>;
    inline constexpr unit_def stefan_boltzmann_constant_def{
      "σ",
      detail::mag<2> * detail::mag_pi * detail::mag_pi * detail::mag_pi * detail::mag_pi * detail::mag_pi
        * square(square(si2019::boltzmann_constant))
        / (detail::mag<15> * cubic(si2019::planck_constant) * square(si2019::speed_of_light_in_vacuum)),
      {.ascii = "sigma"}};
    inline constexpr auto     stefan_boltzmann_constant = defined_by<stefan_boltzmann_constant_def>;
    inline constexpr unit_def first_radiation_constant_def{
      "c₁", detail::mag<2> * detail::mag_pi * si2019::planck_constant * square(si2019::speed_of_light_in_vacuum),
      {.ascii = "c_1"}};
    inline constexpr auto     first_radiation_constant = defined_by<first_radiation_constant_def>;
    inline constexpr unit_def second_radiation_constant_def{
      "c₂", si2019::planck_constant * si2019::speed_of_light_in_vacuum / si2019::boltzmann_constant, {.ascii = "c_2"}};
    inline constexpr auto     second_radiation_constant = defined_by<second_radiation_constant_def>;
  } // namespace si2019

  // CODATA 2022 recommended values (Mohr et al., Rev. Mod. Phys. 97, 025002).
  namespace codata2022
  {
    inline constexpr unit_def gravitational_constant_def{
      "G", detail::mag<6.67430e-11_g> * cubic(si::metre) / (si::kilogram * square(si::second))};
    inline constexpr auto     gravitational_constant = defined_by<gravitational_constant_def>;
    inline constexpr unit_def fine_structure_constant_def{
      "α", detail::mag<7.2973525643e-3_g> * one, {.ascii = "alpha"}};
    inline constexpr auto     fine_structure_constant = defined_by<fine_structure_constant_def>;
    inline constexpr unit_def rydberg_constant_def{"R_∞", detail::mag<10'973'731.568157_g> / si::metre,
                                                   {.ascii = "R_inf"}};
    inline constexpr auto     rydberg_constant = defined_by<rydberg_constant_def>;
    inline constexpr unit_def vacuum_magnetic_permeability_def{
      "μ₀", detail::mag<1.25663706127e-6_g> * si::newton / square(si::ampere), {.ascii = "mu_0"}};
    inline constexpr auto     vacuum_magnetic_permeability = defined_by<vacuum_magnetic_permeability_def>;
    inline constexpr unit_def characteristic_impedance_of_vacuum_def{"Z₀", detail::mag<376.730313412_g> * si::ohm,
                                                                     {.ascii = "Z_0"}};
    inline constexpr auto     characteristic_impedance_of_vacuum = defined_by<characteristic_impedance_of_vacuum_def>;
    inline constexpr unit_def electron_mass_def{"m_e", detail::mag<9.1093837139e-31_g> * si::kilogram};
    inline constexpr auto     electron_mass = defined_by<electron_mass_def>;
    inline constexpr unit_def proton_mass_def{"m_p", detail::mag<1.67262192595e-27_g> * si::kilogram};
    inline constexpr auto     proton_mass = defined_by<proton_mass_def>;
    inline constexpr unit_def neutron_mass_def{"m_n", detail::mag<1.67492750056e-27_g> * si::kilogram};
    inline constexpr auto     neutron_mass = defined_by<neutron_mass_def>;
    inline constexpr unit_def atomic_mass_constant_def{"m_u", detail::mag<1.66053906892e-27_g> * si::kilogram};
    inline constexpr auto     atomic_mass_constant = defined_by<atomic_mass_constant_def>;
    inline constexpr unit_def bohr_radius_def{"a₀", detail::mag<5.29177210544e-11_g> * si::metre, {.ascii = "a_0"}};
    inline constexpr auto     bohr_radius = defined_by<bohr_radius_def>;
    inline constexpr unit_def vacuum_electric_permittivity_def{"ε₀",
                                                               detail::mag<8.8541878188e-12_g> * si::farad / si::metre,
                                                               {.ascii = "epsilon_0"}};
    inline constexpr auto     vacuum_electric_permittivity = defined_by<vacuum_electric_permittivity_def>;
  } // namespace codata2022

  // The constants as quantities: the number 1 of the constant's unit, under its ISQ kind where the kind is
  // unambiguous.
  namespace detail
  {
    template <reference R>
    inline constexpr auto one_of = quantity_access::make<quantity<R, std::remove_cvref_t<decltype(just<1>)>>>(just<1>);
  } // namespace detail

  namespace constants
  {
    // SI 2019 defining constants, standard gravity and the standard atmosphere.
    inline constexpr auto hyperfine_structure_transition_frequency_of_cs =
      detail::one_of<isq::frequency[si2019::hyperfine_structure_transition_frequency_of_cs]>;
    inline constexpr auto speed_of_light_in_vacuum = detail::one_of<isq::speed[si2019::speed_of_light_in_vacuum]>;
    inline constexpr auto luminous_efficacy = detail::one_of<si2019::luminous_efficacy>;
    inline constexpr auto standard_gravity = detail::one_of<isq::acceleration_of_free_fall[si2019::standard_gravity]>;
    inline constexpr auto standard_atmosphere = detail::one_of<isq::pressure[si2019::standard_atmosphere]>;
    inline constexpr auto planck_constant = detail::one_of<isq::action[si2019::planck_constant]>;
    inline constexpr auto reduced_planck_constant = detail::one_of<isq::action[si2019::reduced_planck_constant]>;
    inline constexpr auto elementary_charge = detail::one_of<isq::electric_charge[si2019::elementary_charge]>;
    inline constexpr auto boltzmann_constant = detail::one_of<si2019::boltzmann_constant>;
    inline constexpr auto avogadro_constant = detail::one_of<si2019::avogadro_constant>;
    inline constexpr auto molar_gas_constant = detail::one_of<si2019::molar_gas_constant>;
    inline constexpr auto faraday_constant = detail::one_of<si2019::faraday_constant>;
    inline constexpr auto josephson_constant = detail::one_of<si2019::josephson_constant>;
    inline constexpr auto von_klitzing_constant = detail::one_of<isq::resistance[si2019::von_klitzing_constant]>;
    inline constexpr auto conductance_quantum = detail::one_of<isq::conductance[si2019::conductance_quantum]>;
    inline constexpr auto magnetic_flux_quantum = detail::one_of<isq::magnetic_flux[si2019::magnetic_flux_quantum]>;

    inline constexpr auto stefan_boltzmann_constant = detail::one_of<si2019::stefan_boltzmann_constant>;
    inline constexpr auto first_radiation_constant = detail::one_of<si2019::first_radiation_constant>;
    inline constexpr auto second_radiation_constant = detail::one_of<si2019::second_radiation_constant>;

    // CODATA 2022 recommended values.
    inline constexpr auto gravitational_constant = detail::one_of<codata2022::gravitational_constant>;
    inline constexpr auto fine_structure_constant = detail::one_of<codata2022::fine_structure_constant>;
    inline constexpr auto rydberg_constant = detail::one_of<codata2022::rydberg_constant>;
    inline constexpr auto vacuum_magnetic_permeability = detail::one_of<codata2022::vacuum_magnetic_permeability>;
    inline constexpr auto characteristic_impedance_of_vacuum =
      detail::one_of<isq::impedance[codata2022::characteristic_impedance_of_vacuum]>;
    inline constexpr auto electron_mass = detail::one_of<isq::mass[codata2022::electron_mass]>;
    inline constexpr auto proton_mass = detail::one_of<isq::mass[codata2022::proton_mass]>;
    inline constexpr auto neutron_mass = detail::one_of<isq::mass[codata2022::neutron_mass]>;
    inline constexpr auto atomic_mass_constant = detail::one_of<isq::mass[codata2022::atomic_mass_constant]>;
    inline constexpr auto bohr_radius = detail::one_of<isq::radius[codata2022::bohr_radius]>;
    inline constexpr auto vacuum_electric_permittivity = detail::one_of<codata2022::vacuum_electric_permittivity>;
  } // namespace constants

} // namespace ins

#endif // INSIDE_UNITS_CONSTANTS_HPP
