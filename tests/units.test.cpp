// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Units: dimensions, units, conversions, kinds, the API, inside's algorithms
// on quantities, math on quantities, and the physical constants.

#include <inside/units.hpp>
#include <inside/units/constants.hpp>
#include <inside/units/io.hpp>
#include <inside/units/math.hpp>

#include "support.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <format>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

// units.hpp: dimensions, units, conversions, quantity kinds, arithmetic, text.
namespace units_tests {
using namespace ins;

namespace {
constexpr auto km = si::kilo(si::metre);
constexpr auto mm = si::milli(si::metre);

template <class A, class B>
consteval bool addable() {
    return ins::detail::common_kind(std::remove_cvref_t<A>::reference, std::remove_cvref_t<B>::reference).Ok;
}
} // namespace

TEST(UnitsTest, dimensions_and_magnitudes) {
    static_assert(dimension_of(si::newton.value) ==
                  base_dimension<"mass"> * base_dimension<"length"> / base_dimension<"time">.pow(2));
    static_assert(magnitude_of(km.value) == magnitude{grid_number{1000}});
    static_assert(magnitude_of(si::kilogram.value).is_one());
    static_assert(magnitude_of(si::gram.value) == magnitude{grid_number{rational{1, 1000}}});
    static_assert(magnitude_of(si::hour.value) == magnitude{grid_number{3600}});
    static_assert(magnitude_of(international::inch.value) == magnitude{grid_number{rational{254, 10000}}});
    static_assert(magnitude_of(si::degree.value).Pi == 1);
    // m·s/s is m again, and the factors are kept in one order.
    static_assert(si::metre * si::second / si::second == si::metre);
    static_assert(std::is_same_v<decltype(si::metre * si::second), decltype(si::second * si::metre)>);
    // J is not N·m as a unit (another symbol), but the same magnitude and dimension.
    static_assert(dimension_of(si::joule.value) == dimension_of((si::newton * si::metre).value));
}

TEST(UnitsTest, conversion_rescales_the_grid_and_keeps_the_raw) {
    const id<"[0..10] [km]"> d{7, km};
    const auto                          m = d.in(si::metre);
    using M                               = std::remove_cvref_t<decltype(m)>::rep;
    static_assert(lower_of<M> == 0 && upper_of<M> == 10000 && notch_of<M> == 1000);
    EXPECT_EQ(m.numerical_value_in(si::metre), 7000);
    EXPECT_EQ(m.numerical_value_in(si::metre).raw(), d.numerical_value_in(km).raw());
    static_assert(sizeof(M) == sizeof(id<"[0..10]">));
    // m → km: notch 1/1000, exact.
    const id<"[0..1500] [m]"> l{1234, si::metre};
    EXPECT_EQ(rational{l.numerical_value_in(km)}, rational(1234, 1000));
    // inch → mm, exact (25.4).
    const id<"[0..12] [in]"> in{3, international::inch};
    EXPECT_EQ(rational{in.numerical_value_in(mm)}, rational(762, 10));
    EXPECT_EQ(rational{in.numerical_value_in(international::foot)}, rational(1, 4));
}

// A number goes into and out of a quantity with its unit named.
TEST(UnitsTest, numbers_name_their_unit) {
    using metres = id<"[0..5000] [m]">;
    const metres a{42, si::metre};
    EXPECT_TRUE(a == 42_q * si::metre);
    // An inside in another unit of the quantity converts exactly; a policy for
    // the one store, and the spec[unit] form.
    const metres b{id<"[0..4]">{2}, si::kilo(si::metre)};
    EXPECT_TRUE(b == 2000_q * si::metre);
    EXPECT_TRUE((metres{9000, si::metre, clamp} == 5000_q * si::metre));
    const id<"[0..10] [m]{height}"> h{3, isq::height[si::metre]};
    EXPECT_EQ(h.numerical_value_in(si::metre), 3);
    // Copy-list-initialization names the unit too.
    const metres c = {7, si::metre};
    EXPECT_TRUE(c == 7_q * si::metre);
    // try_make and the predicates take the unit as well.
    EXPECT_TRUE(metres::try_make(12, si::metre).has_value());
    EXPECT_EQ(metres::try_make(6000, si::metre).error(), error::overflow);
    EXPECT_TRUE(conversion_overflows<metres>(6000, si::metre));
    EXPECT_FALSE(conversion_overflows<metres>(id<"[0..4]">{4}, si::kilo(si::metre)));
    // Reading: in its own unit the number is the stored one, else rescaled.
    static_assert(std::is_same_v<decltype(a.numerical_value_in(si::metre)), metres::rep>);
    EXPECT_EQ(rational{b.numerical_value_in(si::kilo(si::metre))}, rational(2));
    // A number alone is no quantity, in or out.
    static_assert(!std::is_constructible_v<metres, int>);
    static_assert(!std::is_convertible_v<int, metres>);
    static_assert(!std::is_constructible_v<metres, int, decltype(si::second)>);
}

TEST(UnitsTest, sums_run_in_the_common_unit) {
    const auto five_km = 5_q * km;
    const auto three_m = 3_q * si::metre;
    const auto s       = five_km + three_m;
    static_assert(std::remove_cvref_t<decltype(s)>::unit == si::metre.value);
    EXPECT_EQ(s.numerical_value_in(si::metre), 5003);
    EXPECT_EQ(to_string(s), "5003 m");
    // A range: the result grid is the exact hull, in metres.
    const id<"[0..10] [km]">           a{2, km};
    const id<"[0..10000] [m]"> b{500, si::metre};
    const auto                                    c = a + b;
    using C                                         = std::remove_cvref_t<decltype(c)>::rep;
    static_assert(lower_of<C> == 0 && upper_of<C> == 20000);
    EXPECT_EQ(c.numerical_value_in(si::metre), 2500);
    // Neither divides the other: ft + m runs in 1/1250 m (= 1/381 ft).
    const auto f = international::foot + si::metre;
    EXPECT_EQ(rational{f.numerical_value_in(si::metre)}, rational(1631, 1250)); // 1.3048 m
    EXPECT_EQ(unit_symbol(std::remove_cvref_t<decltype(f)>::unit), "[1/381·ft]");
    // The smaller unit is scaled, whichever side it is on: one type.
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(f)>,
                                 decltype(1_q * si::metre + 1_q * international::foot)>);
    // Comparisons across units.
    EXPECT_TRUE(km == 1000_q * si::metre);
    EXPECT_TRUE(999_q * si::metre < km);
}

TEST(UnitsTest, products_and_quotients) {
    const id<"[0..100] [m]"> d{42, si::metre};
    const id<"[1..10] [s]"> t{4, si::second};
    const auto                                  v = d / t; // 1 ≤ t: no zero divisor, a plain value
    EXPECT_EQ(rational{v.numerical_value_in(si::metre / si::second)}, rational(21, 2));
    EXPECT_EQ(to_string(v), "10.5 m/s");
    // A divisor whose grid holds 0: inside's expected.
    const id<"[0..10] [s]"> t0{0, si::second};
    const auto                                  e = d / t0;
    static_assert(ins::detail::is_expected_v<std::remove_cvref_t<decltype(e)>>);
    EXPECT_FALSE(e.has_value());
    // N·m is an energy, implicitly a joule.
    const auto                            w = (3_q * si::newton) * d;
    id<"[0..300] [J]"> j = w;
    EXPECT_EQ(j.numerical_value_in(si::joule), 126);
    // Scaling by a number.
    EXPECT_EQ((d * 2_q).numerical_value_in(si::metre), 84);
    EXPECT_EQ((d / 2_q).numerical_value_in(si::metre), 21);
    EXPECT_EQ(to_string(2_q / si::second), "2 s⁻¹");
}

TEST(UnitsTest, dimensionless_quantities_are_numbers) {
    const id<"[1..100] [kg]"> a{21, si::kilogram};
    const id<"[1..100] [kg]"> b{6, si::kilogram};
    // (a / b is a fraction of denominator up to 100 in [0, 100]: implicit)
    const id<"[0..100] <100>"> r = a / b; // 7/2, implicitly
    EXPECT_EQ(rational{r}, rational(7, 2));
    // km/m does not cancel to unit one: its number is ×1000.
    const auto           ratio = (3_q * km) / (2_q * si::metre);
    const id<"[1500]"> n     = ratio;
    EXPECT_EQ(n, 1500);
    // Into a grid that must round: explicit, through that inside's constructor.
    const auto x =
        *id<"[0..10] 1/10 round_half_away">::try_make((a / b * "1/3"_q).numerical_value_in(one));
    EXPECT_EQ(rational{x}, rational(12, 10));
    // An angle is a quantity, not a number.
    static_assert(!std::is_convertible_v<id<"[0..7] [rad]">, id<"[0..7]">>);
    static_assert(!std::is_constructible_v<id<"[0..7]">, id<"[0..7] [rad]">>);
    static_assert(!std::is_convertible_v<id<"[0..7] [m]">, id<"[0..7]">>);
}

TEST(UnitsTest, magnitudes_scale_the_unit_not_the_number) {
    const id<"[0..10] [s]"> t{3, si::second};
    const auto                                  p = pi * t;
    static_assert(magnitude_of(std::remove_cvref_t<decltype(p)>::unit).Pi == 1);
    EXPECT_EQ(p.numerical_value_in(pi * si::second), 3); // the number is untouched
    EXPECT_EQ(to_string(p), "3 [π·s]");
    EXPECT_EQ(unit_symbol(std::remove_cvref_t<decltype(p)>::unit, text_encoding::ascii), "[pi*s]");
    EXPECT_EQ(to_string(t * ins::detail::mag<2>), "3 [2·s]");
    EXPECT_EQ((t * ins::detail::mag<2>).numerical_value_in(si::second), 6);
    EXPECT_EQ(unit_symbol(std::remove_cvref_t<decltype(t / pi)>::unit), "[π⁻¹·s]");
    // π · π⁻¹ cancels: back to seconds, exactly.
    EXPECT_EQ((pi * t / pi).numerical_value_in(si::second), 3);
    // A magnitude times a number is that number in [π], then [π s].
    EXPECT_EQ(to_string(pi * 2_q * si::second), "2 [π·s]");
    EXPECT_EQ(to_string(2_q * pi), "2 [π]");
    EXPECT_EQ(unit_symbol(pi / si::metre), "[π·m⁻¹]");
}

TEST(UnitsTest, compound_assignment_and_casts) {
    id<"[0..10000] [m]"> d{100, si::metre};
    error                                   ec{}; // (a sum the grids cannot prove: reported)
    d.on_error(ec) += km;
    EXPECT_EQ(d.numerical_value_in(si::metre), 1100);
    d.on_error(ec) -= 100_q * si::metre;
    EXPECT_EQ(d.numerical_value_in(si::metre), 1000);
    EXPECT_EQ(ec, error{});
    const auto narrow = value_cast<id<"[0..2000]">>(d); // may not fit: an expected
    EXPECT_EQ(narrow->numerical_value_in(si::metre), 1000);
    EXPECT_EQ((value_cast<id<"[0..500]">>(d).error()), error::overflow);
    // In another unit: 1000 m is 1 km; 1001 m rounds onto whole km by F, and
    // is past a {0, 1} km grid.
    EXPECT_EQ((value_cast<km, id<"[0..5] 1/1000">>(d)->numerical_value_in(km)), 1);
    d.on_error(ec) += si::metre;
    EXPECT_EQ((value_cast<km, id<"[0..5] round_half_away">>(d)->numerical_value_in(km)), 1);
    EXPECT_EQ((value_cast<km, id<"[0..1] 1/1000">>(d).error()), error::overflow);
}

TEST(UnitsTest, kinds_convert_up_implicitly_and_down_explicitly) {
    using height_m = id<"[0..100] [m]{height}">;
    using length_m = id<"[0..100] [m]{length}">;
    using width_m  = id<"[0..100] [m]{width}">;
    static_assert(std::is_convertible_v<height_m, length_m>);
    static_assert(!std::is_convertible_v<length_m, height_m>);
    static_assert(!std::is_constructible_v<width_m, height_m>);
    const height_m h{3, isq::height[si::metre]};
    const length_m l = h;
    EXPECT_EQ(l.numerical_value_in(si::metre), 3);
    const auto back = kind_cast<isq::height>(l);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(back)>, height_m>);
    // A plain unit's kind (si::metre: any length) takes every length.
    static_assert(std::is_convertible_v<id<"[0..100] [m]">, height_m>);
    static_assert(std::is_convertible_v<height_m, id<"[0..100] [m]">>);
    // width + height: their common ancestor, length.
    const auto s = width_m{1, isq::width[si::metre]} + h;
    static_assert(std::remove_cvref_t<decltype(s)>::reference.kind()->Name == "length");
}

TEST(UnitsTest, derived_kinds_follow_their_recipes) {
    using W   = id<"[0..10] [m]{width}">;
    using H   = id<"[0..10] [m]{height}">;
    using A   = id<"[0..100] [m^2]{area}">;
    const A a = W{2, isq::width[si::metre]} * H{3, isq::height[si::metre]}; // width·height → area
    EXPECT_EQ(a.numerical_value_in(square(si::metre)), 6);
    // mass·speed² → kinetic energy; force·length → energy, but not torque.
    using kg_t = id<"[0..10] [kg]{mass}">;
    using v_t  = id<"[0..10] [m/s]{speed}">;
    using ke_t = id<"[0..1000] [J]{kinetic_energy}">;
    static_assert(std::is_convertible_v<decltype(kg_t{} * v_t{} * v_t{}), ke_t>);
    using f_t = id<"[0..10] [N]{force}">;
    using l_t = id<"[0..10] [m]{length}">;
    static_assert(std::is_convertible_v<decltype(f_t{} * l_t{}), id<"[0..100] [J]{energy}">>);
    static_assert(!std::is_convertible_v<decltype(f_t{} * l_t{}),
                                         id<"[0..100] [N*m]{torque}">>);
    // m/s (no kind) is any speed.
    static_assert(std::is_convertible_v<id<"[0..10] [m/s]">, v_t>);
}

TEST(UnitsTest, separate_kinds_do_not_mix) {
    const auto hz   = 1_q * si::hertz;
    const auto bq   = 1_q * si::becquerel;
    const auto rad  = 1_q * si::radian;
    const auto one_ = 1_q * (si::metre / si::metre);
    static_assert(!addable<decltype(hz), decltype(bq)>());
    static_assert(!addable<decltype(rad), decltype(one_)>());
    static_assert(addable<decltype(hz), decltype(1_q / si::second)>());
    static_assert(
        !std::is_convertible_v<id<"[0..10] [Hz]">, id<"[0..10] [Bq]">>);
    // Gy and Sv: both J/kg, different kinds.
    static_assert(!addable<decltype(1_q * si::gray), decltype(1_q * si::sievert)>());
    static_assert(!ins::detail::kind_casts(std::remove_cvref_t<decltype(rad)>::reference, isq::dimensionless.value));
}

TEST(UnitsTest, text) {
    EXPECT_EQ(unit_symbol(si::kilogram * square(si::metre) / square(si::second)), "kg·m²/s²");
    EXPECT_EQ(unit_symbol(si::kilogram * square(si::metre) / square(si::second), text_encoding::ascii), "kg*m^2/s^2");
    EXPECT_EQ(unit_symbol(si::joule / (si::kilogram * si::kelvin)), "J/(K·kg)");
    EXPECT_EQ(unit_symbol(si::micro(si::metre)), "µm");
    EXPECT_EQ(unit_symbol(si::micro(si::metre), text_encoding::ascii), "um");
    EXPECT_EQ(unit_symbol(si::ohm), "Ω");
    EXPECT_EQ(to_string(90_q * si::degree), "90°");
    EXPECT_EQ(to_string(3_q * km / si::hour), "3 km/h");
    EXPECT_EQ(to_string(3_q * ins::one), "3");
    std::ostringstream os;
    os << 5_q * si::newton;
    EXPECT_EQ(os.str(), "5 N");
}

TEST(UnitsTest, constants_are_units) {
    const auto c = (1_q * si2019::speed_of_light_in_vacuum).in(si::metre / si::second);
    EXPECT_EQ(c.numerical_value_in(si::metre / si::second), 299'792'458);
    EXPECT_EQ(to_string(2_q * si2019::speed_of_light_in_vacuum), "2 c");
    const auto g = (1_q * si2019::standard_gravity).in(si::metre / square(si::second));
    EXPECT_EQ(to_string(g), "9.80665 m/s²");
}

TEST(UnitsTest, big_grid_prefixes_and_units) {
    static_assert(magnitude_of(si::yocto(si::gram).value) == magnitude{1e-27_g});
    const auto e = (1_q * si::electronvolt).in(si::joule);
    EXPECT_EQ(to_string(e), "0.0000000000000000001602176634 J");
    // E = h·ν, exactly: one photon of the caesium frequency.
    const auto en =
        (1_q * si2019::planck_constant) * (1_q * si2019::hyperfine_structure_transition_frequency_of_cs);
    EXPECT_EQ(to_string(en.in(si::joule)), "0.00000000000000000000000609110229711386655 J");
}

TEST(UnitsTest, points_shift_their_grid_between_origins) {
    const auto t = point<si::degree_Celsius>(id<"[-40..60]">{20});
    static_assert(std::remove_cvref_t<decltype(t)>::origin->Name == "ice_point");
    EXPECT_EQ(to_string(t), "20 ℃");
    // From absolute zero, still in ℃: 293.15, on a shifted grid.
    const auto q = t.quantity_from(si::absolute_zero);
    using Q      = std::remove_cvref_t<decltype(q)>::rep;
    static_assert(lower_of<Q> == rational(23315, 100) && upper_of<Q> == rational(33315, 100));
    EXPECT_EQ(rational{q.numerical_value_in(si::degree_Celsius)}, rational(29315, 100));
    // As a kelvin point.
    const auto k = t.point_for(si::absolute_zero).in(si::kelvin);
    EXPECT_EQ(to_string(k), "293.15 K");
    EXPECT_TRUE(k == t);
    // 68 ℉ is 20 ℃.
    const auto f = point<international::degree_Fahrenheit>(id<"[0..100]">{68});
    EXPECT_TRUE(f == t);
    EXPECT_EQ(rational{f.point_for(si::ice_point)
                           .in(si::degree_Celsius)
                           .quantity_from_origin()
                           .numerical_value_in(si::degree_Celsius)},
              rational(20));
    // Implicit conversion to a kelvin point type.
    using kelvin_point    = id<"[0..400] 1/100 [K] @absolute_zero">;
    const kelvin_point kp = t;
    EXPECT_EQ(rational{kp.quantity_from_origin().numerical_value_in(si::kelvin)}, rational(29315, 100));
}

TEST(UnitsTest, points_from_a_number_and_their_unit) {
    using celsius = id<"[-40..60] 1/2 [deg_C] @ice_point round_half_away">;
    const celsius room{21.4, si::degree_Celsius}; // stored by the type's rounding
    EXPECT_EQ(to_string(room), "21.5 ℃");
    EXPECT_TRUE(room == point<si::degree_Celsius>(21.5_q));
    const std::vector<celsius> t = {{21.5, si::degree_Celsius}, {-5, si::degree_Celsius}};
    EXPECT_TRUE(t[1] < t[0]);
    const celsius hot{80, si::degree_Celsius, clamp}; // a one-shot policy, as for a quantity
    EXPECT_EQ(to_string(hot), "60 ℃");
    // Run-time numbers, as an inside takes them: an int under clamp, a double (maybe NaN) with a channel.
    volatile int    whole   = 90;
    volatile double reading = 70.2;
    error           ec{};
    const celsius   gauge {static_cast<int>(whole), si::degree_Celsius, clamp};
    const celsius   sensor{static_cast<double>(reading), si::degree_Celsius, make_policy<clamp>(ec)};
    EXPECT_EQ(to_string(gauge), "60 ℃");
    EXPECT_EQ(to_string(sensor), "60 ℃");
    EXPECT_EQ(ec, error{});
    static_assert(celsius{21.5, si::degree_Celsius} > celsius{-5, si::degree_Celsius});
    // Only the point's own unit: a number in K names no origin, a number alone no unit.
    static_assert(!std::constructible_from<celsius, int, decltype(si::kelvin)>);
    static_assert(!std::constructible_from<celsius, int, decltype(si::metre)>);
    static_assert(!std::constructible_from<celsius, int>);
}

TEST(UnitsTest, point_arithmetic) {
    auto       t  = point<si::degree_Celsius>(id<"[-40..60]">{20});
    const auto t2 = t + delta<si::kelvin>(5_q);
    EXPECT_EQ(to_string(t2), "25 ℃");
    const auto d = t2 - point<si::kelvin>(id<"[250..300]">{273}); // 25 ℃ − 273 K = 25.15 K
    EXPECT_EQ(rational{d.numerical_value_in(si::kelvin)}, rational(2515, 100));
    auto tc = point<si::degree_Celsius>(id<"[-40..60] clamp">{20}); // (+= names its range handling)
    tc += delta<si::degree_Celsius>(1_q);
    EXPECT_EQ(to_string(tc), "21 ℃");
    const auto z = si::absolute_zero + 300_q * si::kelvin;
    EXPECT_TRUE(z > t);
    // A unit without an origin counts from the implicit zero.
    const auto x = point<si::metre>(id<"[0..10]">{3});
    static_assert(std::remove_cvref_t<decltype(x)>::origin->Name.empty()); // origin_def::none
    EXPECT_EQ((x - point<si::metre>(1_q)).numerical_value_in(si::metre), 2);
}

TEST(UnitsTest, std_format) {
    EXPECT_EQ(std::format("{}", 5_q * si::kilo(si::metre)), "5 km");
    EXPECT_EQ(std::format("{:>4}", 5_q * si::newton), "   5 N");
    EXPECT_EQ(std::format("{}", 90_q * si::degree), "90°");
    const id<"[0..10] 1/4 [m/s]"> v{rational(9, 4), si::metre / si::second};
    EXPECT_EQ(std::format("{:.1f}", v), "2.2 m/s");
    EXPECT_EQ(std::format("{}", point<si::degree_Celsius>(id<"[-40..60]">{20})), "20 ℃");
    EXPECT_EQ(std::format("{}", 3_q * ins::one), "3");
}

TEST(UnitsTest, point_in_and_value_cast_on_points) {
    const auto oven = point<international::degree_Fahrenheit>(id<"[0..600]">{350});
    const auto c    = oven.point_in(si::degree_Celsius); // (350 − 32)·5/9
    static_assert(std::remove_cvref_t<decltype(c)>::origin->Name == "ice_point");
    EXPECT_EQ(rational{c.quantity_from_origin().numerical_value_in(si::degree_Celsius)}, rational(530, 3));
    EXPECT_EQ(to_string(c.point_in(si::kelvin)), "26989/60 K");      // 449.8166…: exact, so N/D
    const auto r = value_cast<id<"[0..300] round_half_away">>(c);    // may not fit: an expected
    EXPECT_EQ(to_string(*r), "177 ℃");
    const auto k =
        value_cast<si::kelvin, id<"[0..1000] 1/10 round_half_away">>(c.point_for(si::absolute_zero));
    EXPECT_EQ(to_string(k), "449.8 K");
}
} // namespace units_tests

// The units API review before 0.4: identity of specs and units, honest
// constraints with reasons, spec queries, value_cast to a quantity type,
// unit · magnitude, canonical common units, text, points.
namespace units_api_tests {
using namespace ins;

namespace {
constexpr auto km = si::kilo(si::metre);

// A spec that reuses a catalog name under another parent, and two units that
// differ only in the kind they measure.
inline constexpr auto length_of_width = kind<"length", isq::width>;
inline constexpr auto per_second_f    = unit<"Hz", one / si::second, {.kind = isq::frequency}>;
inline constexpr auto per_second_a    = unit<"Hz", one / si::second, {.kind = isq::activity}>;

// unit_options: an ASCII symbol, a kind, an origin, no space before.
inline constexpr auto turn_unit = unit<"tr", 2_q * pi * si::radian, {.space_before = false}>;
inline constexpr auto rpm_unit  = unit<"rpm", one / si::minute, {.kind = isq::frequency}>;

// A base dimension of your own.
inline constexpr auto currency_dim = base_dimension<"currency">;
inline constexpr auto money_kind   = kind<"money", currency_dim>;

// ADC counts: a dimension apart from SI's dimensionless one.
inline constexpr auto count_dim       = base_dimension<"count">;
inline constexpr auto adc_counts_kind = kind<"adc_counts", count_dim>;

// Eight base dimensions of your own, four in each of two named units; one is
// a class member.
inline constexpr auto gold_dim    = base_dimension<"gold">;
inline constexpr auto gold_kind   = kind<"gold", gold_dim>;
inline constexpr auto gold_u      = unit<"g$", gold_kind>;
inline constexpr auto health_dim  = base_dimension<"health">;
inline constexpr auto health_kind = kind<"health", health_dim>;
inline constexpr auto health_u    = unit<"HP", health_kind>;
inline constexpr auto mana_dim    = base_dimension<"mana">;
inline constexpr auto mana_kind   = kind<"mana", mana_dim>;
inline constexpr auto mana_u      = unit<"MP", mana_kind>;
struct stamina {
    static constexpr auto dim  = base_dimension<"stamina">;
    static constexpr auto spec = kind<"stamina", dim>;
    static constexpr auto st   = unit<"st", spec>;
};
inline constexpr auto xp_dim      = base_dimension<"xp">;
inline constexpr auto xp_kind     = kind<"xp", xp_dim>;
inline constexpr auto xp_u        = unit<"XP", xp_kind>;
inline constexpr auto pixel_dim   = base_dimension<"pixel">;
inline constexpr auto pixel_kind  = kind<"pixel", pixel_dim>;
inline constexpr auto pixel_u     = unit<"px", pixel_kind>;
inline constexpr auto packet_dim  = base_dimension<"packet">;
inline constexpr auto packet_kind = kind<"packet", packet_dim>;
inline constexpr auto packet_u    = unit<"pkt", packet_kind>;
inline constexpr auto token_dim   = base_dimension<"token">;
inline constexpr auto token_kind  = kind<"token", token_dim>;
inline constexpr auto token_u     = unit<"tok", token_kind>;

template <class A, class B>
concept can_add = requires(const A& a, const B& b) { a + b; };
template <class A, class B>
concept can_times = requires(const A& a, const B& b) { a * b; };
template <class A, class B>
consteval bool addable_units() {
    return can_add<A, B>;
}
template <class A, class B>
concept can_less = requires(const A& a, const B& b) { a < b; };
template <class A, class B>
concept can_equal = requires(const A& a, const B& b) { a == b; };
template <class A, class B>
concept can_compare = can_less<A, B> && can_less<B, A> && can_equal<A, B> && can_equal<B, A>;
template <class A, class B>
concept cannot_compare = !can_less<A, B> && !can_less<B, A> && !can_equal<A, B> && !can_equal<B, A>;
template <class A>
concept can_scale_raw = requires(const A& a) { a * 2; } || requires(const A& a) { 2 * a; } ||
                        requires(const A& a) { a / 2; } || requires(const A& a) { 2 / a; };
template <class A>
concept can_in_seconds = requires(const A& a) { a.in(si::second); };
template <class A>
concept can_in_radians = requires(const A& a) { a.in(si::radian); };
template <class A>
concept can_cast_to_duration = requires(const A& a) { kind_cast<isq::duration>(a); };
template <class A, class B>
concept can_atan2 = requires(const A& a, const B& b) { math::atan2(a, b); };
template <class Q, class A>
concept can_value_cast = requires(const A& a) { value_cast<Q>(a); };
template <class A, class B>
concept can_subtract = requires(const A& a, const B& b) { a - b; };
template <class P>
concept can_point_in_seconds = requires(const P& p) { p.point_in(si::second); };

using metres  = id<"[-10..10] 1/8 [m] round_half_away">;
using seconds = id<"[-10..10] 1/8 [s] round_half_away">;
using degrees = id<"[0..360] [deg]">;
} // namespace

TEST(UnitsApiTest, specs_and_units_are_identified_by_all_their_parts) {
    // Same name and dimension, another parent: another spec.
    static_assert(!(length_of_width == isq::length));
    static_assert(length_of_width == length_of_width);
    // Same symbols, dimension and magnitude, another kind: another unit.
    static_assert(!(per_second_f == per_second_a));
    static_assert(per_second_f == per_second_f);
}

TEST(UnitsApiTest, mismatches_are_constraint_failures) {
    static_assert(can_add<metres, metres> && !can_add<metres, seconds> && !can_less<metres, seconds>);
    static_assert(!can_add<decltype(1_q * si::hertz), decltype(1_q * si::becquerel)>);
    static_assert(!can_scale_raw<metres>);
    static_assert(!can_in_seconds<metres> && can_in_radians<id<"[0..7] [rad]">>);
    static_assert(!can_in_radians<degrees>); // a factor of pi: value_cast rounds it
    static_assert(!can_cast_to_duration<metres>);
    static_assert(can_atan2<metres, metres> && !can_atan2<metres, seconds>);
}

TEST(UnitsApiTest, quantities_compare_only_with_compatible_quantities) {
    using plain      = id<"[0..10]">;
    using kilometres = id<"[0..10] [km]">;
    using percents   = id<"[0..100] [%]">;
    using ones       = id<"[0..10] [1]">;
    using celsius_q  = id<"[0..10] [deg_C]">;
    using celsius_pt = decltype(point<si::degree_Celsius>(plain{}));
    using kelvin_pt  = decltype(point<si::kelvin>(id<"[0..400]">{}));
    // Only a plain inside compares with a standard integer (a double is rejected by design).
    static_assert(can_compare<plain, int> && cannot_compare<plain, double>);
    // A quantity compares with a quantity of its kind, in any unit.
    static_assert(can_compare<metres, kilometres>);
    static_assert(can_compare<percents, decltype(0_q * percent)> && can_compare<percents, ones>);
    // Never with a number, not even a dimensionless one: % and unit one take no int, double or plain inside.
    static_assert(cannot_compare<metres, int> && cannot_compare<metres, plain>);
    static_assert(cannot_compare<percents, int> && cannot_compare<percents, double> && cannot_compare<percents, plain>);
    static_assert(cannot_compare<ones, int> && cannot_compare<ones, plain>);
    static_assert(cannot_compare<celsius_q, int> && cannot_compare<metres, seconds>);
    // A point compares with a point of the same absolute origin, never with a quantity or a number.
    static_assert(can_compare<celsius_pt, kelvin_pt>);
    static_assert(cannot_compare<celsius_pt, celsius_q> && cannot_compare<celsius_pt, int>
                  && cannot_compare<celsius_pt, plain>);
    // A unit compares as one of itself, under the same rule; −u is −1 u.
    static_assert(can_compare<metres, decltype(si::metre)> && can_compare<kilometres, decltype(si::metre)>);
    static_assert(can_compare<percents, decltype(percent)> && can_compare<percents, decltype(one)>);
    static_assert(cannot_compare<plain, decltype(one)> && cannot_compare<metres, decltype(si::second)>
                  && cannot_compare<celsius_pt, decltype(si::degree_Celsius)>);
    EXPECT_TRUE(metres(1, si::metre) == si::metre && si::metre == metres(1, si::metre));
    EXPECT_TRUE(-si::metre < metres(0, si::metre) && kilometres(1, km) > si::metre);
    static_assert(-si::metre == -1_q * si::metre);
}

TEST(UnitsApiTest, a_unit_is_one_of_itself_in_arithmetic) {
    using clamped_seconds = id<"[0..100000] [s] clamp">;
    using plain           = id<"[0..10]">;
    const auto len        = 3_q * si::metre;
    EXPECT_TRUE(len + si::metre == 4_q * si::metre && si::metre + len == 4_q * si::metre);
    EXPECT_TRUE(len - si::metre == 2_q * si::metre && si::metre - len == -2_q * si::metre);
    EXPECT_TRUE(si::metre + si::metre == 2_q * si::metre && km - si::metre == 999_q * si::metre);
    EXPECT_TRUE(si::metre * (2_q * si::second) == 2_q * si::metre * si::second);
    clamped_seconds t{100, si::second};
    t += si::minute;
    t -= si::second;
    EXPECT_TRUE(t == 159_q * si::second);
    const auto [h, rest] = divmod(5430_q * si::second, si::hour);
    EXPECT_TRUE(h == 1 && rest == 1830_q * si::second);
    EXPECT_TRUE(5430_q * si::second % si::hour == 1830_q * si::second);
    // A point moves by a unit.
    EXPECT_TRUE(point<si::kelvin>(5_q) + si::kelvin == point<si::kelvin>(6_q));
    // The rule of two quantities holds: no other dimension, no plain number; a quantity is still made as
    // number * unit.
    static_assert(!can_add<decltype(len), decltype(si::second)> && !can_subtract<decltype(si::second), decltype(len)>);
    static_assert(!can_add<plain, decltype(si::metre)> && !can_add<decltype(si::metre), decltype(si::second)>);
    static_assert(!can_scale_raw<decltype(si::metre)> && !can_times<decltype(si::metre), decltype(2_q)>);
}

// A tag of one's own is at namespace scope, where a type's text finds it.
inline constexpr auto zero_origin = origin<"zero", base_dimension<"thermodynamic_temperature">>;

TEST(UnitsApiTest, tags_for_your_own_entries) {
    const auto p = zero_origin + 5_q * si::kelvin;
    EXPECT_EQ(p.quantity_from_origin().numerical_value_in(si::kelvin), 5);
    static_assert(!can_subtract<decltype(p), decltype(point<si::kelvin>(1_q))>); // two absolute origins
}

TEST(UnitsApiTest, kind_is_a_value) {
    const id<"[100..1000] [km]{altitude}"> altitude{185, isq::altitude[km]};
    const id<"[6300..6400] [km]{radius}">  earth{6371, isq::radius[km]};
    const auto                                             r = altitude + earth;
    static_assert(r.kind == isq::length && !(r.kind == isq::height));
    static_assert(altitude.kind == isq::altitude);
    static_assert(decltype(r / (2_q * si::second))::kind == isq::length / isq::duration);
    static_assert(isq::length / isq::duration == isq::length / isq::time);
}

TEST(UnitsApiTest, value_cast_to_a_quantity_type) {
    using km_q = id<"[0..20] 1/1000 [km]">;
    const id<"[0..20000] [m]"> d{12345, si::metre};
    const auto                                    a = value_cast<km_q>(d);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(a)>, km_q>);
    EXPECT_EQ(to_string(a), "12.345 km");
    EXPECT_EQ((value_cast<id<"[0..20] [km] round_floor">>(d).numerical_value_in(km)), 12);
    EXPECT_EQ((value_cast<id<"[0..5] 1/1000 [km]">>(d).error()), error::overflow);
    // The spec must convert as for an implicit conversion: up the tree, not down.
    using height_q = id<"[0..9] [m]{height}">;
    using length_q = id<"[0..9] [m]{length}">;
    static_assert(can_value_cast<length_q, height_q> && !can_value_cast<height_q, length_q>);
    static_assert(!can_value_cast<km_q, seconds>);
}

TEST(UnitsApiTest, units_scale_by_magnitudes_on_either_side) {
    static_assert(si::metre * ins::detail::mag<2> == ins::detail::mag<2> * si::metre);
    static_assert(si::metre / pi == ins::detail::mag<1> / pi * si::metre);
    EXPECT_EQ(unit_symbol(si::metre * ins::detail::mag<2>), "[2·m]");
}

TEST(UnitsApiTest, sums_take_one_type_in_either_order) {
    const auto j  = 1_q * si::joule;
    const auto nm = 1_q * (si::newton * si::metre);
    static_assert(std::is_same_v<decltype(j + nm), decltype(nm + j)>);
    EXPECT_EQ(to_string(nm + j), "2 J");
}

TEST(UnitsApiTest, a_denominator_alone_prints_negative_exponents) {
    EXPECT_EQ(to_string(2_q / si::second), "2 s⁻¹");
    EXPECT_EQ(to_string(1_q / (si::second * si::metre)), "1 m⁻¹·s⁻¹");
    EXPECT_EQ(unit_symbol(inverse(si::second), text_encoding::ascii), "s^-1");
    EXPECT_EQ(unit_symbol(pi / si::metre), "[π·m⁻¹]");
    EXPECT_EQ(unit_symbol(si::metre / si::second), "m/s");
}

TEST(UnitsApiTest, points_from_zero_and_their_constraints) {
    const auto c = point<si::degree_Celsius>(id<"[-50..50] 1/10">{20});
    EXPECT_EQ(to_string(c.quantity_from_zero()), "293.15 ℃");
    EXPECT_EQ(to_string(point<si::metre>(5_q).quantity_from_zero()), "5 m");
    static_assert(!can_in_seconds<decltype(c)> && !can_point_in_seconds<decltype(c)>);
}

// (A quantity's type spells its unit by symbol, found among the tags at namespace scope.)
namespace {
inline constexpr auto rankine = unit<"°R", "5/9"_q * si::kelvin, {.ascii = "deg_R", .origin = si::absolute_zero}>;
} // namespace

TEST(UnitsApiTest, unit_options) {
    EXPECT_EQ(unit_symbol(rankine, text_encoding::ascii), "deg_R");
    EXPECT_EQ(to_string(point<rankine>(9_q).point_in(si::kelvin)), "5 K"); // counted from absolute zero
    EXPECT_EQ(to_string(1_q * turn_unit), "1tr");
    static_assert(kind_of(rpm_unit.value)->Name == "frequency");
    static_assert(!addable_units<decltype(1_q * rpm_unit), decltype(1_q * si::becquerel)>());
}

namespace {
inline constexpr auto euro = unit<"EUR", money_kind>;
inline constexpr auto cent = unit<"ct", 0.01_q * euro>;
} // namespace

TEST(UnitsApiTest, base_dimensions_of_your_own) {
    const auto     sum  = 2_q * euro + 5_q * cent;
    EXPECT_EQ(to_string(sum), "205 ct");
    static_assert(!can_add<decltype(1_q * euro), decltype(1_q * si::metre)>);
    static_assert(!can_add<decltype(1_q * euro), decltype(1_q * one)>); // not a plain number
    static_assert(!(dimension_of((euro / si::kilogram).value) == dimension_of(euro.value)));
    static_assert(dimension_of(square(euro).value).root(2) == dimension_of(euro.value));
    static_assert(dimension_of((euro / cent).value) == dim_one);
    EXPECT_EQ(to_string((3_q * euro) / (2_q * si::kilogram)), "1.5 EUR/kg");
}

namespace {
inline constexpr auto loot = unit<"loot", gold_u * health_u * mana_u * stamina::st>;
inline constexpr auto net  = unit<"net", xp_u * pixel_u * packet_u * token_u>;
} // namespace

TEST(UnitsApiTest, any_number_of_base_dimensions_of_your_own) {
    constexpr dimension eight = dimension_of((loot * net).value);
    static_assert(eight.Size == 8);
    // By content, in any order.
    static_assert(eight == dimension_of((net * loot).value));
    static_assert(eight ==
                  token_dim * packet_dim * pixel_dim * xp_dim * stamina::dim * mana_dim * health_dim * gold_dim);
    static_assert(eight.pow(3).root(3) == eight);
    static_assert(eight / eight == dim_one);
    static_assert(dimension_of((loot * net / si::second).value) == eight / base_dimension<"time">);

    const auto both  = (2_q * loot) * (3_q * net);
    const auto seven = both / gold_u;
    EXPECT_EQ(to_string(both + both), "12 loot·net");
    EXPECT_EQ(to_string(both / (2_q * si::second)), "3 loot·net/s");
    static_assert(dimension_of(std::remove_cvref_t<decltype(seven)>::reference.unit()).Size == 7); // gold⁰ drops out
    static_assert(!can_add<decltype(both), decltype(seven)>);
}

// The SI base dimensions are declared as one of your own is, and a base
// dimension is its name: base_unit<"m", "length"> equals si::metre. A
// quantity's type spells its unit by its symbol, so the re-declared metre and
// the catalog's are one type.
TEST(UnitsApiTest, si_base_dimensions_are_base_dimensions) {
    static_assert(dimension_of(si::metre.value) == base_dimension<"length">);
    static_assert(base_unit<"m", "length"> == si::metre);
    using I = id<"[0..100]">;
    static_assert(std::is_same_v<id<quantity_key<base_unit<"m", "length">, I>>, id<"[0..100] [m]">>);
    constexpr id<"[0..100] [m]"> m = id<quantity_key<base_unit<"m", "length">, I>>{3, base_unit<"m", "length">};
    static_assert(m == id<"[0..100] [m]">{3, si::metre});
    static_assert(dimension_of(si::newton.value).Size == 3);
    static_assert(dimension_of(si::kelvin.value) == isq::thermodynamic_temperature.value->Dim);
    constexpr dimension price = dimension_of((unit<"EUR", money_kind> / si::kilogram).value);
    static_assert(price == currency_dim / base_dimension<"mass">);
    static_assert(price == base_dimension<"mass">.pow(-1) * currency_dim); // in any order
    static_assert(price.Size == 2 && price.Base[0]->Name == "currency" && price.Base[1]->Name == "mass");
    static_assert(price * base_dimension<"mass"> == currency_dim);
    static_assert(dimension_of((si::metre / si::metre).value) == dim_one);
}

// Named definitions: each definition type takes its one-liner's arguments, and
// defined_by<d> is what the one-liner gives, spelled by d's name. It compares
// equal to the one-liner's and converts both ways, but is another type.
namespace {
inline constexpr auto             line_fathom = unit<"ftm", 1.8288_q * si::metre>;
inline constexpr unit_def         named_fathom_def{"ftm", 1.8288_q * si::metre};
inline constexpr auto             named_fathom = defined_by<named_fathom_def>;
inline constexpr auto             kilo_named_fathom = si::kilo(named_fathom); // (its text, kftm, names it)
inline constexpr kind_def         named_count_def{"count", base_dimension<"count">};
inline constexpr unit_def         named_cts_def{"cnt", defined_by<named_count_def>};
inline constexpr auto             named_cts = defined_by<named_cts_def>;
inline constexpr unit_def         named_watt_def{"W", si::joule / si::second};
inline constexpr kind_def         named_velocity_def{"velocity", isq::speed,
                                                     {.recipe = isq::displacement / isq::duration}};
inline constexpr kind_def         named_angle_def{"angular_measure", isq::dimensionless,
                                                  {.distinct = true, .recipe = isq::arc_length / isq::radius}};
inline constexpr origin_def       named_ice_def{"ice", si::absolute_zero + 273.15_q * si::kelvin};
inline constexpr auto             named_ice = defined_by<named_ice_def>;
inline constexpr origin_def       named_zero_def{"zero", base_dimension<"thermodynamic_temperature">};
inline constexpr basis_vector_def named_e1_def{"e1", geometric<+1>};
} // namespace

TEST(UnitsApiTest, named_definitions) {
    using I = id<"[0..100]">;
    static_assert(named_fathom == line_fathom);
    static_assert(named_cts == base_unit<"cnt", "count">);
    static_assert(defined_by<named_watt_def> == si::watt);
    static_assert(defined_by<named_velocity_def> == isq::velocity);
    static_assert(defined_by<named_angle_def> == isq::angular_measure);
    static_assert(defined_by<named_e1_def> == basis_vector<"e1", geometric<+1>>);
    static_assert(ins::detail::same(origin_ref{defined_by<named_ice_def>},
                                    origin_ref{origin<"ice", si::absolute_zero + 273.15_q * si::kelvin>}));
    static_assert(ins::detail::same(origin_ref{defined_by<named_zero_def>},
                                    origin_ref{origin<"zero", base_dimension<"thermodynamic_temperature">>}));

    // The tag of the definition's type, holding the definition's address.
    static_assert(std::is_same_v<decltype(named_fathom), const unit_t<unit_value{named_fathom_def}>>);
    static_assert(std::is_same_v<decltype(defined_by<named_count_def>), const kind_t<&named_count_def>>);
    static_assert(std::is_same_v<decltype(defined_by<named_ice_def>), const origin_t<origin_ref{&named_ice_def}>>);

    // The one-liner's type: a quantity's type spells its unit by its symbol, and the two are equal.
    static_assert(std::is_same_v<id<quantity_key<named_fathom, I>>, id<quantity_key<line_fathom, I>>>);
    constexpr id<"[0..100] [ftm]">  a = id<"[0..100] [ftm]">{12, named_fathom};
    constexpr id<"[0..100] [ftm]"> b = a;
    static_assert(a == b);

    // Prefixes, conversions, products, points and text, as for any unit.
    EXPECT_EQ(to_string(id<"[0..100] [kftm]">{2, si::kilo(named_fathom)}), "2 kftm");
    EXPECT_EQ(to_string(b.in(si::metre)), "21.9456 m");
    EXPECT_EQ(to_string(id<"[0..100] [cnt]">{7, named_cts} / si::second), "7 cnt/s");
    EXPECT_EQ(to_string(point<si::degree_Celsius>(id<"[-5..5]">{1}).point_for(named_ice).quantity_from_origin()),
              "1 ℃");
    constexpr auto e1 = defined_by<named_e1_def>;
    static_assert(dimension_of((e1 * e1).value) == dim_one);
}

// A named unit (with a prefix too) and its own kind is held as that unit alone, any other reference as its parts.
// Each reference has one form, however it is reached, so equal references are one type.
namespace {
inline constexpr auto metre_second = unit<"m_s", si::metre * si::second>; // (no separator in a symbol: m·s is two)
} // namespace

TEST(UnitsApiTest, a_reference_has_one_form) {
    using num = id<"[0..100]">;
    using m_t = id<"[0..100] [m]">;
    static_assert(!m_t::reference.HasParts && !id<"[0..100] [kg]">::reference.HasParts);
    static_assert(!id<"[0..100] [km]">::reference.HasParts);
    static_assert(m_t::reference.kind()->Name == "length" && m_t::reference.kind_of());
    static_assert(m_t::reference.unit() == unit_value{si::metre_def} && m_t::reference.derived().Size == 0);
    // Reached by arithmetic: the same type.
    static_assert(std::is_same_v<decltype(num{} * si::metre), m_t>);
    static_assert(std::is_same_v<decltype(num{} * si::metre * si::second / si::second), m_t>);
    static_assert(m_t::reference == reference{&isq::length_def, true, recipe{}, unit_value{si::metre_def}});
    // Exactly a kind, a product of units, and a scaled unit: the parts.
    static_assert(id<"[0..100] [m]{length}">::reference.HasParts);
    static_assert(!std::is_same_v<id<"[0..100] [m]{length}">, m_t>);
    static_assert(id<"[0..100] [m/s]">::reference.HasParts);
    static_assert(id<"[0..100] [m^2]">::reference.HasParts);
    // A named unit without a kind: the unit alone too, and kind() is kind_def::none.
    static_assert(!id<"[0..100] [m_s]">::reference.HasParts);
    static_assert(id<"[0..100] [m_s]">::reference.kind()->Name.empty());
}

namespace {
inline constexpr auto cts = unit<"cts", adc_counts_kind>;
} // namespace

TEST(UnitsApiTest, adc_counts) {
    using reading      = id<"[0..4095] [cts]">;
    using gain_t       = id<"[1000..1500] 1/1000 [cts/V] round_half_away">;
    using volts_t      = id<"[0..5] 1/1000 [V]">;
    const reading                          raw{2048, cts};
    const id<"[-64..64] [cts]"> offset{12, cts};
    const gain_t                           gain{rational{1240909, 1000}, cts / si::volt};
    const auto                             v = (raw - offset) / gain; // zero-free gain: a plain quantity
    EXPECT_EQ(to_string(value_cast<id<"[-5000..5000] [mV] round_half_away">>(v)),
              "1641 mV");
    // Two-point calibration: the exact Δcts / ΔV, checked into gain_t once.
    const id<"[0..1] 1/1000 [V]"> v1{rational{1, 2}, si::volt};
    const id<"[2..5] 1/1000 [V]"> v2{rational{5, 2}, si::volt};
    const auto g = value_cast<gain_t>((reading{3114, cts} - reading{632, cts}) / (v2 - v1));
    ASSERT_TRUE(g.has_value());
    EXPECT_EQ(to_string(*g), "1241.000 cts/V");
    EXPECT_FALSE(value_cast<gain_t>((reading{632, cts} - reading{632, cts}) / (v2 - v1)).has_value());
    static_assert(!can_add<reading, volts_t>);
    static_assert(!can_add<reading, decltype(1_q * one)>); // counts are not a plain number
    static_assert(!(dimension_of((cts / si::volt).value) == dimension_of((one / si::volt).value)));
    static_assert(!std::constructible_from<id<"[0..5] [A]">, decltype(v)>);
}
} // namespace units_api_tests

// inside's algorithms on quantities: policy hooks, ++/--, try_make,
// mul<C>/div<C>, add_all/mul_all/sum, parse, the conversion predicates,
// the clamp constructor/the wrap constructor, dot/cross/lerp, and sin/cos of an angle in
// a unit with a factor of π.
namespace units_algorithms_tests {
using namespace ins;

namespace {
inline constexpr auto health_dim  = base_dimension<"health">;
inline constexpr auto health_kind = kind<"health", health_dim>;
inline constexpr auto HP          = unit<"HP", health_kind>;
inline constexpr auto slot        = unit<"slot", "1/32768"_q * pi * si::radian>;
// π/2^70 rad: a factor past the 64-bit rational.
inline constexpr auto fine_turn = unit<"fturn", "1/1180591620717411303424"_q * pi * si::radian>; // (ft: foot)

constexpr auto ms = si::milli(si::second);

using seconds_t = id<"[0..60) [s] wrap">;
using hp_t      = id<"[0..100] [HP] clamp">;
using usd_t     = id<"[0..1000] 1/100 [HP] round_half_away">;

template <class Out, class A, class B>
concept can_mul_onto = requires(const A& a, const B& b) { mul<Out>(a, b); };
template <class Q, class X>
concept can_wrap_cast = requires(const X& x) { Q{x, wrap}; };
template <class A, class B>
concept can_add_into = requires(A& a, const B& b) { a.on_wrap([](auto&, auto) {}) += b; };
} // namespace

TEST(UnitsAlgorithmsTest, hooks_take_quantities_and_see_the_quantity) {
    seconds_t s{50, si::second};
    int       carry = -1;
    s.on_wrap([&](seconds_t& self, auto c) {
        carry = to<int>(c);
        EXPECT_EQ(&self, &s);
    }) += 25_q * si::second;
    EXPECT_EQ(to_string(s), "15 s");
    EXPECT_EQ(carry, 1);
    s.on_wrap([&](auto&, auto c) { carry = to<int>(c); }) += 2_q * si::minute; // in seconds, exactly
    EXPECT_EQ(to_string(s), "15 s");
    EXPECT_EQ(carry, 2);
    static_assert(!can_add_into<seconds_t, decltype(1_q * si::metre)>);

    hp_t hp{75, HP};
    bool downed = false;
    hp.on_clamp([&](hp_t& self, auto) { downed = self == 0_q * HP; }) -= 200_q * HP;
    EXPECT_TRUE(downed);
    EXPECT_EQ(to_string(hp), "0 HP");
}

TEST(UnitsAlgorithmsTest, increment_is_one_unit_and_try_make) {
    hp_t hp{98, HP};
    ++hp;
    EXPECT_EQ(to_string(hp++), "99 HP");
    ++hp; // clamps at 100
    --hp;
    EXPECT_EQ(to_string(hp), "99 HP");
    const auto ok  = hp_t::try_make(5, HP);
    const auto bad = id<"[0..100] [HP]">::try_make(500, HP);
    ASSERT_TRUE(ok);
    EXPECT_EQ(to_string(*ok), "5 HP");
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error(), error::overflow);
}

TEST(UnitsAlgorithmsTest, mul_and_div_onto_a_named_output_fold_the_unit_factor) {
    // 3 m/s for 7 s, stored in km to the metre: 0.021 km.
    using km_t   = id<"[0..10] 1/1000 [km] round_half_away">;
    const auto d = mul<km_t>(3_q * si::metre / si::second, id<"[0..10] [s]">{7, si::second});
    EXPECT_EQ(to_string(d), "0.021 km");
    // A plain factor, rounded once onto the cent.
    EXPECT_EQ(to_string(mul<usd_t>(10_q * HP, id<"[0..4] 1/16">{1.0625})), "10.63 HP");
    // A dimensionless quotient into a plain inside: 95.5 ms / 10 ms floors to 9.
    const id<"[0..100] 1/10 [ms]"> lat{rational{955, 10}, ms};
    const auto                                      bin = div<id<"[0..9] clamp|round_floor">>(lat, 10_q * ms);
    EXPECT_EQ(bin, 9);
    // A divisor grid holding 0 gives an expected.
    const auto q = div<usd_t>(10_q * HP, id<"[0..4]">{0});
    ASSERT_FALSE(q);
    EXPECT_EQ(q.error(), error::division_by_zero);
    static_assert(!can_mul_onto<usd_t, decltype(1_q * si::metre), decltype(1_q)>); // a length is no HP
}

TEST(UnitsAlgorithmsTest, folds_sum_and_parse) {
    const std::vector<usd_t> items{usd_t{1.5, HP}, usd_t{2.25, HP}};
    EXPECT_EQ(to_string(*sum<usd_t>(items)), "3.75 HP");
    const std::array<id<"[0..1000] [ms]">, 2> waits{id<"[0..1000] [ms]">{250, ms},
                                                               id<"[0..1000] [ms]">{1000, ms}};
    EXPECT_EQ(to_string(*sum<id<"[0..10] 1/1000 [s]">>(waits)), "1.250 s");
    EXPECT_EQ(to_string(add_all(1_q * HP, 2_q * HP, 3_q * HP)), "6 HP");
    EXPECT_EQ(to_string(mul_all(2_q * si::metre, 3_q * si::metre, 0.5_q)), "3 m²");

    EXPECT_EQ(to_string(*parse<usd_t>("12.34 HP")), "12.34 HP");
    EXPECT_EQ(parse<usd_t>("12.3x HP").error(), error::invalid_format);
    EXPECT_EQ(parse_exact<usd_t>("12.345 HP").error(), error::rounding_error);
}

TEST(UnitsAlgorithmsTest, predicates_and_casts) {
    EXPECT_TRUE(conversion_overflows<usd_t>(2000.0, HP));
    EXPECT_FALSE(conversion_overflows<usd_t>(20.0, HP));
    EXPECT_TRUE(conversion_rounds<usd_t>(0.125, HP));
    EXPECT_TRUE(conversion_loses<usd_t>(-1, HP));

    EXPECT_EQ(to_string(usd_t{5000, HP, clamp}), "1000.00 HP");
    EXPECT_EQ(to_string(seconds_t{130, si::second, wrap}), "10 s");
    EXPECT_EQ(to_string(seconds_t{3_q * si::minute + 5_q * si::second, wrap}), "5 s");
    static_assert(!can_wrap_cast<seconds_t, decltype(1_q * si::metre)>);
}

// q *= n with a builtin integer scales the number, stored back by its rules.
TEST(UnitsAlgorithmsTest, a_builtin_integer_scales_a_quantity) {
    id<"[0..100] [m] clamp"> q{5, si::metre};
    q *= 2;
    EXPECT_TRUE(q == 10_q * si::metre);
    q *= 100; // clamped
    EXPECT_TRUE(q == 100_q * si::metre);
    id<"[0..100] [m]"> c{5, si::metre};
    error                                 ec{};
    c.on_error(ec) *= 3;
    EXPECT_TRUE(c == 15_q * si::metre && ec == error{});
    c.on_error(ec) *= 100;
    EXPECT_TRUE(c == 15_q * si::metre && ec == error::overflow);
}

TEST(UnitsAlgorithmsTest, dot_cross_lerp) {
    const auto x = 3_q * si::metre;
    const auto y = 4_q * si::metre;
    EXPECT_EQ(to_string(x * x + y * y), "25 m²");
    EXPECT_EQ(to_string(x * -x - y * y), "-25 m²");
    EXPECT_EQ(to_string(lerp(x, 5_q * si::metre, 0.5_q)), "4 m");
}

TEST(UnitsAlgorithmsTest, trig_of_a_pi_scaled_angle_reduces_exactly) {
    using amp   = id<"[-1..1] 1/16384 round_half_away">;
    using floor = id<"[-1..1] 1/16384 round_floor">;
    using deg   = id<"[-1000..1000] [deg]">;
    // Niven's values come out exact under any rounding.
    EXPECT_EQ(math::sin<floor>(deg{30, si::degree}), (0.5_q));
    EXPECT_EQ(math::cos<floor>(deg{120, si::degree}), (-0.5_q));
    EXPECT_EQ(math::sin<floor>(deg{-90, si::degree}), -1_q);
    EXPECT_EQ(math::cos<floor>(deg{750, si::degree}), math::cos<floor>(deg{30, si::degree}));
    EXPECT_EQ(math::sin<amp>(deg{180, si::degree}), 0);
    // cos 45° = 0.70710678…: 11585/16384 nearest, and floor alike.
    EXPECT_EQ(math::cos<amp>(deg{45, si::degree}), ("11585/16384"_q));
    EXPECT_EQ(math::cos<floor>(deg{45, si::degree}), ("11585/16384"_q));
    // A quarter turn in slots of 2π/65536 rad.
    const id<"[0..65535] [slot] wrap"> p{16384, slot};
    EXPECT_EQ(math::sin<amp>(p), 1);
    EXPECT_EQ(math::cos<amp>(p), 0);
}

// A π unit whose factor is no 64-bit rational took no sin or cos (the
// factor was one limb).
TEST(UnitsAlgorithmsTest, trig_of_a_pi_angle_with_a_factor_past_64_bits) {
    using amp = id<"[-1..1] 1/16384 round_half_away">;
    using a70 = id<"[0..2361183241434822606848] [fturn]">;                  // up to 2π
    EXPECT_EQ(math::sin<amp>(a70{590295810358705651712_g, fine_turn}), 1);  // 2^69: π/2
    EXPECT_EQ(math::cos<amp>(a70{590295810358705651712_g, fine_turn}), 0);
    EXPECT_EQ(math::cos<amp>(a70{295147905179352825856_g, fine_turn}), ("11585/16384"_q)); // π/4
    EXPECT_EQ(math::sin<amp>(a70{1, fine_turn}), 0);
}

TEST(UnitsAlgorithmsTest, divmod_in_the_common_unit_and_percent) {
    const id<"[0..86399] [s]"> s{3725, si::second};
    const auto [h, rest] = divmod(s, si::hour);
    EXPECT_EQ(h, 1);
    EXPECT_EQ(to_string(rest), "125 s");
    const auto [q, r] =
        divmod(id<"[0..100] [d] round_toward_zero">{17, si::day}, 1_q * si::day * 7_q);
    EXPECT_EQ(q, 2);
    EXPECT_EQ(to_string(r), "3 d");

    EXPECT_EQ(to_string(8_q * percent), "8 %");
    const id<"[0..1] 1/100"> x = 8_q * percent;
    EXPECT_EQ(x, (0.08_q));
    EXPECT_EQ(to_string(mul<usd_t>(45_q * HP, 8_q * percent)), "3.60 HP");
    EXPECT_EQ(unit_symbol(per_mille, text_encoding::ascii), "permille");
}

TEST(UnitsAlgorithmsTest, math_of_a_dimensionless_quantity_and_lifted_chains) {
    constexpr auto ratio = -6_q * si::metre / (20_q * si::metre); // −0.3
    using gain_t         = id<"[0x1p-8..4] 1/65536 round_half_away">;
    using decade_t       = id<"[-1..1] 1/1024 round_half_away">;
    const auto g         = math::pow_base<10, gain_t>(ratio);
    EXPECT_EQ(g, (gain_t{rational{32846, 65536}}));
    const auto d = math::log10<decade_t>(1_q * si::metre / (1_q * si::metre));
    EXPECT_EQ(d, 0);
    // An expected quantity chains through +, *, as an expected inside does.
    const auto chained = 1_q * si::volt / id<"[0..4]">{0} * 2_q + si::volt;
    ASSERT_FALSE(chained);
    EXPECT_EQ(chained.error(), error::division_by_zero);
    const auto fine = 1_q * si::volt / id<"[0..4]">{2} * 2_q + si::volt;
    ASSERT_TRUE(fine);
    EXPECT_EQ(to_string(*fine), "2 V");
}
} // namespace units_algorithms_tests

// units/math.hpp: pi conversions and math on quantities.
namespace units_math_tests {
using namespace ins;

namespace {
using rad_t = id<"[-4..4] 1/1'000'000 round_half_away">;
using deg_t = id<"[-360..360] 1/1'000'000 round_half_away">;

template <class A, class B>
concept can_gcd = requires(const A& a, const B& b) { math::gcd(a, b); } || requires(const A& a, const B& b) {
    math::lcm(a, b);
};
} // namespace

TEST(UnitsMathTest, degree_to_radian_rounds_once) {
    const id<"[0..360] [deg]"> a{90, si::degree};
    const auto                                   r = value_cast<si::radian, rad_t>(a);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(r)>::rep, rad_t>);
    EXPECT_EQ(rational{r.numerical_value_in(si::radian)}, rational(1570796, 1'000'000)); // π/2 = 1.5707963…
    const auto n = value_cast<si::radian, rad_t>(id<"[-360..0] [deg]">{-90, si::degree});
    EXPECT_EQ(rational{n.numerical_value_in(si::radian)}, rational(-1570796, 1'000'000));
    // And back: 1 rad = 57.2957795…°.
    const auto d = value_cast<si::degree, deg_t>(id<"[0..2] [rad]">{1, si::radian});
    EXPECT_EQ(rational{d.numerical_value_in(si::degree)}, rational(57295780, 1'000'000));
    // 0 is exact.
    EXPECT_EQ((value_cast<si::radian, rad_t>(id<"[0..360] [deg]">{0, si::degree})
                   .numerical_value_in(si::radian)),
              0);
    // A rational factor stays exact: arcminute → degree.
    const auto m = value_cast<si::degree, id<"[0..1] 1/60">>(30_q * si::arcminute);
    EXPECT_EQ(rational{m.numerical_value_in(si::degree)}, rational(1, 2));
}

TEST(UnitsMathTest, roots_take_the_units_root) {
    using area_t = id<"[0..100] 1/1000 [m^2]{area} round_half_away">;
    const auto s = math::sqrt(area_t{2, isq::area[square(si::metre)]});
    static_assert(std::remove_cvref_t<decltype(s)>::unit == si::metre.value);
    static_assert(std::remove_cvref_t<decltype(s)>::reference.kind()->Name == "length");
    EXPECT_EQ(rational{s.numerical_value_in(si::metre)}, rational(1414, 1000));
    const auto c =
        math::cbrt(id<"[0..1000] 1/100 [m^3] round_half_away">{27, cubic(si::metre)});
    EXPECT_EQ(c.numerical_value_in(si::metre), 3);
    EXPECT_EQ(to_string(c), "3.00 m"); // a decimal notch fixes the digits
}

TEST(UnitsMathTest, trigonometry_in_radians) {
    const id<"[-4..4] 1/1'000'000 [rad] round_half_away"> a{rational(1, 2), si::radian};
    EXPECT_EQ(rational{math::sin(a)}, rational(479426, 1'000'000)); // sin 0.5 = 0.4794255…
    // Of a dimensionless quantity: an angle in radians (of a plain inside, a number).
    const auto t = math::atan(id<"[-1..1] 1/1000 [1] round_half_away">{1, one});
    EXPECT_EQ(to_string(t), "0.785 rad");
    EXPECT_EQ(to_string(math::atan(id<"[-1..1] 1/1000 round_half_away">{1})), "0.785");
    const auto y = id<"[0..10] 1/1000 [m] round_half_away">{3, si::metre};
    const auto x = id<"[0..10] 1/1000 [km] round_half_away">{rational(4, 1000),
                                                                                                si::kilo(si::metre)};
    EXPECT_EQ(rational{math::atan2(y, x).numerical_value_in(si::radian)}, rational(644, 1000)); // atan(3/4) = 0.6435…
    EXPECT_EQ(to_string(math::hypot(y, x)), "5.000 m");
}

TEST(UnitsMathTest, gcd_and_lcm_in_the_common_unit) {
    using minutes = id<"[1..60] [min]">;
    const minutes a{12, si::minute}, b{18, si::minute};
    EXPECT_TRUE(math::gcd(a, b) == 6_q * si::minute);
    EXPECT_TRUE(math::lcm(a, b) == 36_q * si::minute);
    EXPECT_EQ(to_string(math::lcm(a, b)), "36 min");
    // Mixed units: the common unit (3 cm and 4 mm are 30 mm and 4 mm).
    const id<"[0..10] [cm]"> c{3, si::centi(si::metre)};
    const id<"[0..100] [mm]"> m{4, si::milli(si::metre)};
    EXPECT_EQ(to_string(math::gcd(c, m)), "2 mm");
    EXPECT_TRUE(math::lcm(c, m) == 6_q * si::centi(si::metre));
    static_assert(!can_gcd<minutes, decltype(1_q * si::metre)> && can_gcd<minutes, decltype(1_q * si::hour)>);
}

TEST(UnitsMathTest, number_functions_keep_the_unit) {
    const id<"[-10..10] 1/4 [m] round_half_away"> d{rational(-11, 4), si::metre};
    EXPECT_EQ(to_string(math::abs(d)), "2.75 m");
    EXPECT_EQ(math::floor(d).numerical_value_in(si::metre), -3);
    EXPECT_EQ(math::ceil(d).numerical_value_in(si::metre), -2);
    EXPECT_EQ(math::round(d).numerical_value_in(si::metre), -3);
    EXPECT_EQ(math::trunc(d).numerical_value_in(si::metre), -2);
    EXPECT_EQ(to_string(ins::pow<2>(d)), "7.5625 m²");
}

TEST(UnitsMathTest, pi_magnitudes_round_once_in_value_cast) {
    // π · 2 s → 6.283185 s: π is part of the unit until the one rounding.
    const id<"[0..10] [s]"> t{2, si::second};
    const auto s = value_cast<si::second, id<"[0..100] 1/1'000'000 round_half_away">>(pi * t);
    EXPECT_EQ(rational{s.numerical_value_in(si::second)}, rational(6283185, 1'000'000));
    const auto h = value_cast<si::hour, id<"[0..1] 1/1'000'000'000 round_half_away">>(pi * t);
    EXPECT_EQ(rational{h.numerical_value_in(si::hour)}, rational(1745329, 1'000'000'000)); // 2π/3600 = 0.00174532925…
}

TEST(UnitsMathTest, named_outputs_take_checked_inputs) {
    using out_t = id<"[0..100] 1/1000 round_half_away">;
    // An arithmetic result is checked: the deduced sqrt refuses it, sqrt<Out> rounds onto Out.
    const id<"[0..50] [m^2]"> a{2, square(si::metre)};
    const auto                                         s = math::sqrt<out_t>(a + a);
    static_assert(std::remove_cvref_t<decltype(s)>::unit == si::metre.value);
    EXPECT_EQ(to_string(s), "2.000 m");
    EXPECT_EQ(math::cbrt<out_t>(id<"[0..100] [m^3]">{27, cubic(si::metre)})
                  .numerical_value_in(si::metre),
              3);
    // (A grid that reaches below 0 is a compile error: fail/sqrt_domain.)

    using unit_t = id<"[-1..1] 1/1'000'000 round_half_away">;
    const id<"[0..3] [rad]"> one_rad{1, si::radian};
    EXPECT_EQ(rational{math::sin<unit_t>(one_rad)}, rational(841471, 1'000'000));
    EXPECT_EQ(rational{math::cos<unit_t>(one_rad)}, rational(540302, 1'000'000));
    using rad_out = id<"[-4..4] 1/1000 round_half_away">;
    EXPECT_EQ(to_string(math::atan<rad_out>(id<"[0..2] [1]">{1, one})), "0.785 rad");
    // A dimensionless quantity is a number too: asin of 1/2 = π/6.
    const auto half = id<"[0..5] [m]">{3, si::metre} /
                      id<"[5..10] [m]">{6, si::metre}; // in [0, 1]
    EXPECT_EQ(to_string(math::asin<rad_out>(half)), "0.524 rad");
    const auto y = id<"[0..10] [m]">{3, si::metre};
    const auto x = id<"[0..10] [m]">{4, si::metre};
    EXPECT_EQ(to_string(math::hypot<out_t>(x, y)), "5.000 m");
    EXPECT_EQ(to_string(math::atan2<rad_out>(y, x)), "0.644 rad");
}

// The rounding mode on the deduced call works on quantities too.
TEST(UnitsMathTest, mode_on_the_deduced_call) {
    using area_t = id<"[0..100] 1/1024 [m^2]">; // no rounding policy
    static_assert(math::sqrt<round_floor>(area_t{2, square(si::metre)}).numerical_value_in(si::metre) ==
                  1.4140625_q);
    static_assert(math::sin<round_half_away>(id<"[0..4] 1/1024 [rad]">{1, si::radian}) > 0);
    static_assert(
        math::atan<round_half_away>(id<"[0..1] 1/64 [1]">{1, one}).numerical_value_in(si::radian) >
        0);
}
// A named output on a quantity: its number alone (the unit follows the
// function), or the whole quantity in the result's unit; specs name the
// number's notch and policy.
TEST(UnitsMathTest, named_number_whole_quantity_and_specs) {
    using area_t = id<"[0..100] 1/1024 [m^2] round_half_away">;
    using L      = id<"[0..10] 1/4096 round_floor">;
    const area_t a{2, square(si::metre)};
    const auto   n = math::sqrt<L>(a);
    static_assert(decltype(n)::unit == si::metre.value && std::same_as<decltype(n)::rep, L>);
    EXPECT_EQ(n, (math::sqrt<id<"[0..10] 1/4096 [m] round_floor">>(a)));
    EXPECT_EQ(to_string(n), "1.4140625 m"); // 5791/4096, rounded down
    const auto s = math::sqrt<per<1000>, round_ceil>(a);
    static_assert(decltype(s)::unit == si::metre.value && notch_of<decltype(s)::rep> == rational{1, 1000});
    EXPECT_EQ(to_string(s), "1.415 m");
    // atan2 and hypot: the number in the common unit, the unit the rule's.
    const auto y = id<"[0..10] 1/1000 [m] round_half_away">{3, si::metre};
    const auto x = id<"[0..10] 1/1000 [km] round_half_away">{rational(4, 1000),
                                                                                                si::kilo(si::metre)};
    EXPECT_EQ(to_string(math::atan2<id<"[-4..4] 1/100 round_half_away">>(y, x)), "0.64 rad");
    EXPECT_EQ(to_string(math::hypot<per<10>>(y, x)), "5.0 m");
    // A number function of a dimensionless quantity: a number, named or by spec.
    using ratio_t = id<"[1..100] 1/64 [1] round_half_away">;
    EXPECT_EQ((math::log10<id<"[0..2] 1/8 round_half_away">>(ratio_t{10, one})), 1);
    static_assert(insidable<decltype(math::log10<per<8>>(ratio_t{10, one}))>);
    // sin of degrees on a notch you name: exact at 30°.
    EXPECT_EQ((math::sin<per<2>>(id<"[0..90] [deg]">{30, si::degree})), 0.5_q);
}

} // namespace units_math_tests

// math/constants.hpp and units/constants.hpp: mathematical constants (math::) and physical constants as
// quantities (ins::constants::), exact where the SI defines them.
namespace constants_tests {
using namespace ins;

namespace pc = ins::constants;

TEST(ConstantsTest, physical_constants_have_their_kind_and_value) {
    static_assert(pc::speed_of_light_in_vacuum.kind == isq::speed);
    static_assert(pc::standard_gravity.kind == isq::acceleration_of_free_fall);
    static_assert(pc::standard_atmosphere.kind == isq::pressure);
    static_assert(pc::characteristic_impedance_of_vacuum.kind == isq::impedance);
    EXPECT_EQ(to_string(pc::speed_of_light_in_vacuum.in(si::metre / si::second)), "299792458 m/s");
    EXPECT_EQ(to_string(pc::standard_atmosphere.in(si::pascal)), "101325 Pa");
    EXPECT_EQ(to_string(pc::standard_gravity.in(si::metre / square(si::second))), "9.80665 m/s²");
    EXPECT_EQ(to_string(pc::hyperfine_structure_transition_frequency_of_cs.in(si::hertz)), "9192631770 Hz");
    EXPECT_EQ(to_string(pc::fine_structure_constant.in(one)), "0.0072973525643");
    EXPECT_EQ(to_string(pc::rydberg_constant.in(one / si::metre)), "10973731.568157 m⁻¹");
    EXPECT_EQ(to_string(pc::characteristic_impedance_of_vacuum.in(si::ohm)), "376.730313412 Ω");
    EXPECT_EQ(
        std::format("{:.5e}", pc::gravitational_constant.in(cubic(si::metre) / (si::kilogram * square(si::second)))),
        "6.67430e-11 m³/(kg·s²)");
    EXPECT_EQ(std::format("{:.11e}", pc::vacuum_magnetic_permeability.in(si::newton / square(si::ampere))),
              "1.25663706127e-06 N/A²");
}

TEST(ConstantsTest, si2019_defining_constants_are_exact) {
    static_assert(pc::planck_constant.kind == isq::action);
    static_assert(pc::elementary_charge.kind == isq::electric_charge);
    EXPECT_EQ(std::format("{:.8e}", pc::planck_constant.in(si::joule * si::second)), "6.62607015e-34 J·s");
    EXPECT_EQ(std::format("{:.9e}", pc::elementary_charge.in(si::coulomb)), "1.602176634e-19 C");
    EXPECT_EQ(std::format("{:.6e}", pc::boltzmann_constant.in(si::joule / si::kelvin)), "1.380649e-23 J/K");
    EXPECT_EQ(std::format("{:.8e}", pc::avogadro_constant.in(one / si::mole)), "6.02214076e+23 mol⁻¹");
    EXPECT_EQ(std::format("{:.10e}", pc::electron_mass.in(si::kilogram)), "9.1093837139e-31 kg");
    EXPECT_EQ(std::format("{:.11e}", pc::atomic_mass_constant.in(si::kilogram)), "1.66053906892e-27 kg");
    EXPECT_EQ(std::format("{:.11e}", (1_q * si::dalton).in(si::kilogram)), "1.66053906892e-27 kg");
}

TEST(ConstantsTest, derived_constants_are_exact_products) {
    // R = N_A·k and F = N_A·e, in full.
    EXPECT_EQ(to_string(pc::molar_gas_constant.in(si::joule / (si::mole * si::kelvin))), "8.31446261815324 J/(K·mol)");
    EXPECT_EQ(to_string(pc::faraday_constant.in(si::coulomb / si::mole)), "96485.3321233100184 C/mol");
    EXPECT_TRUE(pc::molar_gas_constant == pc::avogadro_constant * pc::boltzmann_constant);
    EXPECT_TRUE(pc::faraday_constant == pc::avogadro_constant * pc::elementary_charge);
    // K_J·Φ₀ = 1 and R_K·G₀ = 2: dimension one, exactly.
    EXPECT_EQ(to_string((pc::josephson_constant * pc::magnetic_flux_quantum).in(one)), "1");
    EXPECT_EQ(to_string((pc::von_klitzing_constant * pc::conductance_quantum).in(one)), "2");
    // ħ = h/2π: a factor of π rounds once, through value_cast.
    using fine = id<"[0..1] 1e-45 round_half_away">;
    EXPECT_EQ(std::format("{:.9e}", value_cast<si::joule * si::second, fine>(pc::reduced_planck_constant)),
              "1.054571818e-34 J·s");
    // E = h·ν for 540 THz, exactly.
    const id<"[0..1000] [THz]"> nu{540, si::tera(si::hertz)};
    EXPECT_EQ(std::format("{:.9e}", (pc::planck_constant * nu).in(si::joule)), "3.578077881e-19 J");
    // c₂ = hc/k.
    EXPECT_EQ(std::format("{:.9e}", pc::second_radiation_constant.in(si::metre * si::kelvin)), "1.438776878e-02 K·m");
}
} // namespace constants_tests

// Arms no other test reaches, found by the reachability audit (gcov and an
// `if constexpr` probe over the suite).
namespace reached_arms_tests {
using namespace ins;

// A conversion keeps the number's policy; pow<3>, q-- and a prefix without
// a symbol.
TEST(UnitsArmTest, conversion_policy_pow_decrement_and_unnamed_prefix) {
    const id<"[0..100] [m] clamp"> q{5, si::metre};
    const auto                                         cm = q.in(si::centi(si::metre));
    static_assert(has_flag(policy_of<std::remove_cvref_t<decltype(cm)>::rep>, clamp));
    EXPECT_EQ(to_string(cm), "500 cm");
    EXPECT_EQ(to_string(pow<3>(q)), "125 m³");
    auto       r   = q;
    const auto old = r--;
    EXPECT_EQ(to_string(r), "4 m");
    EXPECT_EQ(to_string(old), "5 m");
    EXPECT_EQ(unit_symbol(prefix_t<4>{}(si::metre)), "?m");
}

// A quantity cursor in another unit is a cursor there: the rescale keeps
// the marker the product dropped.
TEST(UnitsArmTest, a_quantity_cursor_converts_to_a_cursor) {
    cursor<id<"[0..10] [m]">, 1> c;
    ++c;
    const auto mm = c.in(si::milli(si::metre));
    static_assert(has_flag(policy_of<std::remove_cvref_t<decltype(mm)>::rep>, ins::detail::cursor_marker));
    EXPECT_EQ(to_string(*mm), "1000 mm");
}

constexpr auto km = si::kilo(si::metre);
template <class Q>
constexpr bool converts_to_km = requires(const Q& q) { q.template in<km>(); };

// point + quantity, where the quantity has no exact km (its fractions would
// pass 1024 bits there): the sum in the common unit, metres.
TEST(UnitsArmTest, point_plus_a_quantity_without_the_point_unit) {
    using V = id<key{grid{{0, 1}, max_denominator{ins::grid_number{ins::detail::big_int{1} << 510}}}}>;
    const id<quantity_key<si::metre, V>> q{rational{1, 3}, si::metre};
    static_assert(!converts_to_km<std::remove_cvref_t<decltype(q)>>);
    const auto p = point<km>(id<"[0..1/1000] 1/1000">{rational{1, 1000}}); // 1 m
    EXPECT_EQ(to_string(p + q), "4/3 m");
}
} // namespace reached_arms_tests

// Q{v, u} with a plain number in another unit of the quantity: stored once,
// onto Q's grid in u, by every rule of a store; then converted exactly.
namespace number_in_another_unit_tests {
using namespace ins;

using minutes    = id<"[0..600] [min]">;
using hundredths = id<"[0..600] 1/100 [min] round_half_even">;
using metres     = id<"[0..5000] [m]">;
using degrees    = id<"[0..360] [deg]">;

static_assert(minutes{120, si::second} == minutes{2, si::minute});
static_assert(minutes{1, si::hour} == minutes{60, si::minute});
static_assert(metres{2, si::kilo(si::metre)} == metres{2000, si::metre});
static_assert(metres{1.5, si::kilo(si::metre)} == metres{1500, si::metre});
// The call's flag rounds and bounds, in Q's notches: 90 s is 1.5 min.
static_assert(minutes{90, si::second, round_floor} == minutes{1, si::minute});
static_assert(minutes{90, si::second, round_ceil} == minutes{2, si::minute});
static_assert(minutes{1000000, si::second, clamp} == minutes{600, si::minute});
// Rep's own rounding mode: one rounding, of the number as written.
static_assert(hundredths{91, si::second} == hundredths{1.52, si::minute});
// A run-time number names its channel, as in Q's own unit; a constant off the
// notch, a unit of another quantity and a pi factor do not compile.
template <class Q, class V, class U>
concept makes = requires(V v) { Q{v, U{}}; };
static_assert(!makes<minutes, int, decltype(si::metre)>);
static_assert(!makes<degrees, int, decltype(si::radian)>);
static_assert(std::constructible_from<minutes, int, decltype(si::second), decltype(clamp | round_floor)>);

TEST(NumberInAnotherUnitTest, run_time_numbers_take_a_channel) {
    int n = 3;
    EXPECT_EQ(to_string(minutes{n * 60, si::second, clamp | round_floor}), "3 min");
    EXPECT_EQ(to_string(*minutes::try_make(n * 60, si::second)), "3 min");
    EXPECT_EQ(minutes::try_make(n * 61, si::second).error(), error::rounding_error);
    EXPECT_EQ(minutes::try_make(-60 * n, si::second).error(), error::overflow);
    EXPECT_EQ(to_string(*minutes::try_make<round_floor>(n * 61, si::second)), "3 min");
    // A double is rounded once, from its exact value: 45.3 is just below
    // 0.755 min.
    EXPECT_EQ(to_string(*hundredths::try_make(n * 15.1, si::second)), "0.75 min");
    error         ec{};
    const minutes m{n * 59, si::second, make_policy<checked>(ec)};
    EXPECT_NE(ec, error{});
    EXPECT_EQ(to_string(m), "0 min");
}
} // namespace number_in_another_unit_tests

// The double tier of sin, cos and value_cast across π decides only what the
// exact engine would: every value of the input grids, against
// pi_trig_exact and pi_cast_exact, under directed and nearest modes.
namespace pi_tier_tests {
using namespace ins;
namespace ud = ins::detail;

template <class Q, class Out>
void trig_sweep() {
#ifndef INSIDE_MATH_NO_FP
    static_assert(ud::pi_trig_fp<Out, false, Q::reference, typename Q::rep>, "the double tier applies");
#endif
    for (const auto v : values<typename Q::rep>) {
        const Q q = Q{v, si::degree};
        EXPECT_EQ((ud::pi_trig<Out, false>(q).raw()), (ud::pi_trig_exact<Out, false>(q).raw())) << to_string(v);
        EXPECT_EQ((ud::pi_trig<Out, true>(q).raw()), (ud::pi_trig_exact<Out, true>(q).raw())) << to_string(v);
    }
}
TEST(PiTierTest, sin_and_cos_of_degrees_match_the_exact_engine) {
    using deg   = id<"[0..360] 1/64 [deg]">;
    using wide  = id<"[-720..720] 1/8 [deg]">;
    using tenth = id<"[-90..90] 1/10 [deg]">;
    trig_sweep<deg, id<"[-1..1] 1/1048576 round_half_away">>();
    trig_sweep<deg, id<"[-1..1] 1/1048576 round_floor">>();
    trig_sweep<wide, id<"[-1..1] 1/4096 round_half_even">>();
    trig_sweep<wide, id<"[-1..1] 1/1000 round_ceil">>();
    trig_sweep<tenth, id<"[-1..1] 1/1073741824 round_toward_zero">>();
    trig_sweep<tenth, id<"[-1..1] 1/2 round_half_away">>(); // ties: 30°, 150°
}

template <auto U, class Out, class Q>
void cast_sweep(auto unit) {
    static_assert(
        ud::pi_scaled<ud::conversion_factor<Q::reference.unit(), decltype(U)::value>.Ratio, typename Q::rep>::ints,
        "the double tier applies");
    for (const auto v : values<typename Q::rep>) {
        const Q q = Q{v, unit};
        EXPECT_EQ((value_cast<U, Out>(q).numerical_value_in(U).raw()),
                  (ud::pi_cast_exact<ud::conversion_factor<Q::reference.unit(), decltype(U)::value>, Out>(q).raw()))
            << to_string(v);
    }
}
TEST(PiTierTest, value_cast_across_pi_matches_the_exact_engine) {
    using deg = id<"[-360..360] 1/64 [deg]">;
    using rad = id<"[-7..7] 1/1024 [rad]">;
    cast_sweep<si::radian, id<"[-8..8] 1/1048576 round_half_away">, deg>(si::degree);
    cast_sweep<si::radian, id<"[-8..8] 1/1073741824 round_floor">, deg>(si::degree);
    cast_sweep<si::degree, id<"[-402..402] 1/100 round_half_even">, rad>(si::radian);
    cast_sweep<si::degree, id<"[-402..402] 1/65536 round_toward_zero">, rad>(si::radian);
}

// asin, acos, atan and atan2 straight into degrees: rounded once (against a long double reference, within half a
// notch), the double tier deciding only what the exact engine decides, exact at Niven's angles.
template <class Out>
bool degrees_ok(const Out& got, long double radians) {
    const long double want = radians * 180 / 3.14159265358979323846264338327950288L;
    const long double n    = static_cast<long double>(static_cast<double>(notch_of<typename Out::rep>));
    return std::fabs(static_cast<long double>(got.numerical_value_in(si::degree).template to<double>()) - want) <=
           n / 2 + 1e-12L * (1 + std::fabs(want));
}
TEST(PiTierTest, arcs_into_degrees_round_once) {
    namespace entry = ins::math::detail::entry;
    using len       = id<"[-8..8] 1/4 [m]">;
    using deg       = id<"[-180..180] 1/1000 [deg] round_half_away">;
    int bad         = 0;
    for (const auto y : values<typename len::rep>)
        for (const auto x : values<typename len::rep>) {
            const deg f = math::atan2<deg>(len{y, si::metre}, len{x, si::metre});
            bad += !(f.numerical_value_in(si::degree).raw() == ud::pi_arc_exact<entry::atan2_fn, deg>(y, x).raw()) ||
                   !degrees_ok(f, std::atan2(static_cast<long double>(y.template to<double>()),
                                             static_cast<long double>(x.template to<double>())));
        }
    EXPECT_EQ(bad, 0);
    using ratio = id<"[-1..1] 1/64 [1]">;
    using half  = id<"[-90..90] 1/65536 [deg] round_floor">;
    using whole = id<"[0..180] 1/1000 [deg] round_half_even">;
    int bad1    = 0;
    for (const auto v : values<typename ratio::rep>) {
        const ratio       q{v, one};
        const long double r = v.template to<double>();
        const half        s = math::asin<half>(q);
        bad1 += !(s.numerical_value_in(si::degree).raw() == ud::pi_arc_exact<entry::asin_fn, half>(v).raw());
        bad1 += !(math::atan<half>(q).numerical_value_in(si::degree).raw() ==
                  ud::pi_arc_exact<entry::atan_fn, half>(v).raw());
        const whole c = math::acos<whole>(q);
        bad1 += !(c.numerical_value_in(si::degree).raw() == ud::pi_arc_exact<entry::acos_fn, whole>(v).raw());
        // round_floor: the value lies in [got, got + notch).
        const long double want = std::asin(r) * 180 / 3.14159265358979323846264338327950288L;
        const long double got  = static_cast<long double>(s.numerical_value_in(si::degree).template to<double>());
        bad1 += !(got <= want + 1e-12L && want - got < 0x1p-16L + 1e-12L);
        bad1 += !degrees_ok(c, std::acos(r));
    }
    EXPECT_EQ(bad1, 0);
    // Niven's angles, exactly: asin ½ = 30°, acos(-½) = 120°, atan 1 = 45°, atan2(-1, -1) = -135°.
    EXPECT_TRUE(math::asin<half>(ratio{0.5, one}) == 30_q * si::degree);
    EXPECT_TRUE(math::acos<whole>(ratio{-0.5, one}) == 120_q * si::degree);
    EXPECT_TRUE(math::atan<half>(ratio{1, one}) == 45_q * si::degree);
    EXPECT_TRUE(math::atan2<deg>(len{-1, si::metre}, len{-1, si::metre}) == -135_q * si::degree);
    // A unit C that may not hold every angle: an expected.
    using narrow = id<"[-45..45] 1/10 [deg] round_half_away">;
    static_assert(ud::is_expected_v<decltype(math::atan2<narrow>(len{}, len{}))>);
    EXPECT_EQ(math::atan2<narrow>(len{1, si::metre}, len{-1, si::metre}).error(), error::overflow);
    EXPECT_TRUE((*math::atan2<narrow>(len{1, si::metre}, len{2, si::metre}) == 26.6_q * si::degree));
}

// tan of a degree angle: rounded once, exact at 0 and ±45°, its bound from the lattice point nearest a pole.
TEST(PiTierTest, tan_of_degrees_rounds_once) {
    using deg = id<"[-89.75..89.75] 1/4 [deg]">;
    using out = id<"[-512..512] 1/4096 round_half_away">;
    static_assert(std::same_as<decltype(math::tan<out>(deg{})), out>); // |tan| ≤ 1/(2·¼·(1/180)) = 360
    int bad = 0;
    for (const auto v : values<typename deg::rep>) {
        const long double a = v.template to<double>() * 3.14159265358979323846264338327950288L / 180;
        const out         t = math::tan<out>(deg{v, si::degree});
        bad += !(std::fabs(static_cast<long double>(t.template to<double>()) - std::tan(a)) <=
                 0x1p-13L + 1e-12L * (1 + std::fabs(std::tan(a))));
    }
    EXPECT_EQ(bad, 0);
    EXPECT_TRUE(math::tan<out>(deg{45, si::degree}) == 1 && math::tan<out>(deg{-45, si::degree}) == -1);
    EXPECT_TRUE(math::tan<out>(deg{0, si::degree}) == 0);
    // A result that may not hold every value: an expected.
    using small = id<"[-100..100] 1/1000 round_half_away">;
    static_assert(ud::is_expected_v<decltype(math::tan<small>(deg{}))>);
    EXPECT_EQ(math::tan<small>(deg{89.5, si::degree}).error(), error::overflow);
    EXPECT_TRUE((*math::tan<small>(deg{30, si::degree}) == 0.577_q));
}
} // namespace pi_tier_tests

// std::cw<v> is the point v for quantities and points too: v * unit, q * std::cw<2>, Q{std::cw<v>, u}.
namespace constant_wrapper_tests {
using namespace ins;

template <int N>
constexpr auto metres() { return std::cw<N> * si::metre; }

TEST(UnitsTest, constant_wrapper_operands) {
    using L = id<"[0..100] [m]">;
    static_assert(metres<5>() == 5_q * si::metre);
    constexpr L l{std::cw<42>, si::metre};
    static_assert(l == 42_q * si::metre && l * std::cw<2> == 84_q * si::metre && std::cw<2> * l == l + l);
    static_assert(!std::is_constructible_v<L, std::constant_wrapper<42>>);
    using C = id<"[-40..125] [deg_C] @ice_point">;
    constexpr C c{std::cw<21>, si::degree_Celsius};
    static_assert(c == C{21, si::degree_Celsius});
    EXPECT_EQ(to_string(l / std::cw<2>), "21 m");
}
} // namespace constant_wrapper_tests
