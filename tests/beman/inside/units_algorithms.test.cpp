// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// inside's algorithms on quantities: policy hooks, ++/--, try_make,
// mul_into/div_into, add_all/mul_all/sum, parse, the conversion predicates,
// clamp_cast/wrap_cast, dot/cross/lerp, and sin_into/cos_into of an angle in
// a unit with a factor of π.

#include <beman/inside/units.hpp>
#include <beman/inside/units/io.hpp>
#include <beman/inside/units/math.hpp>

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <vector>

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
concept can_wrap_cast = requires(const X& x) { wrap_cast<Q>(x); };
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

    EXPECT_EQ(to_string(clamp_cast<usd_t>(5000)), "1000.00 HP");
    EXPECT_EQ(to_string(wrap_cast<seconds_t>(130)), "10 s");
    EXPECT_EQ(to_string(wrap_cast<seconds_t>(just<3> * si::minute + just<5> * si::second)), "5 s");
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
    const auto [q, r] = divmod(quantity<si::day, inside<{0, 100}, snap>>{17}, just<1, 1> * si::day * just<7>);
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
