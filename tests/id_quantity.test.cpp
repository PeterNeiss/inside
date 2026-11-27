// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Quantities spelled as text: id<"[0..100] [km/h]">, the unit read by its symbol among the units visible where the
// type is named, the canonical text of the unit, the types the operators give, and "42 [1..100) [km/h]"_ins.

#include <inside/inside.hpp>
#include <inside/units.hpp>
#include <inside/units/constants.hpp>
#include <inside/units/io.hpp>

#include <gtest/gtest.h>

#include <string_view>
#include <type_traits>

namespace id_quantity_tests {
using namespace ins;

inline constexpr auto cts = base_unit<"cts", "count">;
constexpr auto        km  = si::kilo(si::metre);
using I                   = inside<{0, 100}>;

// The text a type's key holds.
template <class T>
struct text_of_t;
template <key K>
struct text_of_t<id<K>> {
    static constexpr std::string_view value = K.text();
};
template <class T>
constexpr std::string_view text_of = text_of_t<T>::value;

TEST(IdQuantityTest, a_unit_with_a_text_names_the_type) {
    static_assert(std::is_same_v<quantity<si::metre, I>, id<"[0..100] [m]">>);
    static_assert(std::is_same_v<quantity<km / si::hour, I>, id<"[0..100] [km/h]">>);
    static_assert(std::is_same_v<quantity<si::newton, I>, id<"[0..100] [N]">>);
    static_assert(std::is_same_v<quantity<si::micro(si::second), I>, id<"[0..100] [us]">>);
    static_assert(std::is_same_v<quantity<cts, I>, id<"[0..100] [cts]">>);
    static_assert(std::is_same_v<quantity<cts / si::volt, I>, id<"[0..100] [cts/V]">>);
    static_assert(std::is_same_v<quantity<si::kilogram * si::metre / (si::second * si::second), I>,
                                 id<"[0..100] [kg*m/s^2]">>);
    static_assert(std::is_same_v<quantity<si::metre, inside<{0, 360, bounds::half_open}, wrap>>,
                                 id<"[0..360) [m] wrap">>);
    static_assert(std::is_same_v<quantity<one, I>, id<"[0..100] [1]">>);
}

// A symbol is looked up in the catalog first (its units' own symbols and the prefixed units in wide use): a symbol
// the catalog reads names its unit, so one's own units need symbols of their own
// (tests/fail/id_unit_symbol_of_the_catalog.test.cpp).
TEST(IdQuantityTest, a_symbol_the_catalog_reads_names_the_catalog_unit) {
    static_assert(id<"[0..100] [km]">::unit == km.value);
}

// Every unit of the catalog's tables (the constants' too) reads back from its text: no two share a symbol.
// The symbol of the first that does not, else nothing.
struct round_trip_context {};
consteval std::string_view catalog_unit_without_its_text() {
    const detail::catalog_list c{detail::catalog_table<round_trip_context>()};
    for (std::size_t t = 0; t < c.Size; ++t)
        for (std::size_t i = 0; i < c.Sets[t].Units.Size; ++i) {
            const unit_atom          a{c.Sets[t].Units[i]};
            const unit_value         u{a};
            const auto               n{detail::quantity_name_for(reference_of(u), &origin_def::none, false)};
            if (!n.Ok)
                return a.U->Utf8;
            const auto q{detail::read_quantity<round_trip_context>(n.Name.Unit.view(), n.Name.Kind.view(), {})};
            if (!q.Ok || !(q.Ref == reference_of(u)))
                return a.U->Utf8;
        }
    return {};
}

TEST(IdQuantityTest, every_unit_of_the_catalog_reads_back) {
    static_assert(detail::catalog_table<round_trip_context>().Size == 3); // the catalog, si2019, the constants
    constexpr std::string_view s{catalog_unit_without_its_text()};
    static_assert(s.empty(), s);
    // A plain symbol with an ASCII one of its own reads only as that: k is free, k_B is Boltzmann's.
    static_assert(id<"[0..9] [k_B]">::unit == si2019::boltzmann_constant.value);
    static_assert(id<"[0..9] [c_0]">::unit == si2019::speed_of_light_in_vacuum.value);
    static_assert(id<"[0..9] [h_P]">::unit == si2019::planck_constant.value);
    static_assert(id<"[0..9] [R]">::unit == si2019::molar_gas_constant.value);
}

TEST(IdQuantityTest, every_spelling_of_a_unit_is_one_type) {
    static_assert(text_of<id<"[0..100] [km*h^-1]">> == "[0..100] [km/h]");
    static_assert(text_of<id<"[0..100] [km h⁻¹]">> == "[0..100] [km/h]");
    static_assert(text_of<id<"[0..100] [km·h⁻¹]">> == "[0..100] [km/h]");
    static_assert(text_of<id<"[0..100] [µs]">> == "[0..100] [us]");
    static_assert(text_of<id<"[0..100] [kg·m²/s²]">> == "[0..100] [kg*m^2/s^2]");
    static_assert(text_of<id<"[0..100] [1/s]">> == "[0..100] [s^-1]");
    static_assert(text_of<id<"[0..100] [[1/381 ft]]">> == "[0..100] [[1/381*ft]]");
    static_assert(text_of<id<"[0..10) [m] clamp">> == "[0..9] [m] clamp");
}

TEST(IdQuantityTest, the_types_hold_their_unit_and_number) {
    using Q = id<"[0..100] [km/h]">;
    static_assert(quantity_like<Q> && !insidable<Q>);
    static_assert(Q::unit == (km / si::hour).value);
    static_assert(std::is_same_v<Q::rep, id<"[0..100]">>);
    constexpr Q v{42, km / si::hour};
    static_assert(v == quantity<km / si::hour, I>{42, km / si::hour});
    EXPECT_EQ(to_string(v), "42 km/h");
}

TEST(IdQuantityTest, results_read_as_ids_where_their_reference_is_a_unit) {
    using metres = id<"[1..9] [m]">;
    static_assert(std::is_same_v<decltype(metres{} + metres{}), id<"[2..18] [m]">>);
    static_assert(std::is_same_v<decltype(metres{} - metres{}), id<"[-8..8] [m]">>);
    static_assert(std::is_same_v<decltype(-metres{}), id<"[-9..-1] [m]">>);
    static_assert(std::is_same_v<decltype(metres{} * just<2>), id<"[2..18] 2 [m]">>);            // the even ones
    // A sum in two units takes their common unit.
    static_assert(std::is_same_v<decltype(id<"[0..1] [km]">{} + id<"[0..1] [m]">{}), id<"[0..1001] [m]">>);
    static_assert(std::is_same_v<decltype(id<"[0..1] [km]">{}.in(si::metre)), id<"[0..1000] 1000 [m]">>);
    // A product keeps the kinds it is made of: any length times any duration, in its text after the unit.
    using P = decltype(metres{} * id<"[1..9] [s]">{});
    static_assert(std::is_same_v<P, id<"[1..81] [m*s]{any duration*length}">>);
    static_assert(P::unit == (si::metre * si::second).value);
    // A kind is in the text too.
    static_assert(std::is_same_v<quantity<isq::height[si::metre], I>, id<"[0..100] [m]{height}">>);
}

TEST(IdQuantityTest, members_and_hooks_take_the_type_itself) {
    using metres = id<"[0..9] [m] wrap">;
    metres m{8, si::metre};
    int    carries{0};
    m.on_wrap([&](metres& q, auto) {
        carries += 1;
        static_assert(std::is_same_v<decltype(q), metres&>);
    }) += just<3> * si::metre;
    EXPECT_EQ(carries, 1);
    EXPECT_EQ(m, just<1> * si::metre);
    const auto old{m++};
    static_assert(std::is_same_v<std::remove_const_t<decltype(old)>, metres>);
}

// A kind after the unit, in braces, when it is not the unit's own; a point's origin after @.
TEST(IdQuantityTest, kinds_and_origins_in_the_text) {
    // An exact kind, as kind[unit] names it.
    static_assert(std::is_same_v<quantity<isq::height[si::metre], I>, id<"[0..100] [m]{height}">>);
    static_assert(id<"[0..100] [m]{height}">::reference == reference{isq::height[si::metre]});
    static_assert(text_of<id<"[0..100] [m]{ height }">> == "[0..100] [m]{height}");
    // The unit's own kind, any of it: no braces.
    static_assert(text_of<id<"[0..100] [m]{any length}">> == "[0..100] [m]");
    // A product keeps its kinds: any length times any duration, or exact ones.
    using metres = id<"[1..9] [m]">;
    static_assert(std::is_same_v<decltype(metres{} * id<"[1..9] [s]">{}), id<"[1..81] [m*s]{any duration*length}">>);
    using H = quantity<isq::height[si::metre], inside<{1, 9}>>;
    using W = quantity<isq::width[si::metre], inside<{1, 9}>>;
    static_assert(text_of<decltype(H{} * W{})> == "[1..81] [m^2]{height*width}");
    static_assert(text_of<decltype(H{} / id<"[1..9] [s]">{})> == "[1/9..9] <9> [m/s]{height/duration}");
    // No kind.
    static_assert(text_of<id<"[0..9] [J]{none}">> == "[0..9] [J]{none}");
    static_assert(id<"[0..9] [J]{none}">::reference == reference{&kind_def::none, false, {}, si::joule.value});
    // A point's origin.
    static_assert(key{"[-40..125] [`C] @ice_point"}.text() == "[-40..125] [`C] @ice_point");
    static_assert(key{"[-40..125] [℃]  @ ice_point clamp"}.text() == "[-40..125] [`C] @ice_point clamp");
    static_assert(key{"[0..400] [K] @absolute_zero"}.text() == "[0..400] [K] @absolute_zero");
    static_assert(key{"[0..9] [m] @0"}.text() == "[0..9] [m] @0");
    // What does not read.
    static_assert(!key{"[0..9] [m]{mass}"}.ok());          // another dimension
    static_assert(!key{"[0..9] [m]{nothing}"}.ok());       // no such kind
    static_assert(!key{"[0..9] [m] @nowhere"}.ok());       // no such origin
    static_assert(!key{"[0..9] [m] @ice_point"}.ok());     // an origin of another dimension
}

// A point is an id too: its origin after the unit, @0 for the implicit zero of the dimension.
TEST(IdQuantityTest, points_are_ids) {
    using C = id<"[-40..125] [`C] @ice_point">;
    static_assert(point_like<C> && !quantity_like<C> && !insidable<C>);
    static_assert(std::is_same_v<quantity_point<si::degree_Celsius, si::ice_point.value, inside<{-40, 125}>>, C>);
    static_assert(std::is_same_v<decltype(point<si::degree_Celsius>(inside<{-40, 125}>{20})), C>);
    static_assert(std::is_same_v<C::quantity_type, id<"[-40..125] [`C]">>);
    static_assert(text_of<decltype(si::absolute_zero + just<300> * si::kelvin)> == "[300] [K] @absolute_zero");
    static_assert(text_of<decltype(point<si::metre>(inside<{0, 9}>{3}))> == "[0..9] [m] @0");
    // Shifts, differences and comparisons keep their types as texts.
    constexpr C t{20, si::degree_Celsius};
    static_assert(text_of<decltype(t - t)> == "[-165..165] [`C]");
    static_assert(text_of<decltype(t + just<5> * si::kelvin)> == "[-35..130] [`C] @ice_point");
    static_assert(t.point_in(si::kelvin) == si::absolute_zero + just<29315, 100> * si::kelvin);
    EXPECT_EQ(to_string(t), "20 ℃");
}

// "21.5 [-40..125] 1/10 [`C] @ice_point"_ins: a point with its type.
TEST(IdQuantityTest, a_literal_with_an_origin) {
    constexpr auto p{"21.5 [-40..125] 1/10 [`C] @ice_point"_ins};
    static_assert(std::is_same_v<std::remove_const_t<decltype(p)>, id<"[-40..125] 1/10 [`C] @ice_point">>);
    static_assert(p == point<si::degree_Celsius>(just<43, 2>));
    EXPECT_EQ(to_string(p), "21.5 ℃");
}

// "42 [1..100) [km/h]"_ins: a value with its quantity type, the number in the type's unit.
TEST(IdQuantityTest, a_literal_with_a_unit) {
    constexpr auto v{"42 [1..100) [km/h]"_ins};
    static_assert(std::is_same_v<std::remove_const_t<decltype(v)>, id<"[1..99] [km/h]">>);
    static_assert(v == quantity<km / si::hour, inside<{1, 99}>>{42, km / si::hour});
    constexpr auto p{"42 [km/h]"_ins}; // a unit alone: the point
    static_assert(std::is_same_v<std::remove_const_t<decltype(p)>, id<"[42] [km/h]">>);
    static_assert("200 [0..99] [m] clamp"_ins == just<99> * si::metre);
    static_assert("0.5 [0..1] 1/4 [s]"_ins == just<1, 2> * si::second);
    static_assert("7 [0..4095] [cts]"_ins == just<7> * cts);
    EXPECT_EQ(to_string("42 [1..100) [km/h]"_ins), "42 km/h");
}
} // namespace id_quantity_tests
