// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The units API review before 0.4: identity of specs and units, honest
// constraints with reasons, spec queries, value_cast to a quantity type,
// unit · magnitude, canonical common units, text, points.

#include <beman/inside/units.hpp>
#include <beman/inside/units_math.hpp>

#include <gtest/gtest.h>

#include <type_traits>

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

template <class A, class B>
concept can_add = requires(const A& a, const B& b) { a + b; };
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
