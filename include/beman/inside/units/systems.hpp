// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The catalog: ISQ kinds, SI units and prefixes, si2019's defining
// constants, international units, and the dimensionless percent and
// per_mille. Part of <beman/inside/units.hpp>.
#ifndef BEMAN_INSIDE_UNITS_SYSTEMS_HPP
#define BEMAN_INSIDE_UNITS_SYSTEMS_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/units/core.hpp>

namespace beman::inside::units {

//---------------------------------------------------------------------------
// The catalog: ISQ kinds, SI units and prefixes, international units.
// Each entry is declared as a unit of your own is (units/detail/define.hpp).
// A scaled unit is spelled detail::mag<N> * u here, not the quantity N * u: a
// quantity type costs about thirty times as much to compile, and the catalog
// has thirty of them.
//---------------------------------------------------------------------------
// Base quantities (ISO 80000-1) and the dimensionless one. The seven base
// dimensions are declared as one of your own is.
namespace isq {
inline constexpr auto dimensionless    = kind<"dimensionless", dim_one>;
inline constexpr auto length           = kind<"length", base_dimension<"length">>;
inline constexpr auto mass             = kind<"mass", base_dimension<"mass">>;
inline constexpr auto duration         = kind<"duration", base_dimension<"time">>;
inline constexpr auto electric_current = kind<"electric_current", base_dimension<"electric_current">>;
inline constexpr auto thermodynamic_temperature =
    kind<"thermodynamic_temperature", base_dimension<"thermodynamic_temperature">>;
inline constexpr auto amount_of_substance = kind<"amount_of_substance", base_dimension<"amount_of_substance">>;
inline constexpr auto luminous_intensity  = kind<"luminous_intensity", base_dimension<"luminous_intensity">>;
inline constexpr auto time                = duration;
} // namespace isq

// Space and time (ISO 80000-3).
namespace isq {
inline constexpr auto width           = kind<"width", isq::length>;
inline constexpr auto thickness       = kind<"thickness", isq::width>;
inline constexpr auto diameter        = kind<"diameter", isq::width>;
inline constexpr auto radius          = kind<"radius", isq::width>;
inline constexpr auto altitude        = kind<"altitude", isq::length>;
inline constexpr auto height          = kind<"height", isq::altitude>;
inline constexpr auto path_length     = kind<"path_length", isq::length>;
inline constexpr auto arc_length      = kind<"arc_length", isq::path_length>;
inline constexpr auto distance        = kind<"distance", isq::path_length>;
inline constexpr auto radial_distance = kind<"radial_distance", isq::distance>;
inline constexpr auto wavelength      = kind<"wavelength", isq::length>;
inline constexpr auto displacement    = kind<"displacement", isq::length>;
inline constexpr auto position_vector = kind<"position_vector", isq::displacement>;
inline constexpr auto area            = kind<"area", pow<2>(isq::length)>;
inline constexpr auto volume          = kind<"volume", pow<3>(isq::length)>;
inline constexpr auto angular_measure =
    kind<"angular_measure", isq::dimensionless, {.distinct = true, .recipe = isq::arc_length / isq::radius}>;
inline constexpr auto rotational_displacement =
    kind<"rotational_displacement", isq::angular_measure, {.recipe = isq::path_length / isq::radius}>;
inline constexpr auto phase_angle = kind<"phase_angle", isq::angular_measure, {.distinct = true}>;
inline constexpr auto solid_angular_measure =
    kind<"solid_angular_measure", isq::dimensionless, {.distinct = true, .recipe = isq::area / pow<2>(isq::radius)}>;
inline constexpr auto period_duration = kind<"period_duration", isq::duration>;
inline constexpr auto time_constant   = kind<"time_constant", isq::duration>;
inline constexpr auto speed           = kind<"speed", isq::length / isq::duration>;
inline constexpr auto velocity        = kind<"velocity", isq::speed, {.recipe = isq::displacement / isq::duration}>;
inline constexpr auto acceleration    = kind<"acceleration", isq::velocity / isq::duration>;
inline constexpr auto acceleration_of_free_fall = kind<"acceleration_of_free_fall", isq::acceleration>;
inline constexpr auto angular_velocity     = kind<"angular_velocity", isq::rotational_displacement / isq::duration>;
inline constexpr auto angular_acceleration = kind<"angular_acceleration", isq::angular_velocity / isq::duration>;
inline constexpr auto frequency            = kind<"frequency", inverse(isq::period_duration)>;
inline constexpr auto rotation             = kind<"rotation", isq::dimensionless>;
inline constexpr auto rotational_frequency = kind<"rotational_frequency", isq::rotation / isq::duration>;
inline constexpr auto angular_frequency    = kind<"angular_frequency", isq::phase_angle / isq::duration>;

// Mechanics (ISO 80000-4).
inline constexpr auto mass_density = kind<"mass_density", isq::mass / isq::volume>;
inline constexpr auto momentum     = kind<"momentum", isq::mass * isq::velocity>;
inline constexpr auto force        = kind<"force", isq::mass * isq::length / pow<2>(isq::duration)>;
inline constexpr auto weight  = kind<"weight", isq::force, {.recipe = isq::mass * isq::acceleration_of_free_fall}>;
inline constexpr auto impulse = kind<"impulse", isq::momentum, {.recipe = isq::force * isq::duration}>;
inline constexpr auto moment_of_force = kind<"moment_of_force", isq::position_vector * isq::force>;
inline constexpr auto torque          = kind<"torque", isq::moment_of_force>;
inline constexpr auto pressure        = kind<"pressure", isq::force / isq::area>;
inline constexpr auto gauge_pressure  = kind<"gauge_pressure", isq::pressure>;
inline constexpr auto energy          = kind<"energy", isq::mass * pow<2>(isq::length) / pow<2>(isq::duration)>;
inline constexpr auto mechanical_work =
    kind<"mechanical_work", isq::energy, {.recipe = isq::force * isq::displacement}>;
inline constexpr auto mechanical_energy = kind<"mechanical_energy", isq::mechanical_work>;
inline constexpr auto potential_energy  = kind<"potential_energy", isq::mechanical_energy>;
inline constexpr auto kinetic_energy =
    kind<"kinetic_energy", isq::mechanical_energy, {.recipe = isq::mass * pow<2>(isq::speed)}>;
inline constexpr auto power            = kind<"power", isq::mass * pow<2>(isq::length) / pow<3>(isq::duration)>;
inline constexpr auto mechanical_power = kind<"mechanical_power", isq::power, {.recipe = isq::force * isq::velocity}>;
inline constexpr auto action           = kind<"action", isq::energy * isq::duration>;
inline constexpr auto mass_flow_rate   = kind<"mass_flow_rate", isq::mass / isq::duration>;
inline constexpr auto volume_flow_rate = kind<"volume_flow_rate", isq::volume / isq::duration>;
inline constexpr auto work             = mechanical_work;
} // namespace isq

// Thermodynamics (ISO 80000-5).
namespace isq {
inline constexpr auto internal_energy = kind<"internal_energy", isq::energy>;
inline constexpr auto heat            = kind<"heat", isq::internal_energy>;
inline constexpr auto enthalpy        = kind<"enthalpy", isq::internal_energy>;
inline constexpr auto heat_capacity   = kind<"heat_capacity", isq::heat / isq::thermodynamic_temperature>;
inline constexpr auto entropy         = kind<"entropy", isq::heat / isq::thermodynamic_temperature>;
inline constexpr auto heat_flow_rate  = kind<"heat_flow_rate", isq::heat / isq::duration>;

// Electromagnetism (IEC 80000-6).
inline constexpr auto electric_charge = kind<"electric_charge", isq::electric_current * isq::duration>;
inline constexpr auto electric_potential =
    kind<"electric_potential", isq::energy / (isq::electric_current * isq::duration)>;
inline constexpr auto voltage = kind<"voltage", isq::electric_potential>;
inline constexpr auto capacitance =
    kind<"capacitance", isq::electric_current * isq::duration / isq::electric_potential>;
inline constexpr auto impedance = kind<"impedance", isq::electric_potential / isq::electric_current>;
inline constexpr auto resistance =
    kind<"resistance", isq::impedance, {.recipe = isq::voltage / isq::electric_current}>;
inline constexpr auto admittance  = kind<"admittance", inverse(isq::impedance)>;
inline constexpr auto conductance = kind<"conductance", isq::admittance, {.recipe = inverse(isq::resistance)}>;
inline constexpr auto magnetic_flux_density =
    kind<"magnetic_flux_density", isq::mass / (isq::electric_current * pow<2>(isq::duration))>;
inline constexpr auto magnetic_flux  = kind<"magnetic_flux", isq::magnetic_flux_density * isq::area>;
inline constexpr auto inductance     = kind<"inductance", isq::magnetic_flux / isq::electric_current>;
inline constexpr auto electric_power = kind<"electric_power", isq::power>;

// Light (ISO 80000-7), photopic vision.
inline constexpr auto luminous_flux = kind<"luminous_flux", isq::luminous_intensity * isq::solid_angular_measure>;
inline constexpr auto illuminance   = kind<"illuminance", isq::luminous_flux / isq::area>;

// Physical chemistry and ionizing radiation (ISO 80000-9, -10).
inline constexpr auto catalytic_activity = kind<"catalytic_activity", isq::amount_of_substance / isq::duration>;
inline constexpr auto activity           = kind<"activity", inverse(isq::duration)>;
inline constexpr auto absorbed_dose      = kind<"absorbed_dose", isq::energy / isq::mass>;
inline constexpr auto ionizing_radiation_quality_factor =
    kind<"ionizing_radiation_quality_factor", isq::dimensionless>;
inline constexpr auto dose_equivalent =
    kind<"dose_equivalent", isq::absorbed_dose * isq::ionizing_radiation_quality_factor>;
} // namespace isq

// SI prefixes. Past 10^±18 a prefix needs a grid number past 64 bits.
namespace si {
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
inline constexpr auto second = unit<"s", isq::duration>;
inline constexpr auto metre  = unit<"m", isq::length>;
// (The gram is 1/1000 of the coherent unit of mass, which the kilogram names.)
inline constexpr auto gram =
    detail::unit_tag<detail::definition<detail::unit_def, "g", detail::mag<1_g / 1000_g>, isq::mass>::value>;
inline constexpr auto kilogram = kilo(gram);
} // namespace si
namespace si {
inline constexpr auto ampere        = unit<"A", isq::electric_current>;
inline constexpr auto absolute_zero = origin<"absolute_zero", isq::thermodynamic_temperature>;
inline constexpr auto kelvin        = unit<"K", isq::thermodynamic_temperature, {.origin = si::absolute_zero}>;
inline constexpr auto ice_point     = origin<"ice_point", si::absolute_zero + just<27315, 100> * si::kelvin>;
inline constexpr auto mole          = unit<"mol", isq::amount_of_substance>;
inline constexpr auto candela       = unit<"cd", isq::luminous_intensity>;

// SI derived units with special names.
inline constexpr auto radian = unit<"rad", si::metre / si::metre, {.kind = isq::angular_measure}>;
inline constexpr auto steradian =
    unit<"sr", square(si::metre) / square(si::metre), {.kind = isq::solid_angular_measure}>;
inline constexpr auto hertz          = unit<"Hz", one / si::second, {.kind = isq::frequency}>;
inline constexpr auto newton         = unit<"N", si::kilogram * si::metre / square(si::second), {.kind = isq::force}>;
inline constexpr auto pascal         = unit<"Pa", si::newton / square(si::metre), {.kind = isq::pressure}>;
inline constexpr auto joule          = unit<"J", si::newton * si::metre, {.kind = isq::energy}>;
inline constexpr auto watt           = unit<"W", si::joule / si::second>;
inline constexpr auto coulomb        = unit<"C", si::ampere * si::second>;
inline constexpr auto volt           = unit<"V", si::watt / si::ampere, {.kind = isq::electric_potential}>;
inline constexpr auto farad          = unit<"F", si::coulomb / si::volt, {.kind = isq::capacitance}>;
inline constexpr auto ohm            = unit<"Ω", si::volt / si::ampere, {.ascii = "ohm", .kind = isq::impedance}>;
inline constexpr auto siemens        = unit<"S", one / si::ohm, {.kind = isq::admittance}>;
inline constexpr auto weber          = unit<"Wb", si::volt * si::second>;
inline constexpr auto tesla          = unit<"T", si::weber / square(si::metre), {.kind = isq::magnetic_flux_density}>;
inline constexpr auto henry          = unit<"H", si::weber / si::ampere>;
inline constexpr auto degree_Celsius = unit<"℃", si::kelvin, {.ascii = "`C", .origin = si::ice_point}>;
inline constexpr auto lumen          = unit<"lm", si::candela * si::steradian, {.kind = isq::luminous_flux}>;
inline constexpr auto lux            = unit<"lx", si::lumen / square(si::metre), {.kind = isq::illuminance}>;
inline constexpr auto becquerel      = unit<"Bq", one / si::second, {.kind = isq::activity}>;
inline constexpr auto gray           = unit<"Gy", si::joule / si::kilogram, {.kind = isq::absorbed_dose}>;
inline constexpr auto sievert        = unit<"Sv", si::joule / si::kilogram, {.kind = isq::dose_equivalent}>;
inline constexpr auto katal          = unit<"kat", si::mole / si::second, {.kind = isq::catalytic_activity}>;

// Non-SI units accepted for use with the SI.
inline constexpr auto minute            = unit<"min", detail::mag<60> * si::second>;
inline constexpr auto hour              = unit<"h", detail::mag<60> * si::minute>;
inline constexpr auto day               = unit<"d", detail::mag<24> * si::hour>;
inline constexpr auto astronomical_unit = unit<"au", detail::mag<149'597'870'700> * si::metre>;
inline constexpr auto degree =
    unit<"°", detail::mag<1_g / 180_g> * detail::mag_pi * si::radian, {.ascii = "deg", .space_before = false}>;
inline constexpr auto arcminute =
    unit<"′", detail::mag<1_g / 60_g> * si::degree, {.ascii = "'", .space_before = false}>;
inline constexpr auto arcsecond =
    unit<"″", detail::mag<1_g / 60_g> * si::arcminute, {.ascii = "''", .space_before = false}>;
inline constexpr auto are     = unit<"a", square(si::deca(si::metre))>;
inline constexpr auto litre   = unit<"L", cubic(si::deci(si::metre))>;
inline constexpr auto tonne   = unit<"t", detail::mag<1000> * si::kilogram>;
inline constexpr auto hectare = hecto(are);
} // namespace si
namespace si {
inline constexpr auto dalton       = unit<"Da", detail::mag<1.66053906892e-27_g> * si::kilogram>; // CODATA 2022
inline constexpr auto electronvolt = unit<"eV", detail::mag<1.602176634e-19_g> * si::joule>;
} // namespace si

// The defining constants of the SI (2019) and standard gravity, as units:
// just<1> * si2019::speed_of_light_in_vacuum is c, exactly, and .in(m/s)
// gives its number. h, e, k and N_A need grid numbers past 64 bits.
namespace si2019 {
inline constexpr auto hyperfine_structure_transition_frequency_of_cs =
    unit<"Δν_Cs", detail::mag<9'192'631'770> * si::hertz, {.ascii = "dv_Cs"}>;
inline constexpr auto speed_of_light_in_vacuum = unit<"c", detail::mag<299'792'458>*(si::metre / si::second)>;
inline constexpr auto luminous_efficacy        = unit<"K_cd", detail::mag<683>*(si::lumen / si::watt)>;
inline constexpr auto standard_gravity =
    unit<"g₀", detail::mag<980665_g / 100000_g>*(si::metre / square(si::second)), {.ascii = "g_0"}>;
inline constexpr auto planck_constant    = unit<"h", detail::mag<6.62607015e-34_g> * si::joule * si::second>;
inline constexpr auto elementary_charge  = unit<"e", detail::mag<1.602176634e-19_g> * si::coulomb>;
inline constexpr auto boltzmann_constant = unit<"k", detail::mag<1.380649e-23_g>*(si::joule / si::kelvin)>;
inline constexpr auto avogadro_constant  = unit<"N_A", detail::mag<6.02214076e23_g> / si::mole>;
} // namespace si2019

// International (yard and pound agreement, 1959) and nautical units.
namespace international {
inline constexpr auto yard          = unit<"yd", detail::mag<9144_g / 10000_g> * si::metre>;
inline constexpr auto foot          = unit<"ft", detail::mag<1_g / 3_g> * international::yard>;
inline constexpr auto inch          = unit<"in", detail::mag<1_g / 12_g> * international::foot>;
inline constexpr auto mile          = unit<"mi", detail::mag<1760> * international::yard>;
inline constexpr auto nautical_mile = unit<"nmi", detail::mag<1852> * si::metre>;
inline constexpr auto pound         = unit<"lb", detail::mag<45359237_g / 100000000_g> * si::kilogram>;
inline constexpr auto ounce         = unit<"oz", detail::mag<1_g / 16_g> * international::pound>;
inline constexpr auto pound_force =
    unit<"lbf", detail::mag<980665_g / 100000_g>*(international::pound* si::metre / square(si::second))>;
inline constexpr auto knot = unit<"kn", international::nautical_mile / si::hour>;
inline constexpr auto zeroth_degree_Fahrenheit =
    origin<"zeroth_degree_Fahrenheit", si::absolute_zero + just<45967, 180> * si::kelvin>;
inline constexpr auto degree_Fahrenheit =
    unit<"℉", detail::mag<5_g / 9_g> * si::kelvin, {.ascii = "`F", .origin = international::zeroth_degree_Fahrenheit}>;
} // namespace international

// Scaled dimensionless units, as in mp-units: 8 % is the number 2/25.
inline constexpr auto percent   = unit<"%", detail::mag<0.01_g> * one>;
inline constexpr auto per_mille = unit<"‰", detail::mag<0.001_g> * one, {.ascii = "permille"}>;

} // namespace beman::inside::units

#endif // BEMAN_INSIDE_UNITS_SYSTEMS_HPP
