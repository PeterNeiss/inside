// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// units.hpp: dimensions, units, conversions, quantity kinds, arithmetic, text.

#include <beman/inside/units.hpp>

#include <gtest/gtest.h>

#include <sstream>
#include <type_traits>

using namespace beman::inside;
using namespace beman::inside::units;

namespace {
constexpr auto km = si::kilo(si::metre);
constexpr auto mm = si::milli(si::metre);

template <class A, class B>
constexpr bool addable =
    units::detail::common_spec(std::remove_cvref_t<A>::reference, std::remove_cvref_t<B>::reference).Ok;
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
    EXPECT_EQ(to_string(just<2> / si::second), "2 1/s");
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
    static_assert(std::remove_cvref_t<decltype(s)>::reference.Spec == &isq_defs::length);
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
    static_assert(!addable<decltype(hz), decltype(bq)>);
    static_assert(!addable<decltype(rad), decltype(one_)>);
    static_assert(addable<decltype(hz), decltype(just<1> / si::second)>);
    static_assert(
        !std::is_convertible_v<quantity<si::hertz, inside<{0, 10}>>, quantity<si::becquerel, inside<{0, 10}>>>);
    // Gy and Sv: both J/kg, different kinds.
    static_assert(!addable<decltype(just<1> * si::gray), decltype(just<1> * si::sievert)>);
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

#if BEMAN_INSIDE_BIG_GRIDS
TEST(UnitsTest, big_grid_prefixes_and_units) {
    static_assert(magnitude_of(si::yocto(si::gram).value) == magnitude{1e-27_g});
    const auto e = (just<1> * si::electronvolt).in(si::joule);
    EXPECT_EQ(to_string(e), "0.0000000000000000001602176634 J");
    // E = h·ν, exactly: one photon of the caesium frequency.
    const auto en =
        (just<1> * si2019::planck_constant) * (just<1> * si2019::hyperfine_structure_transition_frequency_of_cs);
    EXPECT_EQ(to_string(en.in(si::joule)), "0.00000000000000000000000609110229711386655 J");
}
#endif

TEST(UnitsTest, points_shift_their_grid_between_origins) {
    const auto t = point<si::degree_Celsius>(inside<{-40, 60}>{20});
    static_assert(std::remove_cvref_t<decltype(t)>::origin == &si_defs::ice_point);
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
    static_assert(std::remove_cvref_t<decltype(x)>::origin == nullptr);
    EXPECT_EQ((x - point<si::metre>(just<1>)).numerical_value(), 2);
}

#ifdef __cpp_lib_format
TEST(UnitsTest, std_format) {
    EXPECT_EQ(std::format("{}", just<5> * si::kilo(si::metre)), "5 km");
    EXPECT_EQ(std::format("{:>4}", just<5> * si::newton), "   5 N");
    EXPECT_EQ(std::format("{}", just<90> * si::degree), "90°");
    const quantity<si::metre / si::second, inside<{{0, 10}, per<4>}>> v{rational(9, 4)};
    EXPECT_EQ(std::format("{:.1f}", v), "2.2 m/s");
    EXPECT_EQ(std::format("{}", point<si::degree_Celsius>(inside<{-40, 60}>{20})), "20 ℃");
    EXPECT_EQ(std::format("{}", just<3> * units::one), "3");
}
#endif
