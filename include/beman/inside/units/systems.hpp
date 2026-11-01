// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The catalog: ISQ quantity specs, SI units and prefixes, si2019's defining
// constants, international units, and the dimensionless percent and
// per_mille. Part of <beman/inside/units.hpp>.
#ifndef BEMAN_INSIDE_UNITS_SYSTEMS_HPP
#define BEMAN_INSIDE_UNITS_SYSTEMS_HPP

#include <beman/inside/inside/detail/config.hpp>

#include <beman/inside/units/core.hpp>

namespace beman::inside::units {

//---------------------------------------------------------------------------
// The catalog: ISQ quantity specs, SI units and prefixes, international units.
// Each name is a tag object; its definition is a static object in *_defs.
//---------------------------------------------------------------------------
#define BEMAN_INSIDE_SPEC(name, ...)                         \
    namespace isq_defs {                                     \
    inline constexpr quantity_spec name{#name, __VA_ARGS__}; \
    }                                                        \
    namespace isq {                                          \
    inline constexpr auto name = spec_c<isq_defs::name>;     \
    }
#define BEMAN_INSIDE_ORIGIN(ns, name, ...)                      \
    namespace ns##_defs {                                       \
        inline constexpr point_origin name{#name, __VA_ARGS__}; \
    }                                                           \
    namespace ns {                                              \
    inline constexpr auto name = origin_c<ns##_defs::name>;     \
    }
#define BEMAN_INSIDE_UNIT(ns, name, ...)                  \
    namespace ns##_defs {                                 \
        inline constexpr named_unit name{__VA_ARGS__};    \
    }                                                     \
    namespace ns {                                        \
    inline constexpr auto name = unit_c<ns##_defs::name>; \
    }

// Base quantities (ISO 80000-1) and the dimensionless one.
BEMAN_INSIDE_SPEC(dimensionless, dim_one)
BEMAN_INSIDE_SPEC(length, dim_length)
BEMAN_INSIDE_SPEC(mass, dim_mass)
BEMAN_INSIDE_SPEC(duration, dim_time)
BEMAN_INSIDE_SPEC(electric_current, dim_electric_current)
BEMAN_INSIDE_SPEC(thermodynamic_temperature, dim_thermodynamic_temperature)
BEMAN_INSIDE_SPEC(amount_of_substance, dim_amount_of_substance)
BEMAN_INSIDE_SPEC(luminous_intensity, dim_luminous_intensity)
namespace isq {
inline constexpr auto time = duration;
}

// Space and time (ISO 80000-3).
BEMAN_INSIDE_SPEC(width, isq::length)
BEMAN_INSIDE_SPEC(thickness, isq::width)
BEMAN_INSIDE_SPEC(diameter, isq::width)
BEMAN_INSIDE_SPEC(radius, isq::width)
BEMAN_INSIDE_SPEC(altitude, isq::length)
BEMAN_INSIDE_SPEC(height, isq::altitude)
BEMAN_INSIDE_SPEC(path_length, isq::length)
BEMAN_INSIDE_SPEC(arc_length, isq::path_length)
BEMAN_INSIDE_SPEC(distance, isq::path_length)
BEMAN_INSIDE_SPEC(radial_distance, isq::distance)
BEMAN_INSIDE_SPEC(wavelength, isq::length)
BEMAN_INSIDE_SPEC(displacement, isq::length)
BEMAN_INSIDE_SPEC(position_vector, isq::displacement)
BEMAN_INSIDE_SPEC(area, pow<2>(isq::length))
BEMAN_INSIDE_SPEC(volume, pow<3>(isq::length))
BEMAN_INSIDE_SPEC(angular_measure, isq::dimensionless, isq::arc_length / isq::radius, is_kind)
BEMAN_INSIDE_SPEC(rotational_displacement, isq::angular_measure, isq::path_length / isq::radius)
BEMAN_INSIDE_SPEC(phase_angle, isq::angular_measure, is_kind)
BEMAN_INSIDE_SPEC(solid_angular_measure, isq::dimensionless, isq::area / pow<2>(isq::radius), is_kind)
BEMAN_INSIDE_SPEC(period_duration, isq::duration)
BEMAN_INSIDE_SPEC(time_constant, isq::duration)
BEMAN_INSIDE_SPEC(speed, isq::length / isq::duration)
BEMAN_INSIDE_SPEC(velocity, isq::speed, isq::displacement / isq::duration)
BEMAN_INSIDE_SPEC(acceleration, isq::velocity / isq::duration)
BEMAN_INSIDE_SPEC(acceleration_of_free_fall, isq::acceleration)
BEMAN_INSIDE_SPEC(angular_velocity, isq::rotational_displacement / isq::duration)
BEMAN_INSIDE_SPEC(angular_acceleration, isq::angular_velocity / isq::duration)
BEMAN_INSIDE_SPEC(frequency, inverse(isq::period_duration))
BEMAN_INSIDE_SPEC(rotation, isq::dimensionless)
BEMAN_INSIDE_SPEC(rotational_frequency, isq::rotation / isq::duration)
BEMAN_INSIDE_SPEC(angular_frequency, isq::phase_angle / isq::duration)

// Mechanics (ISO 80000-4).
BEMAN_INSIDE_SPEC(mass_density, isq::mass / isq::volume)
BEMAN_INSIDE_SPEC(momentum, isq::mass* isq::velocity)
BEMAN_INSIDE_SPEC(force, isq::mass* isq::length / pow<2>(isq::duration))
BEMAN_INSIDE_SPEC(weight, isq::force, isq::mass* isq::acceleration_of_free_fall)
BEMAN_INSIDE_SPEC(impulse, isq::momentum, isq::force* isq::duration)
BEMAN_INSIDE_SPEC(moment_of_force, isq::position_vector* isq::force)
BEMAN_INSIDE_SPEC(torque, isq::moment_of_force)
BEMAN_INSIDE_SPEC(pressure, isq::force / isq::area)
BEMAN_INSIDE_SPEC(gauge_pressure, isq::pressure)
BEMAN_INSIDE_SPEC(energy, isq::mass* pow<2>(isq::length) / pow<2>(isq::duration))
BEMAN_INSIDE_SPEC(mechanical_work, isq::energy, isq::force* isq::displacement)
BEMAN_INSIDE_SPEC(mechanical_energy, isq::mechanical_work)
BEMAN_INSIDE_SPEC(potential_energy, isq::mechanical_energy)
BEMAN_INSIDE_SPEC(kinetic_energy, isq::mechanical_energy, isq::mass* pow<2>(isq::speed))
BEMAN_INSIDE_SPEC(power, isq::mass* pow<2>(isq::length) / pow<3>(isq::duration))
BEMAN_INSIDE_SPEC(mechanical_power, isq::power, isq::force* isq::velocity)
BEMAN_INSIDE_SPEC(action, isq::energy* isq::duration)
BEMAN_INSIDE_SPEC(mass_flow_rate, isq::mass / isq::duration)
BEMAN_INSIDE_SPEC(volume_flow_rate, isq::volume / isq::duration)
namespace isq {
inline constexpr auto work = mechanical_work;
}

// Thermodynamics (ISO 80000-5).
BEMAN_INSIDE_SPEC(internal_energy, isq::energy)
BEMAN_INSIDE_SPEC(heat, isq::internal_energy)
BEMAN_INSIDE_SPEC(enthalpy, isq::internal_energy)
BEMAN_INSIDE_SPEC(heat_capacity, isq::heat / isq::thermodynamic_temperature)
BEMAN_INSIDE_SPEC(entropy, isq::heat / isq::thermodynamic_temperature)
BEMAN_INSIDE_SPEC(heat_flow_rate, isq::heat / isq::duration)

// Electromagnetism (IEC 80000-6).
BEMAN_INSIDE_SPEC(electric_charge, isq::electric_current* isq::duration)
BEMAN_INSIDE_SPEC(electric_potential, isq::energy / (isq::electric_current * isq::duration))
BEMAN_INSIDE_SPEC(voltage, isq::electric_potential)
BEMAN_INSIDE_SPEC(capacitance, isq::electric_current* isq::duration / isq::electric_potential)
BEMAN_INSIDE_SPEC(impedance, isq::electric_potential / isq::electric_current)
BEMAN_INSIDE_SPEC(resistance, isq::impedance, isq::voltage / isq::electric_current)
BEMAN_INSIDE_SPEC(admittance, inverse(isq::impedance))
BEMAN_INSIDE_SPEC(conductance, isq::admittance, inverse(isq::resistance))
BEMAN_INSIDE_SPEC(magnetic_flux_density, isq::mass / (isq::electric_current * pow<2>(isq::duration)))
BEMAN_INSIDE_SPEC(magnetic_flux, isq::magnetic_flux_density* isq::area)
BEMAN_INSIDE_SPEC(inductance, isq::magnetic_flux / isq::electric_current)
BEMAN_INSIDE_SPEC(electric_power, isq::power)

// Light (ISO 80000-7), photopic vision.
BEMAN_INSIDE_SPEC(luminous_flux, isq::luminous_intensity* isq::solid_angular_measure)
BEMAN_INSIDE_SPEC(illuminance, isq::luminous_flux / isq::area)

// Physical chemistry and ionizing radiation (ISO 80000-9, -10).
BEMAN_INSIDE_SPEC(catalytic_activity, isq::amount_of_substance / isq::duration)
BEMAN_INSIDE_SPEC(activity, inverse(isq::duration))
BEMAN_INSIDE_SPEC(absorbed_dose, isq::energy / isq::mass)
BEMAN_INSIDE_SPEC(ionizing_radiation_quality_factor, isq::dimensionless)
BEMAN_INSIDE_SPEC(dose_equivalent, isq::absorbed_dose* isq::ionizing_radiation_quality_factor)

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
} // namespace si

// SI base units.
BEMAN_INSIDE_UNIT(si, second, "s", isq::duration)
BEMAN_INSIDE_UNIT(si, metre, "m", isq::length)
BEMAN_INSIDE_UNIT(si, gram, "g", mag<rational{1, 1000}>, isq::mass)
namespace si {
inline constexpr auto kilogram = kilo(gram);
}
BEMAN_INSIDE_UNIT(si, ampere, "A", isq::electric_current)
BEMAN_INSIDE_ORIGIN(si, absolute_zero, dim_thermodynamic_temperature)
BEMAN_INSIDE_ORIGIN(si, ice_point, dim_thermodynamic_temperature, &si_defs::absolute_zero, rational{27315, 100})
BEMAN_INSIDE_UNIT(si, kelvin, "K", isq::thermodynamic_temperature, {.origin = si::absolute_zero})
BEMAN_INSIDE_UNIT(si, mole, "mol", isq::amount_of_substance)
BEMAN_INSIDE_UNIT(si, candela, "cd", isq::luminous_intensity)

// SI derived units with special names.
BEMAN_INSIDE_UNIT(si, radian, "rad", si::metre / si::metre, isq::angular_measure)
BEMAN_INSIDE_UNIT(si, steradian, "sr", square(si::metre) / square(si::metre), isq::solid_angular_measure)
BEMAN_INSIDE_UNIT(si, hertz, "Hz", one / si::second, isq::frequency)
BEMAN_INSIDE_UNIT(si, newton, "N", si::kilogram* si::metre / square(si::second), isq::force)
BEMAN_INSIDE_UNIT(si, pascal, "Pa", si::newton / square(si::metre), isq::pressure)
BEMAN_INSIDE_UNIT(si, joule, "J", si::newton* si::metre, isq::energy)
BEMAN_INSIDE_UNIT(si, watt, "W", si::joule / si::second)
BEMAN_INSIDE_UNIT(si, coulomb, "C", si::ampere* si::second)
BEMAN_INSIDE_UNIT(si, volt, "V", si::watt / si::ampere, isq::electric_potential)
BEMAN_INSIDE_UNIT(si, farad, "F", si::coulomb / si::volt, isq::capacitance)
BEMAN_INSIDE_UNIT(si, ohm, "Ω", si::volt / si::ampere, isq::impedance, {.ascii = "ohm"})
BEMAN_INSIDE_UNIT(si, siemens, "S", one / si::ohm, isq::admittance)
BEMAN_INSIDE_UNIT(si, weber, "Wb", si::volt* si::second)
BEMAN_INSIDE_UNIT(si, tesla, "T", si::weber / square(si::metre), isq::magnetic_flux_density)
BEMAN_INSIDE_UNIT(si, henry, "H", si::weber / si::ampere)
BEMAN_INSIDE_UNIT(si, degree_Celsius, "℃", si::kelvin, {.ascii = "`C", .origin = si::ice_point})
BEMAN_INSIDE_UNIT(si, lumen, "lm", si::candela* si::steradian, isq::luminous_flux)
BEMAN_INSIDE_UNIT(si, lux, "lx", si::lumen / square(si::metre), isq::illuminance)
BEMAN_INSIDE_UNIT(si, becquerel, "Bq", one / si::second, isq::activity)
BEMAN_INSIDE_UNIT(si, gray, "Gy", si::joule / si::kilogram, isq::absorbed_dose)
BEMAN_INSIDE_UNIT(si, sievert, "Sv", si::joule / si::kilogram, isq::dose_equivalent)
BEMAN_INSIDE_UNIT(si, katal, "kat", si::mole / si::second, isq::catalytic_activity)

// Non-SI units accepted for use with the SI.
BEMAN_INSIDE_UNIT(si, minute, "min", mag<60>* si::second)
BEMAN_INSIDE_UNIT(si, hour, "h", mag<60>* si::minute)
BEMAN_INSIDE_UNIT(si, day, "d", mag<24>* si::hour)
BEMAN_INSIDE_UNIT(si, astronomical_unit, "au", mag<149'597'870'700>* si::metre)
BEMAN_INSIDE_UNIT(si, degree, "°", mag_pi* mag<rational{1, 180}>* si::radian, {.ascii = "deg", .space_before = false})
BEMAN_INSIDE_UNIT(si, arcminute, "′", mag<rational{1, 60}>* si::degree, {.ascii = "'", .space_before = false})
BEMAN_INSIDE_UNIT(si, arcsecond, "″", mag<rational{1, 60}>* si::arcminute, {.ascii = "''", .space_before = false})
BEMAN_INSIDE_UNIT(si, are, "a", square(si::deca(si::metre)))
BEMAN_INSIDE_UNIT(si, litre, "L", cubic(si::deci(si::metre)))
BEMAN_INSIDE_UNIT(si, tonne, "t", mag<1000>* si::kilogram)
namespace si {
inline constexpr auto hectare = hecto(are);
}
BEMAN_INSIDE_UNIT(si, dalton, "Da", mag<1.66053906892e-27_g>* si::kilogram) // CODATA 2022
BEMAN_INSIDE_UNIT(si, electronvolt, "eV", mag<1.602176634e-19_g>* si::joule)

// The defining constants of the SI (2019) and standard gravity, as units:
// just<1> * si2019::speed_of_light_in_vacuum is c, exactly, and .in(m/s)
// gives its number. h, e, k and N_A need grid numbers past 64 bits.
BEMAN_INSIDE_UNIT(
    si2019, hyperfine_structure_transition_frequency_of_cs, "Δν_Cs", mag<9'192'631'770>* si::hertz, {.ascii = "dv_Cs"})
BEMAN_INSIDE_UNIT(si2019, speed_of_light_in_vacuum, "c", mag<299'792'458>* si::metre / si::second)
BEMAN_INSIDE_UNIT(si2019, luminous_efficacy, "K_cd", mag<683>* si::lumen / si::watt)
BEMAN_INSIDE_UNIT(
    si2019, standard_gravity, "g₀", mag<rational{980665, 100000}>* si::metre / square(si::second), {.ascii = "g_0"})
BEMAN_INSIDE_UNIT(si2019, planck_constant, "h", mag<6.62607015e-34_g>* si::joule* si::second)
BEMAN_INSIDE_UNIT(si2019, elementary_charge, "e", mag<1.602176634e-19_g>* si::coulomb)
BEMAN_INSIDE_UNIT(si2019, boltzmann_constant, "k", mag<1.380649e-23_g>* si::joule / si::kelvin)
BEMAN_INSIDE_UNIT(si2019, avogadro_constant, "N_A", mag<6.02214076e23_g> / si::mole)

// International (yard and pound agreement, 1959) and nautical units.
BEMAN_INSIDE_UNIT(international, yard, "yd", mag<rational{9144, 10000}>* si::metre)
BEMAN_INSIDE_UNIT(international, foot, "ft", mag<rational{1, 3}>* international::yard)
BEMAN_INSIDE_UNIT(international, inch, "in", mag<rational{1, 12}>* international::foot)
BEMAN_INSIDE_UNIT(international, mile, "mi", mag<1760>* international::yard)
BEMAN_INSIDE_UNIT(international, nautical_mile, "nmi", mag<1852>* si::metre)
BEMAN_INSIDE_UNIT(international, pound, "lb", mag<rational{45359237, 100000000}>* si::kilogram)
BEMAN_INSIDE_UNIT(international, ounce, "oz", mag<rational{1, 16}>* international::pound)
BEMAN_INSIDE_UNIT(international,
                  pound_force,
                  "lbf",
                  mag<rational{980665, 100000}>* international::pound* si::metre / square(si::second))
BEMAN_INSIDE_UNIT(international, knot, "kn", international::nautical_mile / si::hour)
BEMAN_INSIDE_ORIGIN(international,
                    zeroth_degree_Fahrenheit,
                    dim_thermodynamic_temperature,
                    &si_defs::absolute_zero,
                    rational{45967, 180})
BEMAN_INSIDE_UNIT(international,
                  degree_Fahrenheit,
                  "℉",
                  mag<rational{5, 9}>* si::kelvin,
                  {.ascii = "`F", .origin = international::zeroth_degree_Fahrenheit})

// Scaled dimensionless units, as in mp-units: 8 % is the number 2/25.
namespace dimensionless_defs {
inline constexpr named_unit percent{"%", mag<rational{1, 100}> * one};
inline constexpr named_unit per_mille{"‰", mag<rational{1, 1000}> * one, {.ascii = "permille"}};
} // namespace dimensionless_defs
inline constexpr auto percent   = unit_c<dimensionless_defs::percent>;
inline constexpr auto per_mille = unit_c<dimensionless_defs::per_mille>;

#undef BEMAN_INSIDE_UNIT
#undef BEMAN_INSIDE_ORIGIN
#undef BEMAN_INSIDE_SPEC

} // namespace beman::inside::units

#endif // BEMAN_INSIDE_UNITS_SYSTEMS_HPP
