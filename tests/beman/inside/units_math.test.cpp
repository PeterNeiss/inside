// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// units_math.hpp: pi conversions and math on quantities.

#include <beman/inside/units_math.hpp>

#include <gtest/gtest.h>

#include <type_traits>

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
    static_assert(std::remove_cvref_t<decltype(s)>::reference.Spec == &isq_defs::length);
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
