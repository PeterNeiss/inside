// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// std::chrono at the boundary: from_duration / to_duration / period_notch.

#include <beman/inside/chrono.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <limits>
#include <type_traits>

using namespace beman::inside;
using namespace std::chrono;

namespace {
using sec_t  = inside<{0, 86400, bounds::half_open}>; // a time of day in seconds
using msec_t = inside<{{0, 86400}, per<1000>}>;       // … to the millisecond
} // namespace

TEST(ChronoTest, period_notch_is_the_period_in_seconds) {
    static_assert(period_notch<std::milli> == per<1000>);
    static_assert(period_notch<std::ratio<60>> == rational{60});
    static_assert(period_notch<std::ratio<1>> == rational{1});
}

TEST(ChronoTest, from_duration_is_exact) {
    EXPECT_EQ(rational{*from_duration<msec_t>(milliseconds{1500})}, rational(3, 2));
    EXPECT_EQ(*from_duration<sec_t>(minutes{2}), 120);
    EXPECT_EQ(*from_duration<sec_t>(seconds{0}), 0);
    static_assert(*from_duration<msec_t>(milliseconds{250}) == 0.25_ins);
}

TEST(ChronoTest, from_duration_follows_the_target_policy) {
    // Off the target's notch: an error, or rounded by a per-call flag.
    EXPECT_EQ(from_duration<sec_t>(milliseconds{1500}).error(), errc::rounding_error);
    EXPECT_EQ((*from_duration<sec_t, round_nearest>(milliseconds{1500})), 2);
    EXPECT_EQ((*from_duration<sec_t, round_floor>(milliseconds{1500})), 1);
    // Out of range: an overflow, or clamped / wrapped by the type's policy.
    EXPECT_EQ(from_duration<sec_t>(hours{25}).error(), errc::overflow);
    EXPECT_EQ((*from_duration<inside<{0, 86400, bounds::half_open}, wrap>>(hours{25})), 3600);
    EXPECT_EQ((*from_duration<inside<{0, 86400, bounds::half_open}, clamp>>(hours{25})), 86399);
    // count·num past the 64-bit rational
    EXPECT_EQ(from_duration<sec_t>(hours{std::numeric_limits<std::int64_t>::max()}).error(), errc::overflow);
}

TEST(ChronoTest, from_duration_takes_a_floating_count) {
    EXPECT_EQ(rational{*from_duration<msec_t>(duration<double>{0.25})}, rational(1, 4));
    EXPECT_EQ((rational{*from_duration<msec_t>(duration<double, std::milli>{1500.0})}), rational(3, 2));
    EXPECT_EQ(from_duration<msec_t>(duration<double, std::milli>{std::numeric_limits<double>::infinity()}).error(),
              errc::not_finite);
    EXPECT_EQ((from_duration<msec_t>(duration<double, std::milli>{1e300}).error()), errc::overflow);
}

TEST(ChronoTest, to_duration_is_plain_when_the_grid_proves_it) {
    auto s = to_duration<seconds>(sec_t{90});
    static_assert(std::is_same_v<decltype(s), seconds>);
    EXPECT_EQ(s.count(), 90);
    auto ms = to_duration<milliseconds>(msec_t{1.5});
    static_assert(std::is_same_v<decltype(ms), milliseconds>);
    EXPECT_EQ(ms.count(), 1500);
    static_assert(to_duration<minutes>(inside<{{0, 3600}, 60}>{120}).count() == 2);
    // A floating rep rounds once and never fails.
    auto third = to_duration<duration<double, std::milli>>(inside<{{0, 1}, per<3>}>{just<1>.per<3>});
    static_assert(std::is_same_v<decltype(third), duration<double, std::milli>>);
    EXPECT_EQ(third.count(), 1000.0 / 3);
}

TEST(ChronoTest, to_duration_reports_off_period_and_overflow) {
    auto c = to_duration<seconds>(msec_t{1.5});
    static_assert(std::is_same_v<decltype(c), std::expected<seconds, errc>>);
    EXPECT_EQ(c.error(), errc::rounding_error);
    EXPECT_EQ(to_duration<seconds>(msec_t{2}).value().count(), 2); // on a whole second
    auto d = to_duration<duration<std::int8_t>>(sec_t{200});
    static_assert(std::is_same_v<decltype(d), std::expected<duration<std::int8_t>, errc>>);
    EXPECT_EQ(d.error(), errc::overflow);
    EXPECT_EQ(to_duration<duration<std::int8_t>>(sec_t{100}).value().count(), 100);
}

TEST(ChronoTest, to_duration_rounds_by_a_per_call_mode) {
    // With a mode the count always exists, so the result is plain again.
    auto n = to_duration<seconds, round_nearest>(msec_t{1.5});
    static_assert(std::is_same_v<decltype(n), seconds>);
    EXPECT_EQ(n.count(), 2);
    EXPECT_EQ((to_duration<seconds, round_floor>(msec_t{1.5}).count()), 1);
    EXPECT_EQ((to_duration<seconds, round_ceil>(msec_t{1.25}).count()), 2);
    EXPECT_EQ((to_duration<seconds, round_half_even>(msec_t{2.5}).count()), 2);
    using signed_t = inside<{{-10, 10}, per<4>}>;
    EXPECT_EQ((to_duration<seconds, round_nearest>(signed_t{-1.5}).count()), -2); // a tie away from zero
    EXPECT_EQ((to_duration<seconds, round_floor>(signed_t{-1.25}).count()), -2);
}

TEST(ChronoTest, round_trips) {
    for (int ms : {0, 1, 999, 1000, 45'296'789, 86'399'999}) {
        const auto x = from_duration<msec_t>(milliseconds{ms});
        ASSERT_TRUE(x.has_value());
        EXPECT_EQ(to_duration<milliseconds>(*x).count(), ms);
    }
}
