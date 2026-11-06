// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Units: dimensions, units, conversions, kinds, the API, inside's algorithms
// on quantities, math on quantities, and the physical constants.

#include <beman/inside/units.hpp>
#include <beman/inside/units/constants.hpp>
#include <beman/inside/units/io.hpp>
#include <beman/inside/units/math.hpp>

#include <gtest/gtest.h>

#include <array>
#include <concepts>
#include <cstdint>
#include <format>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

// units.hpp: dimensions, units, conversions, quantity kinds, arithmetic, text.
namespace units_tests {
using namespace beman::inside;
using namespace beman::inside::units;

namespace {
constexpr auto km = si::kilo(si::metre);
constexpr auto mm = si::milli(si::metre);

template <class A, class B>
consteval bool addable() {
    return units::detail::common_spec(std::remove_cvref_t<A>::reference, std::remove_cvref_t<B>::reference).Ok;
}
} // namespace

TEST(UnitsTest, dimensions_and_magnitudes) {
    static_assert(dimension_of(si::newton.value) == dim_mass * dim_length / dim_time.pow(2));
    static_assert(magnitude_of(km.value) == magnitude{grid_rational{1000}});
    static_assert(magnitude_of(si::kilogram.value).is_one());
    static_assert(magnitude_of(si::gram.value) == magnitude{grid_rational{rational{1, 1000}}});
    static_assert(magnitude_of(si::hour.value) == magnitude{grid_rational{3600}});
    static_assert(magnitude_of(international::inch.value) == magnitude{grid_rational{rational{254, 10000}}});
    static_assert(magnitude_of(si::degree.value).Pi == 1);
    // m·s/s is m again, and the factors are kept in one order.
    static_assert(si::metre * si::second / si::second == si::metre);
    static_assert(std::is_same_v<decltype(si::metre * si::second), decltype(si::second * si::metre)>);
    // J is not N·m as a unit (another symbol), but the same magnitude and dimension.
    static_assert(dimension_of(si::joule.value) == dimension_of((si::newton * si::metre).value));
}

TEST(UnitsTest, conversion_rescales_the_grid_and_keeps_the_raw) {
    const quantity<km, inside<{0, 10}>> d{7};
    const auto                          m = d.in(si::metre);
    using M                               = std::remove_cvref_t<decltype(m.numerical_value())>;
    static_assert(lower_of<M> == 0 && upper_of<M> == 10000 && notch_of<M> == 1000);
    EXPECT_EQ(m.numerical_value(), 7000);
    EXPECT_EQ(m.numerical_value().raw(), d.numerical_value().raw());
    static_assert(sizeof(M) == sizeof(inside<{0, 10}>));
    // m → km: notch 1/1000, exact.
    const quantity<si::metre, inside<{0, 1500}>> l{1234};
    EXPECT_EQ(rational{l.in(km).numerical_value()}, rational(1234, 1000));
    // inch → mm, exact (25.4).
    const quantity<international::inch, inside<{0, 12}>> in{3};
    EXPECT_EQ(rational{in.in(mm).numerical_value()}, rational(762, 10));
    EXPECT_EQ(rational{in.numerical_value_in(international::foot)}, rational(1, 4));
}

TEST(UnitsTest, sums_run_in_the_common_unit) {
    const auto five_km = just<5> * km;
    const auto three_m = just<3> * si::metre;
    const auto s       = five_km + three_m;
    static_assert(std::remove_cvref_t<decltype(s)>::unit == si::metre.value);
    EXPECT_EQ(s.numerical_value(), 5003);
    EXPECT_EQ(to_string(s), "5003 m");
    // A range: the result grid is the exact hull, in metres.
    const quantity<km, inside<{0, 10}>>           a{2};
    const quantity<si::metre, inside<{0, 10000}>> b{500};
    const auto                                    c = a + b;
    using C                                         = std::remove_cvref_t<decltype(c.numerical_value())>;
    static_assert(lower_of<C> == 0 && upper_of<C> == 20000);
    EXPECT_EQ(c.numerical_value(), 2500);
    // Neither divides the other: ft + m runs in 1/1250 m (= 1/381 ft).
    const auto f = just<1> * international::foot + just<1> * si::metre;
    EXPECT_EQ(rational{f.in(si::metre).numerical_value()}, rational(1631, 1250)); // 1.3048 m
    EXPECT_EQ(unit_symbol(std::remove_cvref_t<decltype(f)>::unit), "[1/381 ft]");
    // The smaller unit is scaled, whichever side it is on: one type.
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(f)>,
                                 decltype(just<1> * si::metre + just<1> * international::foot)>);
    // Comparisons across units.
    EXPECT_TRUE(just<1> * km == just<1000> * si::metre);
    EXPECT_TRUE(just<999> * si::metre < just<1> * km);
}

TEST(UnitsTest, products_and_quotients) {
    const quantity<si::metre, inside<{0, 100}>> d{42};
    const quantity<si::second, inside<{1, 10}>> t{4};
    const auto                                  v = d / t; // 1 ≤ t: no zero divisor, a plain value
    EXPECT_EQ(rational{v.numerical_value()}, rational(21, 2));
    EXPECT_EQ(to_string(v), "10.5 m/s");
    // A divisor whose grid holds 0: inside's expected.
    const quantity<si::second, inside<{0, 10}>> t0{0};
    const auto                                  e = d / t0;
    static_assert(units::detail::is_expected_v<std::remove_cvref_t<decltype(e)>>);
    EXPECT_FALSE(e.has_value());
    // N·m is an energy, implicitly a joule.
    const auto                            w = (just<3> * si::newton) * d;
    quantity<si::joule, inside<{0, 300}>> j = w;
    EXPECT_EQ(j.numerical_value(), 126);
    // Scaling by a number.
    EXPECT_EQ((d * just<2>).numerical_value(), 84);
    EXPECT_EQ((d / just<2>).numerical_value(), 21);
    EXPECT_EQ(to_string(just<2> / si::second), "2 s⁻¹");
}

TEST(UnitsTest, dimensionless_quantities_are_numbers) {
    const quantity<si::kilogram, inside<{1, 100}>> a{21};
    const quantity<si::kilogram, inside<{1, 100}>> b{6};
    const inside<{{0, 100}, per<2>}>               r = a / b; // 7/2, implicitly
    EXPECT_EQ(rational{r}, rational(7, 2));
    // km/m does not cancel to unit one: its number is ×1000.
    const auto           ratio = (just<3> * km) / (just<2> * si::metre);
    const inside<{1500}> n     = ratio;
    EXPECT_EQ(n, 1500);
    // Into a grid that must round: explicit, through that inside's constructor.
    const auto x = inside<{{0, 10}, per<10>}, round_nearest>(a / b * just<1, 3>);
    EXPECT_EQ(rational{x}, rational(12, 10));
    // An angle is a quantity, not a number.
    static_assert(!std::is_convertible_v<quantity<si::radian, inside<{0, 7}>>, inside<{0, 7}>>);
    static_assert(!std::is_constructible_v<inside<{0, 7}>, quantity<si::radian, inside<{0, 7}>>>);
    static_assert(!std::is_convertible_v<quantity<si::metre, inside<{0, 7}>>, inside<{0, 7}>>);
}

TEST(UnitsTest, magnitudes_scale_the_unit_not_the_number) {
    const quantity<si::second, inside<{0, 10}>> t{3};
    const auto                                  p = mag_pi * t;
    static_assert(magnitude_of(std::remove_cvref_t<decltype(p)>::unit).Pi == 1);
    EXPECT_EQ(p.numerical_value(), 3); // the number is untouched
    EXPECT_EQ(to_string(p), "3 [π s]");
    EXPECT_EQ(unit_symbol(std::remove_cvref_t<decltype(p)>::unit, text_encoding::ascii), "[pi s]");
    EXPECT_EQ(to_string(t * mag<2>), "3 [2 s]");
    EXPECT_EQ((t * mag<2>).in(si::second).numerical_value(), 6);
    EXPECT_EQ(unit_symbol(std::remove_cvref_t<decltype(t / mag_pi)>::unit), "[π⁻¹ s]");
    // π · π⁻¹ cancels: back to seconds, exactly.
    EXPECT_EQ((mag_pi * t / mag_pi).in(si::second).numerical_value(), 3);
    // A magnitude times a number is that number in [π], then [π s].
    EXPECT_EQ(to_string(mag_pi * just<2> * si::second), "2 [π s]");
    EXPECT_EQ(to_string(just<2> * mag_pi), "2 [π]");
    EXPECT_EQ(unit_symbol(mag_pi / si::metre), "[π m⁻¹]");
}

TEST(UnitsTest, compound_assignment_and_casts) {
    quantity<si::metre, inside<{0, 10000}>> d{100};
    d += just<1> * km;
    EXPECT_EQ(d.numerical_value(), 1100);
    d -= just<100> * si::metre;
    EXPECT_EQ(d.numerical_value(), 1000);
    const auto narrow = value_cast<inside<{0, 2000}>>(d);
    EXPECT_EQ(narrow.numerical_value(), 1000);
    EXPECT_EQ((try_value_cast<inside<{0, 500}>>(d).error()), errc::overflow);
    // In another unit: 1000 m is 1 km; 1001 m rounds onto whole km by F, and
    // is past a {0, 1} km grid.
    EXPECT_EQ((try_value_cast<km, inside<{{0, 5}, per<1000>}>>(d)->numerical_value()), 1);
    d += just<1> * si::metre;
    EXPECT_EQ((try_value_cast<km, inside<{0, 5}>, round_nearest>(d)->numerical_value()), 1);
    EXPECT_EQ((try_value_cast<km, inside<{{0, 1}, per<1000>}>>(d).error()), errc::overflow);
}

TEST(UnitsTest, kinds_convert_up_implicitly_and_down_explicitly) {
    using height_m = quantity<isq::height[si::metre], inside<{0, 100}>>;
    using length_m = quantity<isq::length[si::metre], inside<{0, 100}>>;
    using width_m  = quantity<isq::width[si::metre], inside<{0, 100}>>;
    static_assert(std::is_convertible_v<height_m, length_m>);
    static_assert(!std::is_convertible_v<length_m, height_m>);
    static_assert(!std::is_constructible_v<width_m, height_m>);
    const height_m h{3};
    const length_m l = h;
    EXPECT_EQ(l.numerical_value(), 3);
    const auto back = quantity_cast<isq::height>(l);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(back)>, height_m>);
    // A plain unit's kind (si::metre: any length) takes every length.
    static_assert(std::is_convertible_v<quantity<si::metre, inside<{0, 100}>>, height_m>);
    static_assert(std::is_convertible_v<height_m, quantity<si::metre, inside<{0, 100}>>>);
    // width + height: their common ancestor, length.
    const auto s = width_m{1} + h;
    static_assert(std::remove_cvref_t<decltype(s)>::reference.Spec->Name == "length");
}

TEST(UnitsTest, derived_kinds_follow_their_recipes) {
    using W   = quantity<isq::width[si::metre], inside<{0, 10}>>;
    using H   = quantity<isq::height[si::metre], inside<{0, 10}>>;
    using A   = quantity<isq::area[square(si::metre)], inside<{0, 100}>>;
    const A a = W{2} * H{3}; // width·height → area
    EXPECT_EQ(a.numerical_value(), 6);
    // mass·speed² → kinetic energy; force·length → energy, but not torque.
    using kg_t = quantity<isq::mass[si::kilogram], inside<{0, 10}>>;
    using v_t  = quantity<isq::speed[si::metre / si::second], inside<{0, 10}>>;
    using ke_t = quantity<isq::kinetic_energy[si::joule], inside<{0, 1000}>>;
    static_assert(std::is_convertible_v<decltype(kg_t{} * v_t{} * v_t{}), ke_t>);
    using f_t = quantity<isq::force[si::newton], inside<{0, 10}>>;
    using l_t = quantity<isq::length[si::metre], inside<{0, 10}>>;
    static_assert(std::is_convertible_v<decltype(f_t{} * l_t{}), quantity<isq::energy[si::joule], inside<{0, 100}>>>);
    static_assert(!std::is_convertible_v<decltype(f_t{} * l_t{}),
                                         quantity<isq::torque[si::newton * si::metre], inside<{0, 100}>>>);
    // m/s (no kind) is any speed.
    static_assert(std::is_convertible_v<quantity<si::metre / si::second, inside<{0, 10}>>, v_t>);
}

TEST(UnitsTest, separate_kinds_do_not_mix) {
    const auto hz   = just<1> * si::hertz;
    const auto bq   = just<1> * si::becquerel;
    const auto rad  = just<1> * si::radian;
    const auto one_ = just<1> * (si::metre / si::metre);
    static_assert(!addable<decltype(hz), decltype(bq)>());
    static_assert(!addable<decltype(rad), decltype(one_)>());
    static_assert(addable<decltype(hz), decltype(just<1> / si::second)>());
    static_assert(
        !std::is_convertible_v<quantity<si::hertz, inside<{0, 10}>>, quantity<si::becquerel, inside<{0, 10}>>>);
    // Gy and Sv: both J/kg, different kinds.
    static_assert(!addable<decltype(just<1> * si::gray), decltype(just<1> * si::sievert)>());
    static_assert(!units::detail::spec_casts(std::remove_cvref_t<decltype(rad)>::reference, &isq_defs::dimensionless));
}

TEST(UnitsTest, text) {
    EXPECT_EQ(unit_symbol(si::kilogram * square(si::metre) / square(si::second)), "kg m²/s²");
    EXPECT_EQ(unit_symbol(si::kilogram * square(si::metre) / square(si::second), text_encoding::ascii), "kg m^2/s^2");
    EXPECT_EQ(unit_symbol(si::joule / (si::kilogram * si::kelvin)), "J/(K kg)");
    EXPECT_EQ(unit_symbol(si::micro(si::metre)), "µm");
    EXPECT_EQ(unit_symbol(si::micro(si::metre), text_encoding::ascii), "um");
    EXPECT_EQ(unit_symbol(si::ohm), "Ω");
    EXPECT_EQ(to_string(just<90> * si::degree), "90°");
    EXPECT_EQ(to_string(just<3> * km / si::hour), "3 km/h");
    EXPECT_EQ(to_string(just<3> * units::one), "3");
    std::ostringstream os;
    os << just<5> * si::newton;
    EXPECT_EQ(os.str(), "5 N");
}

TEST(UnitsTest, constants_are_units) {
    const auto c = (just<1> * si2019::speed_of_light_in_vacuum).in(si::metre / si::second);
    EXPECT_EQ(c.numerical_value(), 299'792'458);
    EXPECT_EQ(to_string(just<2> * si2019::speed_of_light_in_vacuum), "2 c");
    const auto g = (just<1> * si2019::standard_gravity).in(si::metre / square(si::second));
    EXPECT_EQ(to_string(g), "9.80665 m/s²");
}

TEST(UnitsTest, big_grid_prefixes_and_units) {
    static_assert(magnitude_of(si::yocto(si::gram).value) == magnitude{1e-27_g});
    const auto e = (just<1> * si::electronvolt).in(si::joule);
    EXPECT_EQ(to_string(e), "0.0000000000000000001602176634 J");
    // E = h·ν, exactly: one photon of the caesium frequency.
    const auto en =
        (just<1> * si2019::planck_constant) * (just<1> * si2019::hyperfine_structure_transition_frequency_of_cs);
    EXPECT_EQ(to_string(en.in(si::joule)), "0.00000000000000000000000609110229711386655 J");
}

TEST(UnitsTest, points_shift_their_grid_between_origins) {
    const auto t = point<si::degree_Celsius>(inside<{-40, 60}>{20});
    static_assert(std::remove_cvref_t<decltype(t)>::origin->Name == "ice_point");
    EXPECT_EQ(to_string(t), "20 ℃");
    // From absolute zero, still in ℃: 293.15, on a shifted grid.
    const auto q = t.quantity_from(si::absolute_zero);
    using Q      = std::remove_cvref_t<decltype(q.numerical_value())>;
    static_assert(lower_of<Q> == rational(23315, 100) && upper_of<Q> == rational(33315, 100));
    EXPECT_EQ(rational{q.numerical_value()}, rational(29315, 100));
    // As a kelvin point.
    const auto k = t.point_for(si::absolute_zero).in(si::kelvin);
    EXPECT_EQ(to_string(k), "293.15 K");
    EXPECT_TRUE(k == t);
    // 68 ℉ is 20 ℃.
    const auto f = point<international::degree_Fahrenheit>(inside<{0, 100}>{68});
    EXPECT_TRUE(f == t);
    EXPECT_EQ(rational{f.point_for(si::ice_point).in(si::degree_Celsius).quantity_from_origin().numerical_value()},
              rational(20));
    // Implicit conversion to a kelvin point type.
    using kelvin_point    = quantity_point<si::kelvin, &si_defs::absolute_zero, inside<{{0, 400}, per<100>}>>;
    const kelvin_point kp = t;
    EXPECT_EQ(rational{kp.quantity_from_origin().numerical_value()}, rational(29315, 100));
}

TEST(UnitsTest, point_arithmetic) {
    auto       t  = point<si::degree_Celsius>(inside<{-40, 60}>{20});
    const auto t2 = t + delta<si::kelvin>(just<5>);
    EXPECT_EQ(to_string(t2), "25 ℃");
    const auto d = t2 - point<si::kelvin>(inside<{250, 300}>{273}); // 25 ℃ − 273 K = 25.15 K
    EXPECT_EQ(rational{d.numerical_value()}, rational(2515, 100));
    t += delta<si::degree_Celsius>(just<1>);
    EXPECT_EQ(to_string(t), "21 ℃");
    const auto z = si::absolute_zero + just<300> * si::kelvin;
    EXPECT_TRUE(z > t);
    // A unit without an origin counts from the implicit zero.
    const auto x = point<si::metre>(inside<{0, 10}>{3});
    static_assert(!std::remove_cvref_t<decltype(x)>::origin);
    EXPECT_EQ((x - point<si::metre>(just<1>)).numerical_value(), 2);
}

TEST(UnitsTest, std_format) {
    EXPECT_EQ(std::format("{}", just<5> * si::kilo(si::metre)), "5 km");
    EXPECT_EQ(std::format("{:>4}", just<5> * si::newton), "   5 N");
    EXPECT_EQ(std::format("{}", just<90> * si::degree), "90°");
    const quantity<si::metre / si::second, inside<{{0, 10}, per<4>}>> v{rational(9, 4)};
    EXPECT_EQ(std::format("{:.1f}", v), "2.2 m/s");
    EXPECT_EQ(std::format("{}", point<si::degree_Celsius>(inside<{-40, 60}>{20})), "20 ℃");
    EXPECT_EQ(std::format("{}", just<3> * units::one), "3");
}

TEST(UnitsTest, point_in_and_value_cast_on_points) {
    const auto oven = point<international::degree_Fahrenheit>(inside<{0, 600}>{350});
    const auto c    = oven.point_in(si::degree_Celsius); // (350 − 32)·5/9
    static_assert(std::remove_cvref_t<decltype(c)>::origin->Name == "ice_point");
    EXPECT_EQ(rational{c.quantity_from_origin().numerical_value()}, rational(530, 3));
    EXPECT_EQ(to_string(c.point_in(si::kelvin)), "26989/60 K"); // 449.8166…: exact, so N/D
    const auto r = value_cast<inside<{0, 300}, round_nearest>>(c);
    EXPECT_EQ(to_string(r), "177 ℃");
    const auto k = value_cast<si::kelvin, inside<{{0, 1000}, per<10>}, round_nearest>>(c.point_for(si::absolute_zero));
    EXPECT_EQ(to_string(k), "449.8 K");
}
} // namespace units_tests

// The units API review before 0.4: identity of specs and units, honest
// constraints with reasons, spec queries, value_cast to a quantity type,
// unit · magnitude, canonical common units, text, points.
namespace units_api_tests {
using namespace beman::inside;
using namespace beman::inside::units;

namespace {
constexpr auto km = si::kilo(si::metre);

// A spec that reuses a catalog name under another parent, and two units that
// differ only in the kind they measure.
inline constexpr quantity_spec length_of_width{"length", isq::width};
inline constexpr named_unit    per_second_f{"Hz", one / si::second, isq::frequency};
inline constexpr named_unit    per_second_a{"Hz", one / si::second, isq::activity};
inline constexpr point_origin  other_zero{"other_zero", dim_thermodynamic_temperature};

// unit_options: an ASCII symbol, a kind, an origin, no space before.
inline constexpr named_unit rankine_def{
    "°R", mag<rational{5, 9}> * si::kelvin, {.ascii = "R", .origin = si::absolute_zero}};
inline constexpr named_unit turn_def{"tr", mag<2> * mag_pi * si::radian, {.space_before = false}};
inline constexpr named_unit rpm_def{"rpm", one / si::minute, {.kind = isq::frequency}};

// A base dimension of your own.
inline constexpr base_dimension currency_def{"currency"};
inline constexpr quantity_spec  money_def{"money", currency_def};
inline constexpr named_unit     euro_def{"EUR", spec_c<money_def>};
inline constexpr named_unit     cent_def{"ct", mag<rational{1, 100}> * unit_c<euro_def>};

// ADC counts: a dimension apart from SI's dimensionless one.
inline constexpr base_dimension count_def{"count"};
inline constexpr quantity_spec  adc_counts_def{"adc_counts", count_def};
inline constexpr named_unit     cts_def{"cts", spec_c<adc_counts_def>};

template <class A, class B>
concept can_add = requires(const A& a, const B& b) { a + b; };
template <class A, class B>
consteval bool addable_units() {
    return can_add<A, B>;
}
template <class A, class B>
concept can_less = requires(const A& a, const B& b) { a < b; };
template <class A>
concept can_scale_raw = requires(const A& a) { a * 2; } || requires(const A& a) { 2 * a; } ||
                        requires(const A& a) { a / 2; } || requires(const A& a) { 2 / a; };
template <class A>
concept can_in_seconds = requires(const A& a) { a.in(si::second); };
template <class A>
concept can_in_radians = requires(const A& a) { a.in(si::radian); };
template <class A>
concept can_cast_to_duration = requires(const A& a) { quantity_cast<isq::duration>(a); };
template <class A, class B>
concept can_atan2 = requires(const A& a, const B& b) { units::atan2(a, b); };
template <class Q, class A>
concept can_value_cast = requires(const A& a) { value_cast<Q>(a); };
template <class A, class B>
concept can_subtract = requires(const A& a, const B& b) { a - b; };
template <class P>
concept can_point_in_seconds = requires(const P& p) { p.point_in(si::second); };

using metres  = quantity<si::metre, inside<{{-10, 10}, per<8>}, round_nearest>>;
using seconds = quantity<si::second, inside<{{-10, 10}, per<8>}, round_nearest>>;
using degrees = quantity<si::degree, inside<{0, 360}>>;
} // namespace

TEST(UnitsApiTest, specs_and_units_are_identified_by_all_their_parts) {
    // Same name and dimension, another parent: another spec.
    static_assert(!(spec_c<length_of_width> == isq::length));
    static_assert(spec_c<length_of_width> == spec_c<length_of_width>);
    // Same symbols, dimension and magnitude, another kind: another unit.
    static_assert(!(unit_c<per_second_f> == unit_c<per_second_a>));
    static_assert(unit_c<per_second_f> == unit_c<per_second_f>);
}

TEST(UnitsApiTest, mismatches_are_constraint_failures) {
    static_assert(can_add<metres, metres> && !can_add<metres, seconds> && !can_less<metres, seconds>);
    static_assert(!can_add<decltype(just<1> * si::hertz), decltype(just<1> * si::becquerel)>);
    static_assert(!can_scale_raw<metres>);
    static_assert(!can_in_seconds<metres> && can_in_radians<quantity<si::radian, inside<{0, 7}>>>);
    static_assert(!can_in_radians<degrees>); // a factor of pi: value_cast rounds it
    static_assert(!can_cast_to_duration<metres>);
    static_assert(can_atan2<metres, metres> && !can_atan2<metres, seconds>);
}

TEST(UnitsApiTest, tags_for_your_own_entries) {
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(spec_c<length_of_width>)>, spec_t<&length_of_width>>);
    constexpr auto zero = origin_c<other_zero>;
    const auto     p    = zero + just<5> * si::kelvin;
    EXPECT_EQ(p.quantity_from_origin().numerical_value(), 5);
    static_assert(!can_subtract<decltype(p), decltype(point<si::kelvin>(just<1>))>); // two absolute origins
}

TEST(UnitsApiTest, quantity_spec_is_a_value) {
    const quantity<isq::altitude[km], inside<{100, 1000}>> altitude{185};
    const quantity<isq::radius[km], inside<{6300, 6400}>>  earth{6371};
    const auto                                             r = altitude + earth;
    static_assert(r.quantity_spec == isq::length && !(r.quantity_spec == isq::height));
    static_assert(altitude.quantity_spec == isq::altitude);
    static_assert(decltype(r / (just<2> * si::second))::quantity_spec == isq::length / isq::duration);
    static_assert(isq::length / isq::duration == isq::length / isq::time);
}

TEST(UnitsApiTest, value_cast_to_a_quantity_type) {
    using km_q = quantity<km, inside<{{0, 20}, per<1000>}>>;
    const quantity<si::metre, inside<{0, 20000}>> d{12345};
    const auto                                    a = value_cast<km_q>(d);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(a)>, km_q>);
    EXPECT_EQ(to_string(a), "12.345 km");
    EXPECT_EQ((try_value_cast<quantity<km, inside<{0, 20}>>, round_floor>(d)->numerical_value()), 12);
    EXPECT_EQ((try_value_cast<quantity<km, inside<{{0, 5}, per<1000>}>>>(d).error()), errc::overflow);
    // The spec must convert as for an implicit conversion: up the tree, not down.
    using height_q = quantity<isq::height[si::metre], inside<{0, 9}>>;
    using length_q = quantity<isq::length[si::metre], inside<{0, 9}>>;
    static_assert(can_value_cast<length_q, height_q> && !can_value_cast<height_q, length_q>);
    static_assert(!can_value_cast<km_q, seconds>);
}

TEST(UnitsApiTest, units_scale_by_magnitudes_on_either_side) {
    static_assert(si::metre * mag<2> == mag<2> * si::metre);
    static_assert(si::metre / mag_pi == mag<1> / mag_pi * si::metre);
    EXPECT_EQ(unit_symbol(si::metre * mag<2>), "[2 m]");
}

TEST(UnitsApiTest, sums_take_one_type_in_either_order) {
    const auto j  = just<1> * si::joule;
    const auto nm = just<1> * (si::newton * si::metre);
    static_assert(std::is_same_v<decltype(j + nm), decltype(nm + j)>);
    EXPECT_EQ(to_string(nm + j), "2 J");
}

TEST(UnitsApiTest, a_denominator_alone_prints_negative_exponents) {
    EXPECT_EQ(to_string(just<2> / si::second), "2 s⁻¹");
    EXPECT_EQ(to_string(just<1> / (si::second * si::metre)), "1 m⁻¹ s⁻¹");
    EXPECT_EQ(unit_symbol(inverse(si::second), text_encoding::ascii), "s^-1");
    EXPECT_EQ(unit_symbol(mag_pi / si::metre), "[π m⁻¹]");
    EXPECT_EQ(unit_symbol(si::metre / si::second), "m/s");
}

TEST(UnitsApiTest, points_from_zero_and_their_constraints) {
    const auto c = point<si::degree_Celsius>(inside<{{-50, 50}, per<10>}>{20});
    EXPECT_EQ(to_string(c.quantity_from_zero()), "293.15 ℃");
    EXPECT_EQ(to_string(point<si::metre>(just<5>).quantity_from_zero()), "5 m");
    static_assert(!can_in_seconds<decltype(c)> && !can_point_in_seconds<decltype(c)>);
}

TEST(UnitsApiTest, unit_options) {
    constexpr auto rankine = unit_c<rankine_def>;
    EXPECT_EQ(unit_symbol(rankine, text_encoding::ascii), "R");
    EXPECT_EQ(to_string(point<rankine>(just<9>).point_in(si::kelvin)), "5 K"); // counted from absolute zero
    EXPECT_EQ(to_string(just<1> * unit_c<turn_def>), "1tr");
    static_assert(kind_of(unit_c<rpm_def>.value).P->Name == "frequency");
    static_assert(!addable_units<decltype(just<1> * unit_c<rpm_def>), decltype(just<1> * si::becquerel)>());
}

TEST(UnitsApiTest, base_dimensions_of_your_own) {
    constexpr auto euro = unit_c<euro_def>;
    constexpr auto cent = unit_c<cent_def>;
    const auto     sum  = just<2> * euro + just<5> * cent;
    EXPECT_EQ(to_string(sum), "205 ct");
    static_assert(!can_add<decltype(just<1> * euro), decltype(just<1> * si::metre)>);
    static_assert(!can_add<decltype(just<1> * euro), decltype(just<1> * one)>); // not a plain number
    static_assert(!(dimension_of((euro / si::kilogram).value) == dimension_of(euro.value)));
    static_assert(dimension_of(square(euro).value).root(2) == dimension_of(euro.value));
    static_assert(dimension_of((euro / cent).value) == dim_one);
    EXPECT_EQ(to_string((just<3> * euro) / (just<2> * si::kilogram)), "1.5 EUR/kg");
}

TEST(UnitsApiTest, adc_counts) {
    constexpr auto cts = unit_c<cts_def>;
    using reading      = quantity<cts, inside<{0, 4095}>>;
    using gain_t       = quantity<cts / si::volt, inside<{{1000, 1500}, per<1000>}, round_nearest>>;
    using volts_t      = quantity<si::volt, inside<{{0, 5}, per<1000>}>>;
    const reading                          raw{2048};
    const quantity<cts, inside<{-64, 64}>> offset{12};
    const gain_t                           gain{rational{1240909, 1000}};
    const auto                             v = (raw - offset) / gain; // zero-free gain: a plain quantity
    EXPECT_EQ(to_string(value_cast<quantity<si::milli(si::volt), inside<{-5000, 5000}, round_nearest>>>(v)),
              "1641 mV");
    // Two-point calibration: the exact Δcts / ΔV, checked into gain_t once.
    const quantity<si::volt, inside<{{0, 1}, per<1000>}>> v1{rational{1, 2}};
    const quantity<si::volt, inside<{{2, 5}, per<1000>}>> v2{rational{5, 2}};
    const auto g = try_value_cast<gain_t>((reading{3114} - reading{632}) / (v2 - v1));
    ASSERT_TRUE(g.has_value());
    EXPECT_EQ(to_string(*g), "1241.000 cts/V");
    EXPECT_FALSE(try_value_cast<gain_t>((reading{632} - reading{632}) / (v2 - v1)).has_value());
    static_assert(!can_add<reading, volts_t>);
    static_assert(!can_add<reading, decltype(just<1> * one)>); // counts are not a plain number
    static_assert(!(dimension_of((cts / si::volt).value) == dimension_of((one / si::volt).value)));
    static_assert(!std::constructible_from<quantity<si::ampere, inside<{0, 5}>>, decltype(v)>);
}
} // namespace units_api_tests

// inside's algorithms on quantities: policy hooks, ++/--, try_make,
// mul_into/div_into, add_all/mul_all/sum, parse, the conversion predicates,
// the clamp constructor/the wrap constructor, dot/cross/lerp, and sin_into/cos_into of an angle in
// a unit with a factor of π.
namespace units_algorithms_tests {
using namespace beman::inside;
using namespace beman::inside::units;

namespace {
inline constexpr base_dimension health_dim{"health"};
inline constexpr quantity_spec  health_def{"health", health_dim};
inline constexpr named_unit     hp_def{"HP", spec_c<health_def>};
inline constexpr auto           HP = unit_c<hp_def>;
inline constexpr named_unit     slot_def{"slot", mag<rational{1, 32768}> * mag_pi * si::radian};
inline constexpr auto           slot = unit_c<slot_def>;

constexpr auto ms = si::milli(si::second);

using seconds_t = quantity<si::second, inside<{0, 60, bounds::half_open}, wrap>>;
using hp_t      = quantity<HP, inside<{0, 100}, clamp>>;
using usd_t     = quantity<HP, inside<{{0, 1000}, per<100>}, round_nearest>>;

template <class Out, class A, class B>
concept can_mul_into = requires(const A& a, const B& b) { mul_into<Out>(a, b); };
template <class Q, class X>
concept can_wrap_cast = requires(const X& x) { Q{x, wrap}; };
template <class A, class B>
concept can_add_into = requires(A& a, const B& b) { a.on_wrap([](auto&, auto) {}) += b; };
} // namespace

TEST(UnitsAlgorithmsTest, hooks_take_quantities_and_see_the_quantity) {
    seconds_t s{50};
    int       carry = -1;
    s.on_wrap([&](seconds_t& self, auto c) {
        carry = as<int>(c);
        EXPECT_EQ(&self, &s);
    }) += just<25> * si::second;
    EXPECT_EQ(to_string(s), "15 s");
    EXPECT_EQ(carry, 1);
    s.on_wrap([&](auto&, auto c) { carry = as<int>(c); }) += just<2> * si::minute; // in seconds, exactly
    EXPECT_EQ(to_string(s), "15 s");
    EXPECT_EQ(carry, 2);
    static_assert(!can_add_into<seconds_t, decltype(just<1> * si::metre)>);

    hp_t hp{75};
    bool downed = false;
    hp.on_clamp([&](hp_t& self, auto) { downed = self == just<0> * HP; }) -= just<200> * HP;
    EXPECT_TRUE(downed);
    EXPECT_EQ(to_string(hp), "0 HP");
}

TEST(UnitsAlgorithmsTest, increment_is_one_unit_and_try_make) {
    hp_t hp{98};
    ++hp;
    EXPECT_EQ(to_string(hp++), "99 HP");
    ++hp; // clamps at 100
    --hp;
    EXPECT_EQ(to_string(hp), "99 HP");
    const auto ok  = hp_t::try_make(5);
    const auto bad = quantity<HP, inside<{0, 100}>>::try_make(500);
    ASSERT_TRUE(ok);
    EXPECT_EQ(to_string(*ok), "5 HP");
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error(), errc::overflow);
}

TEST(UnitsAlgorithmsTest, mul_into_and_div_into_fold_the_unit_factor) {
    // 3 m/s for 7 s, stored in km to the metre: 0.021 km.
    using km_t   = quantity<si::kilo(si::metre), inside<{{0, 10}, per<1000>}, round_nearest>>;
    const auto d = mul_into<km_t>(just<3> * si::metre / si::second, quantity<si::second, inside<{0, 10}>>{7});
    EXPECT_EQ(to_string(d), "0.021 km");
    // A plain factor, rounded once onto the cent.
    EXPECT_EQ(to_string(mul_into<usd_t>(just<10> * HP, inside<{{0, 4}, per<16>}>{1.0625})), "10.63 HP");
    // A dimensionless quotient into a plain inside: 95.5 ms / 10 ms floors to 9.
    const quantity<ms, inside<{{0, 100}, per<10>}>> lat{rational{955, 10}};
    const auto bin = div_into<inside<{0, 9}, clamp | round_floor>>(lat, just<10> * ms);
    EXPECT_EQ(bin, 9);
    // A divisor grid holding 0 gives an expected.
    const auto q = div_into<usd_t>(just<10> * HP, inside<{0, 4}>{0});
    ASSERT_FALSE(q);
    EXPECT_EQ(q.error(), errc::division_by_zero);
    static_assert(!can_mul_into<usd_t, decltype(just<1> * si::metre), decltype(just<1>)>); // a length is no HP
}

TEST(UnitsAlgorithmsTest, folds_sum_and_parse) {
    const std::vector<usd_t> items{usd_t{1.5}, usd_t{2.25}};
    EXPECT_EQ(to_string(sum<usd_t>(items)), "3.75 HP");
    const std::array<quantity<ms, inside<{0, 1000}>>, 2> waits{quantity<ms, inside<{0, 1000}>>{250},
                                                               quantity<ms, inside<{0, 1000}>>{1000}};
    EXPECT_EQ(to_string(sum<quantity<si::second, inside<{{0, 10}, per<1000>}>>>(waits)), "1.250 s");
    EXPECT_EQ(to_string(add_all(just<1> * HP, just<2> * HP, just<3> * HP)), "6 HP");
    EXPECT_EQ(to_string(mul_all(just<2> * si::metre, just<3> * si::metre, just<1, 2>)), "3 m²");

    EXPECT_EQ(to_string(*parse<usd_t>("12.34")), "12.34 HP");
    EXPECT_EQ(parse<usd_t>("12.3x").error(), errc::invalid_format);
    EXPECT_EQ(parse_exact<usd_t>("12.345").error(), errc::rounding_error);
}

TEST(UnitsAlgorithmsTest, predicates_and_casts) {
    EXPECT_TRUE(conversion_overflows<usd_t>(2000.0));
    EXPECT_FALSE(conversion_overflows<usd_t>(20.0));
    EXPECT_TRUE(conversion_rounds<usd_t>(0.125));
    EXPECT_TRUE(conversion_loses<usd_t>(-1));

    EXPECT_EQ(to_string(usd_t{5000, clamp}), "1000.00 HP");
    EXPECT_EQ(to_string(seconds_t{130, wrap}), "10 s");
    EXPECT_EQ(to_string(seconds_t{just<3> * si::minute + just<5> * si::second, wrap}), "5 s");
    static_assert(!can_wrap_cast<seconds_t, decltype(just<1> * si::metre)>);
}

TEST(UnitsAlgorithmsTest, dot_cross_lerp) {
    const auto x = just<3> * si::metre;
    const auto y = just<4> * si::metre;
    EXPECT_EQ(to_string(dot(x, y, x, y)), "25 m²");
    EXPECT_EQ(to_string(cross(x, y, y, -x)), "-25 m²");
    EXPECT_EQ(to_string(lerp(x, just<5> * si::metre, just<1, 2>)), "4 m");
}

TEST(UnitsAlgorithmsTest, trig_of_a_pi_scaled_angle_reduces_exactly) {
    using amp   = inside<{{-1, 1}, per<16384>}, round_nearest>;
    using floor = inside<{{-1, 1}, per<16384>}, round_floor>;
    using deg   = quantity<si::degree, inside<{-1000, 1000}>>;
    // Niven's values come out exact under any rounding.
    EXPECT_EQ(units::sin_into<floor>(deg{30}), (just<1, 2>));
    EXPECT_EQ(units::cos_into<floor>(deg{120}), (just<-1, 2>));
    EXPECT_EQ(units::sin_into<floor>(deg{-90}), just<-1>);
    EXPECT_EQ(units::cos_into<floor>(deg{750}), units::cos_into<floor>(deg{30}));
    EXPECT_EQ(units::sin_into<amp>(deg{180}), 0);
    // cos 45° = 0.70710678…: 11585/16384 nearest, and floor alike.
    EXPECT_EQ(units::cos_into<amp>(deg{45}), (just<11585, 16384>));
    EXPECT_EQ(units::cos_into<floor>(deg{45}), (just<11585, 16384>));
    // A quarter turn in slots of 2π/65536 rad.
    const quantity<slot, inside<{0, 65535}, wrap>> p{16384};
    EXPECT_EQ(units::sin_into<amp>(p), 1);
    EXPECT_EQ(units::cos_into<amp>(p), 0);
}

TEST(UnitsAlgorithmsTest, divmod_in_the_common_unit_and_percent) {
    const quantity<si::second, inside<{0, 86399}>> s{3725};
    const auto [h, rest] = divmod(s, just<1> * si::hour);
    EXPECT_EQ(h, 1);
    EXPECT_EQ(to_string(rest), "125 s");
    const auto [q, r] =
        divmod(quantity<si::day, inside<{0, 100}, round_toward_zero>>{17}, just<1, 1> * si::day * just<7>);
    EXPECT_EQ(q, 2);
    EXPECT_EQ(to_string(r), "3 d");

    EXPECT_EQ(to_string(just<8> * percent), "8 %");
    const inside<{{0, 1}, per<100>}> x = just<8> * percent;
    EXPECT_EQ(x, (just<2, 25>));
    EXPECT_EQ(to_string(mul_into<usd_t>(just<45> * HP, just<8> * percent)), "3.60 HP");
    EXPECT_EQ(unit_symbol(per_mille, text_encoding::ascii), "permille");
}

TEST(UnitsAlgorithmsTest, math_of_a_dimensionless_quantity_and_lifted_chains) {
    constexpr auto ratio = just<-6> * si::metre / (just<20> * si::metre); // −0.3
    using gain_t         = inside<{{0x1p-8, 4}, per<65536>}, round_nearest>;
    using decade_t       = inside<{{-1, 1}, per<1024>}, round_nearest>;
    const auto g         = units::pow_base_into<gain_t, 10>(ratio);
    EXPECT_EQ(g, (gain_t{rational{32846, 65536}}));
    const auto d = units::log10_into<decade_t>(just<1> * si::metre / (just<1> * si::metre));
    EXPECT_EQ(d, 0);
    // An expected quantity chains through +, *, as an expected inside does.
    const auto chained = just<1> * si::volt / inside<{0, 4}>{0} * just<2> + just<1> * si::volt;
    ASSERT_FALSE(chained);
    EXPECT_EQ(chained.error(), errc::division_by_zero);
    const auto fine = just<1> * si::volt / inside<{0, 4}>{2} * just<2> + just<1> * si::volt;
    ASSERT_TRUE(fine);
    EXPECT_EQ(to_string(*fine), "2 V");
}
} // namespace units_algorithms_tests

// units/math.hpp: pi conversions and math on quantities.
namespace units_math_tests {
using namespace beman::inside;
using namespace beman::inside::units;

namespace {
using rad_t = inside<{{-4, 4}, per<1'000'000>}, round_nearest>;
using deg_t = inside<{{-360, 360}, per<1'000'000>}, round_nearest>;
} // namespace

TEST(UnitsMathTest, degree_to_radian_rounds_once) {
    const quantity<si::degree, inside<{0, 360}>> a{90};
    const auto                                   r = value_cast<si::radian, rad_t>(a);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(r.numerical_value())>, rad_t>);
    EXPECT_EQ(rational{r.numerical_value()}, rational(1570796, 1'000'000)); // π/2 = 1.5707963…
    const auto n = value_cast<si::radian, rad_t>(quantity<si::degree, inside<{-360, 0}>>{-90});
    EXPECT_EQ(rational{n.numerical_value()}, rational(-1570796, 1'000'000));
    // And back: 1 rad = 57.2957795…°.
    const auto d = value_cast<si::degree, deg_t>(quantity<si::radian, inside<{0, 2}>>{1});
    EXPECT_EQ(rational{d.numerical_value()}, rational(57295780, 1'000'000));
    // 0 is exact.
    EXPECT_EQ((value_cast<si::radian, rad_t>(quantity<si::degree, inside<{0, 360}>>{0}).numerical_value()), 0);
    // A rational factor stays exact: arcminute → degree.
    const auto m = value_cast<si::degree, inside<{{0, 1}, per<60>}>>(just<30> * si::arcminute);
    EXPECT_EQ(rational{m.numerical_value()}, rational(1, 2));
}

TEST(UnitsMathTest, roots_take_the_units_root) {
    using area_t = quantity<isq::area[square(si::metre)], inside<{{0, 100}, per<1000>}, round_nearest>>;
    const auto s = units::sqrt(area_t{2});
    static_assert(std::remove_cvref_t<decltype(s)>::unit == si::metre.value);
    static_assert(std::remove_cvref_t<decltype(s)>::reference.Spec->Name == "length");
    EXPECT_EQ(rational{s.numerical_value()}, rational(1414, 1000));
    const auto c = units::cbrt(quantity<cubic(si::metre), inside<{{0, 1000}, per<100>}, round_nearest>>{27});
    EXPECT_EQ(c.numerical_value(), 3);
    EXPECT_EQ(to_string(c), "3.00 m"); // a decimal notch fixes the digits
}

TEST(UnitsMathTest, trigonometry_in_radians) {
    const quantity<si::radian, rad_t> a{rational(1, 2)};
    EXPECT_EQ(rational{units::sin(a)}, rational(479426, 1'000'000)); // sin 0.5 = 0.4794255…
    const auto t = units::atan(inside<{{-1, 1}, per<1000>}, round_nearest>{1});
    EXPECT_EQ(to_string(t), "0.785 rad");
    const auto y = quantity<si::metre, inside<{{0, 10}, per<1000>}, round_nearest>>{3};
    const auto x = quantity<si::kilo(si::metre), inside<{{0, 10}, per<1000>}, round_nearest>>{rational(4, 1000)};
    EXPECT_EQ(rational{units::atan2(y, x).numerical_value()}, rational(644, 1000)); // atan(3/4) = 0.6435…
    EXPECT_EQ(to_string(units::hypot(y, x)), "5.000 m");
}

TEST(UnitsMathTest, number_functions_keep_the_unit) {
    const quantity<si::metre, inside<{{-10, 10}, per<4>}, round_nearest>> d{rational(-11, 4)};
    EXPECT_EQ(to_string(units::abs(d)), "2.75 m");
    EXPECT_EQ(units::floor(d).numerical_value(), -3);
    EXPECT_EQ(units::ceil(d).numerical_value(), -2);
    EXPECT_EQ(units::round(d).numerical_value(), -3);
    EXPECT_EQ(units::trunc(d).numerical_value(), -2);
    EXPECT_EQ(to_string(units::pow<2>(d)), "7.5625 m²");
}

TEST(UnitsMathTest, pi_magnitudes_round_once_in_value_cast) {
    // π · 2 s → 6.283185 s: π is part of the unit until the one rounding.
    const quantity<si::second, inside<{0, 10}>> t{2};
    const auto s = value_cast<si::second, inside<{{0, 100}, per<1'000'000>}, round_nearest>>(mag_pi * t);
    EXPECT_EQ(rational{s.numerical_value()}, rational(6283185, 1'000'000));
    const auto h = value_cast<si::hour, inside<{{0, 1}, per<1'000'000'000>}, round_nearest>>(mag_pi * t);
    EXPECT_EQ(rational{h.numerical_value()}, rational(1745329, 1'000'000'000)); // 2π/3600 = 0.00174532925…
}

TEST(UnitsMathTest, into_forms_take_checked_inputs) {
    using out_t = inside<{{0, 100}, per<1000>}, round_nearest>;
    // An arithmetic result is checked: the deduced sqrt refuses it, sqrt_into rounds onto Out.
    const quantity<square(si::metre), inside<{0, 50}>> a{2};
    const auto                                         s = units::sqrt_into<out_t>(a + a);
    static_assert(std::remove_cvref_t<decltype(s)>::unit == si::metre.value);
    EXPECT_EQ(to_string(s), "2.000 m");
    EXPECT_EQ(units::cbrt_into<out_t>(quantity<cubic(si::metre), inside<{0, 100}>>{27}).numerical_value(), 3);
    // (A grid that reaches below 0 is a compile error: fail/sqrt_domain.)

    using unit_t = inside<{{-1, 1}, per<1'000'000>}, round_nearest>;
    const quantity<si::radian, inside<{0, 3}>> one_rad{1};
    EXPECT_EQ(rational{units::sin_into<unit_t>(one_rad)}, rational(841471, 1'000'000));
    EXPECT_EQ(rational{units::cos_into<unit_t>(one_rad)}, rational(540302, 1'000'000));
    using rad_out = inside<{{-4, 4}, per<1000>}, round_nearest>;
    EXPECT_EQ(to_string(units::atan_into<rad_out>(inside<{0, 2}>{1})), "0.785 rad");
    // A dimensionless quantity is a number too: asin of 1/2 = π/6.
    const auto half = quantity<si::metre, inside<{0, 5}>>{3} / quantity<si::metre, inside<{5, 10}>>{6}; // in [0, 1]
    EXPECT_EQ(to_string(units::asin_into<rad_out>(half)), "0.524 rad");
    const auto y = quantity<si::metre, inside<{0, 10}>>{3};
    const auto x = quantity<si::metre, inside<{0, 10}>>{4};
    EXPECT_EQ(to_string(units::hypot_into<out_t>(x, y)), "5.000 m");
    EXPECT_EQ(to_string(units::atan2_into<rad_out>(y, x)), "0.644 rad");
}

// The rounding mode on the deduced call works on quantities too.
TEST(UnitsMathTest, mode_on_the_deduced_call) {
    using area_t = quantity<square(si::metre), inside<{{0, 100}, per<1024>}>>; // no rounding policy
    static_assert(units::sqrt<round_floor>(area_t{2}).numerical_value() == 1.4140625_ins);
    static_assert(units::sin<round_nearest>(quantity<si::radian, inside<{{0, 4}, per<1024>}>>{1}) > 0);
    static_assert(units::atan<round_nearest>(inside<{{0, 1}, per<64>}>{1}).numerical_value() > 0);
}
} // namespace units_math_tests

// math/constants.hpp and units/constants.hpp: mathematical constants (math::) and physical constants as
// quantities (units::constants::), exact where the SI defines them.
namespace constants_tests {
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
    const W        n{r.Num};
    const W        d{static_cast<std::uint64_t>(r.Den)};
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
    EXPECT_EQ(
        std::format("{:.5e}", pc::gravitational_constant.in(cubic(si::metre) / (si::kilogram * square(si::second)))),
        "6.67430e-11 m³/(kg s²)");
    EXPECT_EQ(std::format("{:.11e}", pc::vacuum_magnetic_permeability.in(si::newton / square(si::ampere))),
              "1.25663706127e-06 N/A²");
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
} // namespace constants_tests
