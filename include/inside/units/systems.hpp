// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The catalog: ISQ kinds, SI units and prefixes, si2019's defining constants, international units, and the
// dimensionless percent and per_mille. Part of <inside/units.hpp>.
#ifndef INSIDE_UNITS_SYSTEMS_HPP
#define INSIDE_UNITS_SYSTEMS_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/units/core.hpp>

#include <initializer_list>

namespace ins
{

  // The catalog: ISQ kinds, SI units and prefixes, international units.
  // Each entry is a named definition and its name, as one of your own can be (units/detail/define.hpp):
  // diagnostics and symbols spell si::volt as (& ins::si::volt_def).
  // A scaled unit is spelled detail::mag<N> * u here, not the quantity N * u:
  // a quantity type costs about thirty times as much to compile, and the catalog has thirty of them.

  // Base quantities (ISO 80000-1) and the dimensionless one.
  // The seven base dimensions are declared as one of your own is.
  namespace isq
  {
    inline constexpr kind_def dimensionless_def{"dimensionless", dim_one};
    inline constexpr auto     dimensionless = defined_by<dimensionless_def>;
    inline constexpr kind_def length_def{"length", base_dimension<"length">};
    inline constexpr auto     length = defined_by<length_def>;
    inline constexpr kind_def mass_def{"mass", base_dimension<"mass">};
    inline constexpr auto     mass = defined_by<mass_def>;
    inline constexpr kind_def duration_def{"duration", base_dimension<"time">};
    inline constexpr auto     duration = defined_by<duration_def>;
    inline constexpr kind_def electric_current_def{"electric_current", base_dimension<"electric_current">};
    inline constexpr auto     electric_current = defined_by<electric_current_def>;
    inline constexpr kind_def thermodynamic_temperature_def{"thermodynamic_temperature",
                                                            base_dimension<"thermodynamic_temperature">};
    inline constexpr auto     thermodynamic_temperature = defined_by<thermodynamic_temperature_def>;
    inline constexpr kind_def amount_of_substance_def{"amount_of_substance", base_dimension<"amount_of_substance">};
    inline constexpr auto     amount_of_substance = defined_by<amount_of_substance_def>;
    inline constexpr kind_def luminous_intensity_def{"luminous_intensity", base_dimension<"luminous_intensity">};
    inline constexpr auto     luminous_intensity = defined_by<luminous_intensity_def>;
    inline constexpr auto time = duration;
  } // namespace isq

  // Space and time (ISO 80000-3).
  namespace isq
  {
    inline constexpr kind_def width_def{"width", isq::length};
    inline constexpr auto     width = defined_by<width_def>;
    inline constexpr kind_def thickness_def{"thickness", isq::width};
    inline constexpr auto     thickness = defined_by<thickness_def>;
    inline constexpr kind_def diameter_def{"diameter", isq::width};
    inline constexpr auto     diameter = defined_by<diameter_def>;
    inline constexpr kind_def radius_def{"radius", isq::width};
    inline constexpr auto     radius = defined_by<radius_def>;
    inline constexpr kind_def altitude_def{"altitude", isq::length};
    inline constexpr auto     altitude = defined_by<altitude_def>;
    inline constexpr kind_def height_def{"height", isq::altitude};
    inline constexpr auto     height = defined_by<height_def>;
    inline constexpr kind_def path_length_def{"path_length", isq::length};
    inline constexpr auto     path_length = defined_by<path_length_def>;
    inline constexpr kind_def arc_length_def{"arc_length", isq::path_length};
    inline constexpr auto     arc_length = defined_by<arc_length_def>;
    inline constexpr kind_def distance_def{"distance", isq::path_length};
    inline constexpr auto     distance = defined_by<distance_def>;
    inline constexpr kind_def radial_distance_def{"radial_distance", isq::distance};
    inline constexpr auto     radial_distance = defined_by<radial_distance_def>;
    inline constexpr kind_def wavelength_def{"wavelength", isq::length};
    inline constexpr auto     wavelength = defined_by<wavelength_def>;
    inline constexpr kind_def displacement_def{"displacement", isq::length};
    inline constexpr auto     displacement = defined_by<displacement_def>;
    inline constexpr kind_def position_vector_def{"position_vector", isq::displacement};
    inline constexpr auto     position_vector = defined_by<position_vector_def>;
    inline constexpr kind_def area_def{"area", pow<2>(isq::length)};
    inline constexpr auto     area = defined_by<area_def>;
    inline constexpr kind_def volume_def{"volume", pow<3>(isq::length)};
    inline constexpr auto     volume = defined_by<volume_def>;

    inline constexpr kind_def angular_measure_def{"angular_measure", isq::dimensionless,
                                                  {.distinct = true, .recipe = isq::arc_length / isq::radius}};
    inline constexpr auto     angular_measure = defined_by<angular_measure_def>;
    inline constexpr kind_def rotational_displacement_def{"rotational_displacement", isq::angular_measure,
                                                          {.recipe = isq::path_length / isq::radius}};
    inline constexpr auto     rotational_displacement = defined_by<rotational_displacement_def>;
    inline constexpr kind_def phase_angle_def{"phase_angle", isq::angular_measure, {.distinct = true}};
    inline constexpr auto     phase_angle = defined_by<phase_angle_def>;
    inline constexpr kind_def solid_angular_measure_def{"solid_angular_measure", isq::dimensionless,
                                                        {.distinct = true, .recipe = isq::area / pow<2>(isq::radius)}};
    inline constexpr auto     solid_angular_measure = defined_by<solid_angular_measure_def>;

    inline constexpr kind_def period_duration_def{"period_duration", isq::duration};
    inline constexpr auto     period_duration = defined_by<period_duration_def>;
    inline constexpr kind_def time_constant_def{"time_constant", isq::duration};
    inline constexpr auto     time_constant = defined_by<time_constant_def>;
    inline constexpr kind_def speed_def{"speed", isq::length / isq::duration};
    inline constexpr auto     speed = defined_by<speed_def>;
    inline constexpr kind_def velocity_def{"velocity", isq::speed, {.recipe = isq::displacement / isq::duration}};
    inline constexpr auto     velocity = defined_by<velocity_def>;
    inline constexpr kind_def acceleration_def{"acceleration", isq::velocity / isq::duration};
    inline constexpr auto     acceleration = defined_by<acceleration_def>;
    inline constexpr kind_def acceleration_of_free_fall_def{"acceleration_of_free_fall", isq::acceleration};
    inline constexpr auto     acceleration_of_free_fall = defined_by<acceleration_of_free_fall_def>;
    inline constexpr kind_def angular_velocity_def{"angular_velocity", isq::rotational_displacement / isq::duration};
    inline constexpr auto     angular_velocity = defined_by<angular_velocity_def>;
    inline constexpr kind_def angular_acceleration_def{"angular_acceleration", isq::angular_velocity / isq::duration};
    inline constexpr auto     angular_acceleration = defined_by<angular_acceleration_def>;
    inline constexpr kind_def frequency_def{"frequency", inverse(isq::period_duration)};
    inline constexpr auto     frequency = defined_by<frequency_def>;
    inline constexpr kind_def rotation_def{"rotation", isq::dimensionless};
    inline constexpr auto     rotation = defined_by<rotation_def>;
    inline constexpr kind_def rotational_frequency_def{"rotational_frequency", isq::rotation / isq::duration};
    inline constexpr auto     rotational_frequency = defined_by<rotational_frequency_def>;
    inline constexpr kind_def angular_frequency_def{"angular_frequency", isq::phase_angle / isq::duration};
    inline constexpr auto     angular_frequency = defined_by<angular_frequency_def>;

    // Mechanics (ISO 80000-4).
    inline constexpr kind_def mass_density_def{"mass_density", isq::mass / isq::volume};
    inline constexpr auto     mass_density = defined_by<mass_density_def>;
    inline constexpr kind_def momentum_def{"momentum", isq::mass * isq::velocity};
    inline constexpr auto     momentum = defined_by<momentum_def>;
    inline constexpr kind_def force_def{"force", isq::mass * isq::length / pow<2>(isq::duration)};
    inline constexpr auto     force = defined_by<force_def>;
    inline constexpr kind_def weight_def{"weight", isq::force, {.recipe = isq::mass * isq::acceleration_of_free_fall}};
    inline constexpr auto     weight = defined_by<weight_def>;
    inline constexpr kind_def impulse_def{"impulse", isq::momentum, {.recipe = isq::force * isq::duration}};
    inline constexpr auto     impulse = defined_by<impulse_def>;
    inline constexpr kind_def moment_of_force_def{"moment_of_force", isq::position_vector * isq::force};
    inline constexpr auto     moment_of_force = defined_by<moment_of_force_def>;
    inline constexpr kind_def torque_def{"torque", isq::moment_of_force};
    inline constexpr auto     torque = defined_by<torque_def>;
    inline constexpr kind_def pressure_def{"pressure", isq::force / isq::area};
    inline constexpr auto     pressure = defined_by<pressure_def>;
    inline constexpr kind_def gauge_pressure_def{"gauge_pressure", isq::pressure};
    inline constexpr auto     gauge_pressure = defined_by<gauge_pressure_def>;
    inline constexpr kind_def energy_def{"energy", isq::mass * pow<2>(isq::length) / pow<2>(isq::duration)};
    inline constexpr auto     energy = defined_by<energy_def>;
    inline constexpr kind_def mechanical_work_def{"mechanical_work", isq::energy,
                                                  {.recipe = isq::force * isq::displacement}};
    inline constexpr auto     mechanical_work = defined_by<mechanical_work_def>;
    inline constexpr kind_def mechanical_energy_def{"mechanical_energy", isq::mechanical_work};
    inline constexpr auto     mechanical_energy = defined_by<mechanical_energy_def>;
    inline constexpr kind_def potential_energy_def{"potential_energy", isq::mechanical_energy};
    inline constexpr auto     potential_energy = defined_by<potential_energy_def>;
    inline constexpr kind_def kinetic_energy_def{"kinetic_energy", isq::mechanical_energy,
                                                 {.recipe = isq::mass * pow<2>(isq::speed)}};
    inline constexpr auto     kinetic_energy = defined_by<kinetic_energy_def>;
    inline constexpr kind_def power_def{"power", isq::mass * pow<2>(isq::length) / pow<3>(isq::duration)};
    inline constexpr auto     power = defined_by<power_def>;
    inline constexpr kind_def mechanical_power_def{"mechanical_power", isq::power,
                                                   {.recipe = isq::force * isq::velocity}};
    inline constexpr auto     mechanical_power = defined_by<mechanical_power_def>;
    inline constexpr kind_def action_def{"action", isq::energy * isq::duration};
    inline constexpr auto     action = defined_by<action_def>;
    inline constexpr kind_def mass_flow_rate_def{"mass_flow_rate", isq::mass / isq::duration};
    inline constexpr auto     mass_flow_rate = defined_by<mass_flow_rate_def>;
    inline constexpr kind_def volume_flow_rate_def{"volume_flow_rate", isq::volume / isq::duration};
    inline constexpr auto     volume_flow_rate = defined_by<volume_flow_rate_def>;
    inline constexpr auto work = mechanical_work;
  } // namespace isq

  // Thermodynamics (ISO 80000-5).
  namespace isq
  {
    inline constexpr kind_def internal_energy_def{"internal_energy", isq::energy};
    inline constexpr auto     internal_energy = defined_by<internal_energy_def>;
    inline constexpr kind_def heat_def{"heat", isq::internal_energy};
    inline constexpr auto     heat = defined_by<heat_def>;
    inline constexpr kind_def enthalpy_def{"enthalpy", isq::internal_energy};
    inline constexpr auto     enthalpy = defined_by<enthalpy_def>;
    inline constexpr kind_def heat_capacity_def{"heat_capacity", isq::heat / isq::thermodynamic_temperature};
    inline constexpr auto     heat_capacity = defined_by<heat_capacity_def>;
    inline constexpr kind_def entropy_def{"entropy", isq::heat / isq::thermodynamic_temperature};
    inline constexpr auto     entropy = defined_by<entropy_def>;
    inline constexpr kind_def heat_flow_rate_def{"heat_flow_rate", isq::heat / isq::duration};
    inline constexpr auto     heat_flow_rate = defined_by<heat_flow_rate_def>;

    // Electromagnetism (IEC 80000-6).
    inline constexpr kind_def electric_charge_def{"electric_charge", isq::electric_current * isq::duration};
    inline constexpr auto     electric_charge = defined_by<electric_charge_def>;
    inline constexpr kind_def electric_potential_def{"electric_potential",
                                                     isq::energy / (isq::electric_current * isq::duration)};
    inline constexpr auto     electric_potential = defined_by<electric_potential_def>;
    inline constexpr kind_def voltage_def{"voltage", isq::electric_potential};
    inline constexpr auto     voltage = defined_by<voltage_def>;
    inline constexpr kind_def capacitance_def{"capacitance",
                                              isq::electric_current * isq::duration / isq::electric_potential};
    inline constexpr auto     capacitance = defined_by<capacitance_def>;

    inline constexpr kind_def impedance_def{"impedance", isq::electric_potential / isq::electric_current};
    inline constexpr auto     impedance = defined_by<impedance_def>;
    inline constexpr kind_def resistance_def{"resistance", isq::impedance,
                                             {.recipe = isq::voltage / isq::electric_current}};
    inline constexpr auto     resistance = defined_by<resistance_def>;
    inline constexpr kind_def admittance_def{"admittance", inverse(isq::impedance)};
    inline constexpr auto     admittance = defined_by<admittance_def>;
    inline constexpr kind_def conductance_def{"conductance", isq::admittance, {.recipe = inverse(isq::resistance)}};
    inline constexpr auto     conductance = defined_by<conductance_def>;

    inline constexpr kind_def magnetic_flux_density_def{"magnetic_flux_density",
                                                        isq::mass / (isq::electric_current * pow<2>(isq::duration))};
    inline constexpr auto     magnetic_flux_density = defined_by<magnetic_flux_density_def>;
    inline constexpr kind_def magnetic_flux_def{"magnetic_flux", isq::magnetic_flux_density * isq::area};
    inline constexpr auto     magnetic_flux = defined_by<magnetic_flux_def>;
    inline constexpr kind_def inductance_def{"inductance", isq::magnetic_flux / isq::electric_current};
    inline constexpr auto     inductance = defined_by<inductance_def>;
    inline constexpr kind_def electric_power_def{"electric_power", isq::power};
    inline constexpr auto     electric_power = defined_by<electric_power_def>;

    // Light (ISO 80000-7), photopic vision.
    inline constexpr kind_def luminous_flux_def{"luminous_flux", isq::luminous_intensity * isq::solid_angular_measure};
    inline constexpr auto     luminous_flux = defined_by<luminous_flux_def>;
    inline constexpr kind_def illuminance_def{"illuminance", isq::luminous_flux / isq::area};
    inline constexpr auto     illuminance = defined_by<illuminance_def>;

    // Physical chemistry and ionizing radiation (ISO 80000-9, -10).
    inline constexpr kind_def catalytic_activity_def{"catalytic_activity", isq::amount_of_substance / isq::duration};
    inline constexpr auto     catalytic_activity = defined_by<catalytic_activity_def>;
    inline constexpr kind_def activity_def{"activity", inverse(isq::duration)};
    inline constexpr auto     activity = defined_by<activity_def>;
    inline constexpr kind_def absorbed_dose_def{"absorbed_dose", isq::energy / isq::mass};
    inline constexpr auto     absorbed_dose = defined_by<absorbed_dose_def>;
    inline constexpr kind_def ionizing_radiation_quality_factor_def{"ionizing_radiation_quality_factor",
                                                                    isq::dimensionless};
    inline constexpr auto     ionizing_radiation_quality_factor = defined_by<ionizing_radiation_quality_factor_def>;
    inline constexpr kind_def dose_equivalent_def{"dose_equivalent",
                                                  isq::absorbed_dose * isq::ionizing_radiation_quality_factor};
    inline constexpr auto     dose_equivalent = defined_by<dose_equivalent_def>;
  } // namespace isq

  // SI prefixes. Past 10^±18 a prefix needs a grid number past 64 bits.
  namespace si
  {
    inline constexpr prefix_t<-30> quecto{};
    inline constexpr prefix_t<-27> ronto{};
    inline constexpr prefix_t<-24> yocto{};
    inline constexpr prefix_t<-21> zepto{};
    inline constexpr prefix_t<-18> atto{};
    inline constexpr prefix_t<-15> femto{};
    inline constexpr prefix_t<-12> pico{};
    inline constexpr prefix_t<-9>  nano{};
    inline constexpr prefix_t<-6>  micro{};
    inline constexpr prefix_t<-3>  milli{};
    inline constexpr prefix_t<-2>  centi{};
    inline constexpr prefix_t<-1>  deci{};
    inline constexpr prefix_t<1>   deca{};
    inline constexpr prefix_t<2>   hecto{};
    inline constexpr prefix_t<3>   kilo{};
    inline constexpr prefix_t<6>   mega{};
    inline constexpr prefix_t<9>   giga{};
    inline constexpr prefix_t<12>  tera{};
    inline constexpr prefix_t<15>  peta{};
    inline constexpr prefix_t<18>  exa{};
    inline constexpr prefix_t<21>  zetta{};
    inline constexpr prefix_t<24>  yotta{};
    inline constexpr prefix_t<27>  ronna{};
    inline constexpr prefix_t<30>  quetta{};

    // SI base units.
    inline constexpr unit_def second_def{"s", isq::duration};
    inline constexpr auto     second = defined_by<second_def>;
    inline constexpr unit_def metre_def{"m", isq::length};
    inline constexpr auto     metre = defined_by<metre_def>;
    // (The gram is 1/1000 of the coherent unit of mass, which the kilogram names.)
    inline constexpr unit_def gram_def{"g", detail::mag<1_g / 1000_g>, isq::mass};
    inline constexpr auto     gram = defined_by<gram_def>;
    inline constexpr auto kilogram = kilo(gram);
  } // namespace si
  namespace si
  {
    inline constexpr unit_def ampere_def{"A", isq::electric_current};
    inline constexpr auto     ampere = defined_by<ampere_def>;
    inline constexpr origin_def absolute_zero_def{"absolute_zero", isq::thermodynamic_temperature};
    inline constexpr auto       absolute_zero = defined_by<absolute_zero_def>;
    inline constexpr unit_def kelvin_def{"K", isq::thermodynamic_temperature, {.origin = si::absolute_zero}};
    inline constexpr auto     kelvin = defined_by<kelvin_def>;
    inline constexpr origin_def ice_point_def{"ice_point", si::absolute_zero + just<27315, 100> * si::kelvin};
    inline constexpr auto       ice_point = defined_by<ice_point_def>;
    inline constexpr unit_def mole_def{"mol", isq::amount_of_substance};
    inline constexpr auto     mole = defined_by<mole_def>;
    inline constexpr unit_def candela_def{"cd", isq::luminous_intensity};
    inline constexpr auto     candela = defined_by<candela_def>;

    // SI derived units with special names.
    inline constexpr unit_def radian_def{"rad", si::metre / si::metre, {.kind = isq::angular_measure}};
    inline constexpr auto     radian = defined_by<radian_def>;
    inline constexpr unit_def steradian_def{"sr", square(si::metre) / square(si::metre),
                                            {.kind = isq::solid_angular_measure}};
    inline constexpr auto     steradian = defined_by<steradian_def>;
    inline constexpr unit_def hertz_def{"Hz", one / si::second, {.kind = isq::frequency}};
    inline constexpr auto     hertz = defined_by<hertz_def>;
    inline constexpr unit_def newton_def{"N", si::kilogram * si::metre / square(si::second), {.kind = isq::force}};
    inline constexpr auto     newton = defined_by<newton_def>;
    inline constexpr unit_def pascal_def{"Pa", si::newton / square(si::metre), {.kind = isq::pressure}};
    inline constexpr auto     pascal = defined_by<pascal_def>;
    inline constexpr unit_def joule_def{"J", si::newton * si::metre, {.kind = isq::energy}};
    inline constexpr auto     joule = defined_by<joule_def>;
    inline constexpr unit_def watt_def{"W", si::joule / si::second};
    inline constexpr auto     watt = defined_by<watt_def>;
    inline constexpr unit_def coulomb_def{"C", si::ampere * si::second};
    inline constexpr auto     coulomb = defined_by<coulomb_def>;
    inline constexpr unit_def volt_def{"V", si::watt / si::ampere, {.kind = isq::electric_potential}};
    inline constexpr auto     volt = defined_by<volt_def>;
    inline constexpr unit_def farad_def{"F", si::coulomb / si::volt, {.kind = isq::capacitance}};
    inline constexpr auto     farad = defined_by<farad_def>;
    inline constexpr unit_def ohm_def{"Ω", si::volt / si::ampere, {.ascii = "ohm", .kind = isq::impedance}};
    inline constexpr auto     ohm = defined_by<ohm_def>;
    inline constexpr unit_def siemens_def{"S", one / si::ohm, {.kind = isq::admittance}};
    inline constexpr auto     siemens = defined_by<siemens_def>;
    inline constexpr unit_def weber_def{"Wb", si::volt * si::second};
    inline constexpr auto     weber = defined_by<weber_def>;
    inline constexpr unit_def tesla_def{"T", si::weber / square(si::metre), {.kind = isq::magnetic_flux_density}};
    inline constexpr auto     tesla = defined_by<tesla_def>;
    inline constexpr unit_def henry_def{"H", si::weber / si::ampere};
    inline constexpr auto     henry = defined_by<henry_def>;
    inline constexpr unit_def degree_Celsius_def{"℃", si::kelvin, {.ascii = "`C", .origin = si::ice_point}};
    inline constexpr auto     degree_Celsius = defined_by<degree_Celsius_def>;
    inline constexpr unit_def lumen_def{"lm", si::candela * si::steradian, {.kind = isq::luminous_flux}};
    inline constexpr auto     lumen = defined_by<lumen_def>;
    inline constexpr unit_def lux_def{"lx", si::lumen / square(si::metre), {.kind = isq::illuminance}};
    inline constexpr auto     lux = defined_by<lux_def>;
    inline constexpr unit_def becquerel_def{"Bq", one / si::second, {.kind = isq::activity}};
    inline constexpr auto     becquerel = defined_by<becquerel_def>;
    inline constexpr unit_def gray_def{"Gy", si::joule / si::kilogram, {.kind = isq::absorbed_dose}};
    inline constexpr auto     gray = defined_by<gray_def>;
    inline constexpr unit_def sievert_def{"Sv", si::joule / si::kilogram, {.kind = isq::dose_equivalent}};
    inline constexpr auto     sievert = defined_by<sievert_def>;
    inline constexpr unit_def katal_def{"kat", si::mole / si::second, {.kind = isq::catalytic_activity}};
    inline constexpr auto     katal = defined_by<katal_def>;

    // Non-SI units accepted for use with the SI.
    inline constexpr unit_def minute_def{"min", detail::mag<60> * si::second};
    inline constexpr auto     minute = defined_by<minute_def>;
    inline constexpr unit_def hour_def{"h", detail::mag<60> * si::minute};
    inline constexpr auto     hour = defined_by<hour_def>;
    inline constexpr unit_def day_def{"d", detail::mag<24> * si::hour};
    inline constexpr auto     day = defined_by<day_def>;
    inline constexpr unit_def astronomical_unit_def{"au", detail::mag<149'597'870'700> * si::metre};
    inline constexpr auto     astronomical_unit = defined_by<astronomical_unit_def>;
    inline constexpr unit_def degree_def{"°", detail::mag<1_g / 180_g> * detail::mag_pi * si::radian,
                                         {.ascii = "deg", .space_before = false}};
    inline constexpr auto     degree = defined_by<degree_def>;
    inline constexpr unit_def arcminute_def{"′", detail::mag<1_g / 60_g> * si::degree,
                                            {.ascii = "'", .space_before = false}};
    inline constexpr auto     arcminute = defined_by<arcminute_def>;
    inline constexpr unit_def arcsecond_def{"″", detail::mag<1_g / 60_g> * si::arcminute,
                                            {.ascii = "''", .space_before = false}};
    inline constexpr auto     arcsecond = defined_by<arcsecond_def>;
    inline constexpr unit_def are_def{"a", square(si::deca(si::metre))};
    inline constexpr auto     are = defined_by<are_def>;
    inline constexpr unit_def litre_def{"L", cubic(si::deci(si::metre))};
    inline constexpr auto     litre = defined_by<litre_def>;
    inline constexpr unit_def tonne_def{"t", detail::mag<1000> * si::kilogram};
    inline constexpr auto     tonne = defined_by<tonne_def>;
    inline constexpr auto hectare = hecto(are);
  } // namespace si
  namespace si
  {
    inline constexpr unit_def dalton_def{"Da", detail::mag<1.66053906892e-27_g> * si::kilogram}; // CODATA 2022
    inline constexpr auto     dalton = defined_by<dalton_def>;
    inline constexpr unit_def electronvolt_def{"eV", detail::mag<1.602176634e-19_g> * si::joule};
    inline constexpr auto     electronvolt = defined_by<electronvolt_def>;
  } // namespace si

  // The defining constants of the SI (2019) and standard gravity, as units:
  // just<1> * si2019::speed_of_light_in_vacuum is c, exactly, and .in(m/s) gives its number.
  // h, e, k and N_A need grid numbers past 64 bits.
  namespace si2019
  {
    inline constexpr unit_def hyperfine_structure_transition_frequency_of_cs_def{
      "Δν_Cs", detail::mag<9'192'631'770> * si::hertz, {.ascii = "dv_Cs"}};
    inline constexpr auto     hyperfine_structure_transition_frequency_of_cs =
      defined_by<hyperfine_structure_transition_frequency_of_cs_def>;
    inline constexpr unit_def speed_of_light_in_vacuum_def{"c", detail::mag<299'792'458> * (si::metre / si::second),
                                                           {.ascii = "c_0"}}; // (c is free: a cent's)
    inline constexpr auto     speed_of_light_in_vacuum = defined_by<speed_of_light_in_vacuum_def>;
    inline constexpr unit_def luminous_efficacy_def{"K_cd", detail::mag<683> * (si::lumen / si::watt)};
    inline constexpr auto     luminous_efficacy = defined_by<luminous_efficacy_def>;
    inline constexpr unit_def standard_gravity_def{"g₀",
                                                   detail::mag<980665_g / 100000_g> * (si::metre / square(si::second)),
                                                   {.ascii = "g_0"}};
    inline constexpr auto     standard_gravity = defined_by<standard_gravity_def>;
    inline constexpr unit_def planck_constant_def{"h", detail::mag<6.62607015e-34_g> * si::joule * si::second,
                                                  {.ascii = "h_P"}}; // (h is the hour)
    inline constexpr auto     planck_constant = defined_by<planck_constant_def>;
    inline constexpr unit_def elementary_charge_def{"e", detail::mag<1.602176634e-19_g> * si::coulomb};
    inline constexpr auto     elementary_charge = defined_by<elementary_charge_def>;
    inline constexpr unit_def boltzmann_constant_def{"k", detail::mag<1.380649e-23_g> * (si::joule / si::kelvin),
                                                     {.ascii = "k_B"}}; // (k is free: a quaternion's)
    inline constexpr auto     boltzmann_constant = defined_by<boltzmann_constant_def>;
    inline constexpr unit_def avogadro_constant_def{"N_A", detail::mag<6.02214076e23_g> / si::mole};
    inline constexpr auto     avogadro_constant = defined_by<avogadro_constant_def>;
  } // namespace si2019

  // International (yard and pound agreement, 1959) and nautical units.
  namespace international
  {
    inline constexpr unit_def yard_def{"yd", detail::mag<9144_g / 10000_g> * si::metre};
    inline constexpr auto     yard = defined_by<yard_def>;
    inline constexpr unit_def foot_def{"ft", detail::mag<1_g / 3_g> * international::yard};
    inline constexpr auto     foot = defined_by<foot_def>;
    inline constexpr unit_def inch_def{"in", detail::mag<1_g / 12_g> * international::foot};
    inline constexpr auto     inch = defined_by<inch_def>;
    inline constexpr unit_def mile_def{"mi", detail::mag<1760> * international::yard};
    inline constexpr auto     mile = defined_by<mile_def>;
    inline constexpr unit_def nautical_mile_def{"nmi", detail::mag<1852> * si::metre};
    inline constexpr auto     nautical_mile = defined_by<nautical_mile_def>;

    inline constexpr unit_def pound_def{"lb", detail::mag<45359237_g / 100000000_g> * si::kilogram};
    inline constexpr auto     pound = defined_by<pound_def>;
    inline constexpr unit_def ounce_def{"oz", detail::mag<1_g / 16_g> * international::pound};
    inline constexpr auto     ounce = defined_by<ounce_def>;
    inline constexpr unit_def pound_force_def{
      "lbf", detail::mag<980665_g / 100000_g> * (international::pound * si::metre / square(si::second))};
    inline constexpr auto     pound_force = defined_by<pound_force_def>;

    inline constexpr unit_def knot_def{"kn", international::nautical_mile / si::hour};
    inline constexpr auto     knot = defined_by<knot_def>;

    inline constexpr origin_def zeroth_degree_Fahrenheit_def{"zeroth_degree_Fahrenheit",
                                                             si::absolute_zero + just<45967, 180> * si::kelvin};
    inline constexpr auto       zeroth_degree_Fahrenheit = defined_by<zeroth_degree_Fahrenheit_def>;
    inline constexpr unit_def degree_Fahrenheit_def{
      "℉", detail::mag<5_g / 9_g> * si::kelvin, {.ascii = "`F", .origin = international::zeroth_degree_Fahrenheit}};
    inline constexpr auto     degree_Fahrenheit = defined_by<degree_Fahrenheit_def>;
  } // namespace international

  // Scaled dimensionless units, as in mp-units: 8 % is the number 2/25.
  inline constexpr unit_def percent_def{"%", detail::mag<0.01_g> * one};
  inline constexpr auto     percent = defined_by<percent_def>;
  inline constexpr unit_def per_mille_def{"‰", detail::mag<0.001_g> * one, {.ascii = "permille"}};
  inline constexpr auto     per_mille = defined_by<per_mille_def>;

} // namespace ins

// The catalog's units, kinds and origins, read once here after the catalog: a quantity type's unit, kind and origin
// are looked up here first (units/detail/unit_key.hpp), so a type of the catalog's searches nothing else.
// Its tags are in ins itself (one, percent, …), ins::isq, ins::si and ins::international; the constants of
// ins::si2019 come after them.
namespace ins::detail::catalog_index
{
  // The widely used prefixed units, which a quantity's text names by their symbols (km, ms, kHz, mV). No other
  // prefix is read into a symbol: a prefixed unit of your own is named by a tag of it
  // (inline constexpr auto kftm = si::kilo(ftm)).
  consteval void add_prefixed(tag_vectors& t, const unit_def& u, std::initializer_list<int> powers)
  {
    for (const int p : powers)
      t.add(unit_atom{&u, static_cast<std::int8_t>(p), 1});
  }

  inline constexpr catalog_sets table = []
  {
    tag_vectors tags;
    collect_tags(^^ins, tags, {}, false);
    collect_tags(^^ins::isq, tags);
    collect_tags(^^ins::si, tags);
    collect_tags(^^ins::international, tags);
    add_prefixed(tags, si::metre_def, {3, -2, -3, -6, -9, -12});
    add_prefixed(tags, si::gram_def, {-3, -6, -9});
    add_prefixed(tags, si::second_def, {-3, -6, -9, -12});
    add_prefixed(tags, si::litre_def, {-1, -2, -3, -6});
    add_prefixed(tags, si::hertz_def, {3, 6, 9, 12});
    add_prefixed(tags, si::watt_def, {-6, -3, 3, 6, 9});
    add_prefixed(tags, si::volt_def, {-6, -3, 3, 6});
    add_prefixed(tags, si::ampere_def, {-9, -6, -3, 3});
    add_prefixed(tags, si::ohm_def, {-3, 3, 6});
    add_prefixed(tags, si::siemens_def, {-6, -3});
    add_prefixed(tags, si::farad_def, {-12, -9, -6, -3});
    add_prefixed(tags, si::henry_def, {-9, -6, -3});
    add_prefixed(tags, si::newton_def, {-3, 3, 6});
    add_prefixed(tags, si::joule_def, {3, 6, 9});
    add_prefixed(tags, si::pascal_def, {2, 3, 6, 9});
    add_prefixed(tags, si::coulomb_def, {-9, -6, -3});
    add_prefixed(tags, si::mole_def, {-9, -6, -3});
    add_prefixed(tags, si::kelvin_def, {-3});
    add_prefixed(tags, si::becquerel_def, {3, 6, 9});
    add_prefixed(tags, si::gray_def, {-3});
    add_prefixed(tags, si::sievert_def, {-6, -3});
    add_prefixed(tags, si::tesla_def, {-9, -6, -3});
    add_prefixed(tags, si::radian_def, {-6, -3});
    add_prefixed(tags, si::electronvolt_def, {3, 6, 9, 12});
    return intern_tags(tags);
  }();
  inline constexpr catalog_sets si2019_table = []
  {
    tag_vectors tags;
    collect_tags(^^ins::si2019, tags);
    return intern_tags(tags);
  }();
} // namespace ins::detail::catalog_index

#endif // INSIDE_UNITS_SYSTEMS_HPP
