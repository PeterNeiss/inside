// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// The default value: a default-constructed or value-initialized inside is 0
// when its grid holds 0, else Lower — a value of the grid on every storage.
// `uninitialized` keeps the trivial default ctor (Raw uninitialized), and a
// failed error-code construction keeps the default value.

#include <beman/inside/inside.hpp>
#include <beman/inside/units.hpp>

#include <gtest/gtest.h>

#include <type_traits>
#include <utility>

using namespace beman::inside;

namespace {
// The value of T{} and of a default-initialized T, both in a constant
// expression (reading an uninitialized raw there would not compile).
template <class T>
constexpr rational value_init() {
    const T t{};
    return rational{t};
}
template <class T>
constexpr rational default_init() {
    T t;
    return rational{t};
}
template <class T>
constexpr bool defaults_to(const rational& v) {
    return value_init<T>() == v && default_init<T>() == v;
}
} // namespace

TEST(DefaultValue, zero_when_the_grid_holds_it_else_lower) {
    static_assert(defaults_to<inside<{0, 100}>>(0));                                    // index from 0
    static_assert(defaults_to<inside<{-100, 100}>>(0));                                 // signed value
    static_assert(defaults_to<inside<{5, 100}>>(5));                                    // value raw, no 0
    static_assert(defaults_to<inside<{200, 300}>>(200));                                // index, no 0
    static_assert(defaults_to<inside<{-10, -5}>>(-10));                                 // below 0
    static_assert(defaults_to<inside<{{-1, 1}, per<16384>}, round_nearest>>(0));        // Q1.14: not Lower
    static_assert(defaults_to<inside<{{0.5, 10.5}, 1}>>(rational{1u, 2}));              // unanchored
    static_assert(defaults_to<inside<{{-1, 1}, max_denominator{8}}>>(0));               // continuous
    static_assert(defaults_to<inside<{{1, 2}, max_denominator{100}}>>(1));              // continuous, no 0
    static_assert(defaults_to<inside<grid{7}>>(7));                                     // point
    static_assert(defaults_to<inside<{0, 100}, clamp | round_nearest | unsafe>>(0));    // any policy
    static_assert(value_init<inside<{{1, 2}, max_denominator{100}}>>().Den != 0);
}

TEST(DefaultValue, wide_storage) {
    // a wide index (more than 2^64 slots)
    static_assert(inside<{0, 0x1p100}>{} == 0_ins);
    static_assert(inside<{0x1p100, 0x1p101}>{} == just<0x1p100>);
    static_assert(inside<{-0x1p101, -0x1p100}>{} == -just<0x1p101>);
    // a fraction raw (a continuous grid past the 64-bit rational)
    using F0 = inside<{{-0x1p70, 0x1p70}, max_denominator{3}}>;
    using F1 = inside<{{1, 0x1p70}, max_denominator{3}}>;
    static_assert(detail::fraction_storage<F0> && detail::fraction_storage<F1>);
    static_assert(F0{} == 0_ins && F1{} == 1_ins);
}

TEST(DefaultValue, runtime_matches_compile_time) {
    inside<{5, 100}> a;
    inside<{-10, -5}> b{};
    inside<{{-1, 1}, per<16384>}, round_nearest> c;
    EXPECT_EQ(rational{a}, 5);
    EXPECT_EQ(rational{b}, -10);
    EXPECT_EQ(rational{c}, 0);
}

TEST(DefaultValue, failed_error_code_construction_keeps_the_default) {
    constexpr auto store = [] {
        errc             ec{};
        inside<{5, 100}> x(200, make_policy(ec));
        return std::pair{ec, rational{x}};
    };
    static_assert(store().first == errc::overflow && store().second == 5);
    errc             ec{};
    inside<{5, 100}> x(200, make_policy(ec));
    EXPECT_EQ(ec, errc::overflow);
    EXPECT_EQ(rational{x}, 5);
}

TEST(DefaultValue, uninitialized_keeps_a_trivial_default_ctor) {
    static_assert(std::is_trivially_default_constructible_v<inside<{5, 100}, uninitialized>>);
    static_assert(std::is_trivially_default_constructible_v<inside<{{1, 2}, max_denominator{100}}, uninitialized>>);
    static_assert(!std::is_trivially_default_constructible_v<inside<{5, 100}>>);
    static_assert(!std::is_trivially_default_constructible_v<inside<{5, 100}, unsafe>>);
    // a value still stores normally; a result takes the default policy, as
    // for every other flag
    using U = inside<{0, 10}, uninitialized>;
    static_assert(U{4} == 4);
    static_assert(!has_flag(flags_of<decltype(U{1} + U{2})>, uninitialized));
    // a cursor starts at Lower whatever T's flags
    static_assert(rational{cursor<inside<{5, 100}, uninitialized>>{}} == 5);
}

TEST(DefaultValue, quantity) {
    using namespace beman::inside::units;
    static_assert(quantity<si::metre, inside<{5, 100}>>{}.numerical_value() == 5_ins);
    static_assert(quantity<si::metre, inside<{-5, 100}>>{}.numerical_value() == 0_ins);
    static_assert(std::is_trivially_default_constructible_v<quantity<si::metre, inside<{5, 100}, uninitialized>>>);
}
