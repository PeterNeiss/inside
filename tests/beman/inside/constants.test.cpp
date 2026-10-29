// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// constants.hpp: mathematical constants (math::) and physical constants as
// quantities (units::constants::), exact where the SI defines them.

#include <beman/inside/constants.hpp>
#include <beman/inside/units_math.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <version> // __cpp_lib_format
#ifdef __cpp_lib_format
    #include <format>
#endif

using namespace beman::inside;
using namespace beman::inside::units;
namespace pc = beman::inside::units::constants;

namespace {
// Every rational n/d is within 2^-57 of its constant v, checked exactly (no
// long double, which is a double on some platforms). ref = floor(v·10^18), so
// v lies in [ref, ref + 1]·10^-18 and |n/d − v| ≤ |n·10^18 − ref·d|/(d·10^18)
// + 10^-18. The test asks for that bound to be below 2^-57, in 256 bits.
void expect_close(const auto& c, std::uint64_t ref) {
    using W          = beman::inside::detail::wide_sint<4>;
    const rational r = rational{c};
    const W        n{r.Numerator};
    const W        d{static_cast<std::uint64_t>(r.Denominator)};
    const W        scale{1'000'000'000'000'000'000ULL};
    W              diff = n * scale - W{ref} * d;
    if (diff < W{0})
        diff = -diff;
    EXPECT_TRUE((diff << 57) + (d << 57) < d * scale) << ref;
}
} // namespace

TEST(ConstantsTest, mathematical_constants_are_close_rationals) {
    expect_close(math::pi, 3141592653589793238ULL);
    expect_close(math::two_pi, 6283185307179586476ULL);
    expect_close(math::half_pi, 1570796326794896619ULL);
    expect_close(math::inv_pi, 318309886183790671ULL);
    expect_close(math::inv_sqrt_pi, 564189583547756286ULL);
    expect_close(math::e, 2718281828459045235ULL);
    expect_close(math::log2e, 1442695040888963407ULL);
    expect_close(math::log10e, 434294481903251827ULL);
    expect_close(math::ln2, 693147180559945309ULL);
    expect_close(math::ln10, 2302585092994045684ULL);
    expect_close(math::sqrt2, 1414213562373095048ULL);
    expect_close(math::sqrt3, 1732050807568877293ULL);
    expect_close(math::inv_sqrt2, 707106781186547524ULL);
    expect_close(math::inv_sqrt3, 577350269189625764ULL);
    expect_close(math::phi, 1618033988749894848ULL);
    expect_close(math::egamma, 577215664901532860ULL);
    expect_close(math::catalan, 915965594177219015ULL);
    expect_close(math::zeta3, 1202056903159594285ULL);
}

TEST(ConstantsTest, physical_constants_have_their_kind_and_value) {
    static_assert(pc::speed_of_light_in_vacuum.quantity_spec == isq::speed);
    static_assert(pc::standard_gravity.quantity_spec == isq::acceleration_of_free_fall);
    static_assert(pc::standard_atmosphere.quantity_spec == isq::pressure);
    static_assert(pc::characteristic_impedance_of_vacuum.quantity_spec == isq::impedance);
    EXPECT_EQ(to_string(pc::speed_of_light_in_vacuum.in(si::metre / si::second)), "299792458 m/s");
    EXPECT_EQ(to_string(pc::standard_atmosphere.in(si::pascal)), "101325 Pa");
    EXPECT_EQ(to_string(pc::standard_gravity.in(si::metre / square(si::second))), "9.80665 m/s²");
    EXPECT_EQ(to_string(pc::hyperfine_structure_transition_frequency_of_cs.in(si::hertz)), "9192631770 Hz");
    EXPECT_EQ(to_string(pc::fine_structure_constant.in(one)), "0.0072973525643");
    EXPECT_EQ(to_string(pc::rydberg_constant.in(one / si::metre)), "10973731.568157 m⁻¹");
    EXPECT_EQ(to_string(pc::characteristic_impedance_of_vacuum.in(si::ohm)), "376.730313412 Ω");
#ifdef __cpp_lib_format
    EXPECT_EQ(
        std::format("{:.5e}", pc::gravitational_constant.in(cubic(si::metre) / (si::kilogram * square(si::second)))),
        "6.67430e-11 m³/(kg s²)");
    EXPECT_EQ(std::format("{:.11e}", pc::vacuum_magnetic_permeability.in(si::newton / square(si::ampere))),
              "1.25663706127e-06 N/A²");
#endif
}

TEST(ConstantsTest, si2019_defining_constants_are_exact) {
    static_assert(pc::planck_constant.quantity_spec == isq::action);
    static_assert(pc::elementary_charge.quantity_spec == isq::electric_charge);
    EXPECT_EQ(std::format("{:.8e}", pc::planck_constant.in(si::joule * si::second)), "6.62607015e-34 J s");
    EXPECT_EQ(std::format("{:.9e}", pc::elementary_charge.in(si::coulomb)), "1.602176634e-19 C");
    EXPECT_EQ(std::format("{:.6e}", pc::boltzmann_constant.in(si::joule / si::kelvin)), "1.380649e-23 J/K");
    EXPECT_EQ(std::format("{:.8e}", pc::avogadro_constant.in(one / si::mole)), "6.02214076e+23 mol⁻¹");
    EXPECT_EQ(std::format("{:.10e}", pc::electron_mass.in(si::kilogram)), "9.1093837139e-31 kg");
    EXPECT_EQ(std::format("{:.11e}", pc::atomic_mass_constant.in(si::kilogram)), "1.66053906892e-27 kg");
    EXPECT_EQ(std::format("{:.11e}", (just<1> * si::dalton).in(si::kilogram)), "1.66053906892e-27 kg");
}

TEST(ConstantsTest, derived_constants_are_exact_products) {
    // R = N_A·k and F = N_A·e, in full.
    EXPECT_EQ(to_string(pc::molar_gas_constant.in(si::joule / (si::mole * si::kelvin))), "8.31446261815324 J/(K mol)");
    EXPECT_EQ(to_string(pc::faraday_constant.in(si::coulomb / si::mole)), "96485.3321233100184 C/mol");
    EXPECT_TRUE(pc::molar_gas_constant == pc::avogadro_constant * pc::boltzmann_constant);
    EXPECT_TRUE(pc::faraday_constant == pc::avogadro_constant * pc::elementary_charge);
    // K_J·Φ₀ = 1 and R_K·G₀ = 2: dimension one, exactly.
    EXPECT_EQ(to_string((pc::josephson_constant * pc::magnetic_flux_quantum).in(one)), "1");
    EXPECT_EQ(to_string((pc::von_klitzing_constant * pc::conductance_quantum).in(one)), "2");
    // ħ = h/2π: a factor of π rounds once, through value_cast.
    using fine = inside<{{0, 1}, 1e-45_g}, round_nearest>;
    EXPECT_EQ(std::format("{:.9e}", value_cast<si::joule * si::second, fine>(pc::reduced_planck_constant)),
              "1.054571818e-34 J s");
    // E = h·ν for 540 THz, exactly.
    const quantity<si::tera(si::hertz), inside<{0, 1000}>> nu{540};
    EXPECT_EQ(std::format("{:.9e}", (pc::planck_constant * nu).in(si::joule)), "3.578077881e-19 J");
    // c₂ = hc/k.
    EXPECT_EQ(std::format("{:.9e}", pc::second_radiation_constant.in(si::metre * si::kelvin)), "1.438776878e-02 K m");
}
